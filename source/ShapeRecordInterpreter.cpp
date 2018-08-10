#include <Eigen/Core>

#include <geos/geom.h>
#include <geos/opBuffer.h>
#include <geos/opDistance.h>

#include <Pita/Node.hpp>
#include <Pita/Context.hpp>
#include <Pita/Vectorizer.hpp>
#include <Pita/Printer.hpp>
#include <Pita/Evaluator.hpp>


namespace cgl
{
	class BoundingRectMaker : public gg::CoordinateFilter
	{
	public:
		BoundingRect rect;

		void filter_ro(const Coordinate* coord) override
		{
			rect.add(EigenVec2(coord->x, coord->y));
		}
	};

	BoundingRect BoundingRectRecordPacked(const PackedVal& value, std::shared_ptr<Context> pContext)
	{
		auto geometries = GeosFromRecordPacked(value, pContext);

		BoundingRectMaker maker;
		for (auto geometry : geometries)
		{
			geometry->apply_ro(&maker);
		}

		return maker.rect;
	}

	std::string getIndent(int depth)
	{
		std::string indent;
		const std::string indentStr = "  ";
		for (int i = 0; i < depth; ++i)
		{
			indent += indentStr;
		}
		return indent;
	}

	struct OutputPolygon
	{
		std::vector<gg::Geometry*> polygon;
		std::vector<gg::Geometry*> hole;
	};

	bool ReadPackedPolyData(const PackedList& polygons, std::vector<OutputPolygon>& outputPolygonDatas, const TransformPacked& transform)
	{
		const auto type = GetPackedListType(polygons);

		const auto readPolygon = [&](const std::vector<PackedList::Data>& vertices)->bool
		{
			gg::CoordinateArraySequence pts;

			for (const auto& vertexData : vertices)
			{
				if (!IsType<PackedRecord>(vertexData.value))
				{
					return false;
				}

				const auto& cooedinateData = As<PackedRecord>(vertexData.value).values;
				auto itX = cooedinateData.find("x");
				auto itY = cooedinateData.find("y");
				if (itX == cooedinateData.end() || itY == cooedinateData.end() || !IsNum(itX->second.value) || !IsNum(itY->second.value))
				{
					return false;
				}

				auto pos = transform.product(EigenVec2(AsDouble(itX->second.value), AsDouble(itY->second.value)));
				pts.add(gg::Coordinate(pos.x(), pos.y()));
			}

			if (!pts.empty())
			{
				pts.add(pts.front());

				auto factory = gg::GeometryFactory::create();
				gg::LinearRing* linearRing = factory->createLinearRing(pts);

				if (IsClockWise(linearRing))
				{
					outputPolygonDatas.emplace_back();
					auto& outputPolygons = outputPolygonDatas.back().polygon;
					outputPolygons.push_back(factory->createPolygon(linearRing, {}));
				}
				//穴がデータに追加されるのは、既存のポリゴンが存在するときのみ
				else if(!outputPolygonDatas.empty())
				{
					auto& outputHoles = outputPolygonDatas.back().hole;
					outputHoles.push_back(factory->createPolygon(linearRing, {}));
				}
			}

			return true;
		};

		if (type == PackedPolyDataType::POLYGON)
		{
			return readPolygon(polygons.data);
		}
		else if (type == PackedPolyDataType::MULTIPOLYGON)
		{
			for (const auto& polygonData : polygons.data)
			{
				const auto& currentPolygonData = polygonData.value;

				if (!IsType<PackedList>(currentPolygonData) || !readPolygon(As<PackedList>(currentPolygonData).data))
				{
					return false;
				}
			}

			return true;
		}

		//PackedPolyDataType::?
		return false;
	}

	bool ReadPackedLineData(const PackedList& lines, std::vector<gg::Geometry*>& outputLineDatas, const TransformPacked& transform)
	{
		const auto type = GetPackedListType(lines);

		const auto readLine = [&](const std::vector<PackedList::Data>& vertices)->bool
		{
			gg::CoordinateArraySequence pts;

			for (const auto& vertexData : vertices)
			{
				if (!IsType<PackedRecord>(vertexData.value))
				{
					return false;
				}

				const auto& cooedinateData = As<PackedRecord>(vertexData.value).values;
				auto itX = cooedinateData.find("x");
				auto itY = cooedinateData.find("y");
				if (itX == cooedinateData.end() || itY == cooedinateData.end() || !IsNum(itX->second.value) || !IsNum(itY->second.value))
				{
					return false;
				}

				auto pos = transform.product(EigenVec2(AsDouble(itX->second.value), AsDouble(itY->second.value)));
				pts.add(gg::Coordinate(pos.x(), pos.y()));
				
			}

			if (!pts.empty())
			{
				auto factory = gg::GeometryFactory::create();
				outputLineDatas.push_back(factory->createLineString(pts));
			}

			return true;
		};

		if (type == PackedPolyDataType::POLYGON)
		{
			return readLine(lines.data);
		}
		else if (type == PackedPolyDataType::MULTIPOLYGON)
		{
			for (const auto& lineData : lines.data)
			{
				const auto& currentLineData = lineData.value;

				if (!IsType<PackedList>(currentLineData) || !readLine(As<PackedList>(currentLineData).data))
				{
					return false;
				}
			}

			return true;
		}

		//PackedPolyDataType::?
		return false;
	}

	std::vector<gg::Geometry*> GeosFromListPacked(const cgl::PackedList& list, std::shared_ptr<Context> pContext, const cgl::TransformPacked& transform);

	std::vector<gg::Geometry*> GeosFromRecordPackedImpl(const cgl::PackedRecord& record, std::shared_ptr<Context> pContext, const cgl::TransformPacked& parent)
	{
		const cgl::TransformPacked current(record);

		const cgl::TransformPacked transform = parent * current;

		std::vector<gg::Geometry*> resultPolygons;
		std::vector<gg::Geometry*> currentLines;

		for (const auto& member : record.values)
		{
			const cgl::PackedVal& value = member.second.value;

			if (member.first == "polygon")
			{
				std::vector<OutputPolygon> outputPolygons;
				try
				{
					if (cgl::IsType<cgl::PackedList>(value))
					{
						if (!ReadPackedPolyData(cgl::As<cgl::PackedList>(value), outputPolygons, transform))
						{
							CGL_Error("polygonに指定されたデータの形式が不正です。");
						}
					}
					else if (cgl::IsType<cgl::FuncVal>(value))
					{
						pContext->switchFrontScope();

						Eval evaluator(pContext);
						LRValue result;
						try
						{
							result = evaluator.callFunction(LocationInfo(), cgl::As<cgl::FuncVal>(value), {});
						}
						catch (std::exception& e)
						{
							std::cout << "Packed Record Eval: " << e.what() << std::endl;
							throw;
						}
						const cgl::PackedVal evaluated = Packed(pContext->expand(result, LocationInfo()), *pContext);

						pContext->switchBackScope();

						if (!cgl::IsType<cgl::PackedList>(evaluated))
						{
							CGL_Error("polygon()の評価結果の型が不正です。");
						}

						if (!ReadPackedPolyData(cgl::As<cgl::PackedList>(evaluated), outputPolygons, transform))
						{
							CGL_Error("polygonに指定されたデータの形式が不正です。");
						}
					}
				}
				catch (std::exception& e)
				{
					std::cout << "Packed Record1: " << e.what() << std::endl;
					throw;
				}

				try
				{
					for (auto& polygonData : outputPolygons)
					{
						auto& currentPolygons = polygonData.polygon;
						const auto& currentHoles = polygonData.hole;

						if (!currentPolygons.empty())
						{
							if (!currentHoles.empty())
							{
								for (int s = 0; s < currentPolygons.size(); ++s)
								{
									gg::Geometry* erodeGeometry = currentPolygons[s];

									for (int d = 0; d < currentHoles.size(); ++d)
									{
										erodeGeometry = erodeGeometry->difference(currentHoles[d]);

										if (erodeGeometry->getGeometryTypeId() == geos::geom::GEOS_MULTIPOLYGON)
										{
											currentPolygons.erase(currentPolygons.begin() + s);

											const gg::MultiPolygon* polygons = dynamic_cast<const gg::MultiPolygon*>(erodeGeometry);
											for (int i = 0; i < polygons->getNumGeometries(); ++i)
											{
												currentPolygons.insert(currentPolygons.begin() + s, polygons->getGeometryN(i)->clone());
											}

											erodeGeometry = currentPolygons[s];
										}
										else if (erodeGeometry->getGeometryTypeId() != geos::geom::GEOS_POLYGON
											&& erodeGeometry->getGeometryTypeId() != geos::geom::GEOS_GEOMETRYCOLLECTION)
										{
											CGL_Error("Differenceの評価結果の型が不正です。");
										}
									}

									currentPolygons[s] = erodeGeometry;
								}
							}

							GeosPolygonsConcat(resultPolygons, currentPolygons);
						}
					}
				}
				catch (std::exception& e)
				{
					std::cout << "Packed Record2: " << e.what() << std::endl;
					throw;
				}
			}
			else if (member.first == "line")
			{
				if (cgl::IsType<cgl::PackedList>(value))
				{
					if (!ReadPackedLineData(cgl::As<cgl::PackedList>(value), currentLines, transform))
					{
						CGL_Error("lineに指定されたデータの形式が不正です。");
					}
				}
				else if (cgl::IsType<cgl::FuncVal>(value))
				{
					Eval evaluator(pContext);
					const cgl::PackedVal evaluated = Packed(pContext->expand(evaluator.callFunction(LocationInfo(), cgl::As<cgl::FuncVal>(value), {}), LocationInfo()), *pContext);

					if (!cgl::IsType<cgl::PackedList>(evaluated))
					{
						CGL_Error("line()の評価結果の型が不正です。");
					}

					if (!ReadPackedLineData(cgl::As<cgl::PackedList>(evaluated), currentLines, transform))
					{
						CGL_Error("lineに指定されたデータの形式が不正です。");
					}
				}
			}
			else if (cgl::IsType<cgl::PackedRecord>(value))
			{
				GeosPolygonsConcat(resultPolygons, GeosFromRecordPackedImpl(cgl::As<cgl::PackedRecord>(value), pContext, transform));
			}
			else if (cgl::IsType<cgl::PackedList>(value))
			{
				GeosPolygonsConcat(resultPolygons, GeosFromListPacked(cgl::As<cgl::PackedList>(value), pContext, transform));
			}
		}

		try
		{
			auto factory = gg::GeometryFactory::create();

			if (resultPolygons.empty())
			{
				return currentLines;
			}
			else
			{
				resultPolygons.insert(resultPolygons.end(), currentLines.begin(), currentLines.end());
				return resultPolygons;
			}
		}
		catch (std::exception& e)
		{
			std::cout << "Packed Record End: " << e.what() << std::endl;
			throw;
		}
	}

	std::vector<gg::Geometry*> GeosFromListPacked(const cgl::PackedList& list, std::shared_ptr<Context> pContext, const cgl::TransformPacked& transform)
	{
		std::vector<gg::Geometry*> currentPolygons;
		for (const auto& val : list.data)
		{
			const PackedVal& value = val.value;
			if (cgl::IsType<cgl::PackedRecord>(value))
			{
				GeosPolygonsConcat(currentPolygons, GeosFromRecordPackedImpl(cgl::As<cgl::PackedRecord>(value), pContext, transform));
			}
			else if (cgl::IsType<cgl::PackedList>(value))
			{
				GeosPolygonsConcat(currentPolygons, GeosFromListPacked(cgl::As<cgl::PackedList>(value), pContext, transform));
			}
		}

		return currentPolygons;
	}

	std::vector<gg::Geometry*> GeosFromRecordPacked(const PackedVal& value, std::shared_ptr<Context> pContext, const cgl::TransformPacked& parent)
	{
		if (cgl::IsType<cgl::PackedRecord>(value))
		{
			const auto& record = cgl::As<cgl::PackedRecord>(value);

			//valueが直下にxとyを持つ構造{x: _, y: _, ...}だった場合は単一のベクトル値として解釈する
			if (record.values.find("x") != record.values.end() &&
				record.values.find("y") != record.values.end())
			{
				const cgl::TransformPacked current(record);
				const cgl::TransformPacked transform = parent * current;

				std::vector<gg::Geometry*> currentPolygons;

				const auto v = ReadVec2Packed(record, transform);

				auto factory = gg::GeometryFactory::create();
				currentPolygons.push_back(factory->createPoint(gg::Coordinate(std::get<0>(v), std::get<1>(v))));

				return currentPolygons;
			}

			return GeosFromRecordPackedImpl(cgl::As<cgl::PackedRecord>(value), pContext, parent);
		}
		if (cgl::IsType<cgl::PackedList>(value))
		{
			return GeosFromListPacked(cgl::As<cgl::PackedList>(value), pContext, parent);
		}

		return{};
	}

	bool GeosFromList2Packed(std::ostream& os, const PackedList& list, const std::string& name, int depth, std::shared_ptr<Context> pContext, const cgl::TransformPacked& transform);

	bool GeosFromRecord2Packed(std::ostream& os, const PackedVal& value, const std::string& name, int depth, std::shared_ptr<Context> pContext, const cgl::TransformPacked& transform = cgl::TransformPacked());

	bool GeosFromRecordImpl2Packed(std::ostream& os, const PackedRecord& record, const std::string& name, int depth, std::shared_ptr<Context> pContext, const cgl::TransformPacked& parent)
	{
		const TransformPacked current(record);
		const TransformPacked transform = parent * current;

		std::vector<PitaGeometry> wholePolygons;

		std::vector<gg::Geometry*> resultPolygons;
		//gg::Geometry* currentLine = nullptr;
		std::vector<gg::Geometry*> currentLines;

		//現時点では実際に描画されるデータを持っているかどうかわからないため、一旦別のストリームに保存しておく
		std::stringstream currentStream;
		currentStream << getIndent(depth) << "<g id=\"" << name << "\" ";

		std::stringstream currentChildStream;

		//Color currentColor;

		bool hasShape = false;

		for (const auto& member : record.values)
		{
			const auto& value = member.second.value;

			if (member.first == "polygon")
			{
				std::vector<OutputPolygon> outputPolygons;

				if (IsType<PackedList>(value))
				{
					if (!ReadPackedPolyData(cgl::As<cgl::PackedList>(value), outputPolygons, transform))
					{
						CGL_Error("polygonに指定されたデータの形式が不正です。");
					}
				}
				else if (IsType<FuncVal>(value))
				{
					Eval evaluator(pContext);
					const cgl::PackedVal evaluated = Packed(pContext->expand(evaluator.callFunction(LocationInfo(), cgl::As<cgl::FuncVal>(value), {}), LocationInfo()), *pContext);

					if (!cgl::IsType<cgl::PackedList>(evaluated))
					{
						CGL_Error("polygon()の評価結果の型が不正です。");
					}

					if (!ReadPackedPolyData(cgl::As<cgl::PackedList>(evaluated), outputPolygons, transform))
					{
						CGL_Error("polygonに指定されたデータの形式が不正です。");
					}
				}

				for (auto& currentPolygon : outputPolygons)
				{
					auto& currentPolygons = currentPolygon.polygon;
					const auto& currentHoles = currentPolygon.hole;

					if (!currentPolygons.empty())
					{
						if (!currentHoles.empty())
						{
							for (int s = 0; s < currentPolygons.size(); ++s)
							{
								gg::Geometry* erodeGeometry = currentPolygons[s];

								for (int d = 0; d < currentHoles.size(); ++d)
								{
									gg::Geometry* testGeometry;
									try
									{
										testGeometry = erodeGeometry->difference(currentHoles[d]);
										erodeGeometry = testGeometry;
									}
									catch (const std::exception& e)
									{
										//std::cout << "error: " << e.what() << "\n";
										continue;
									}

									if (erodeGeometry->getGeometryTypeId() == geos::geom::GEOS_MULTIPOLYGON)
									{
										currentPolygons.erase(currentPolygons.begin() + s);

										const gg::MultiPolygon* polygons = dynamic_cast<const gg::MultiPolygon*>(erodeGeometry);
										for (int i = 0; i < polygons->getNumGeometries(); ++i)
										{
											currentPolygons.insert(currentPolygons.begin() + s, polygons->getGeometryN(i)->clone());
										}

										erodeGeometry = currentPolygons[s];
									}
									else if (erodeGeometry->getGeometryTypeId() != geos::geom::GEOS_POLYGON
										&& erodeGeometry->getGeometryTypeId() != geos::geom::GEOS_GEOMETRYCOLLECTION)
									{
										CGL_Error("Differenceの評価結果の型が不正です。");
									}
								}

								currentPolygons[s] = erodeGeometry;
							}
						}

						GeosPolygonsConcat(resultPolygons, currentPolygons);
					}
				}
			}
			else if (member.first == "line")
			{
				if (IsType<PackedList>(value))
				{
					if (!ReadPackedLineData(As<PackedList>(value), currentLines, transform))
					{
						CGL_Error("lineに指定されたデータの形式が不正です。");
					}
				}
				else if (IsType<FuncVal>(value))
				{
					Eval evaluator(pContext);
					const cgl::PackedVal evaluated = Packed(pContext->expand(evaluator.callFunction(LocationInfo(), cgl::As<cgl::FuncVal>(value), {}), LocationInfo()), *pContext);

					if (!cgl::IsType<cgl::PackedList>(evaluated))
					{
						CGL_Error("line()の評価結果の型が不正です。");
					}

					if (!ReadPackedLineData(As<PackedList>(evaluated), currentLines, transform))
					{
						CGL_Error("lineに指定されたデータの形式が不正です。");
					}
				}
			}
			else if (member.first == "fill")
			{
				if (IsType<PackedRecord>(value))
				{
					Color currentColor;
					ReadColorPacked(currentColor, As<PackedRecord>(value), transform);
					currentStream << "fill=\"" << currentColor.toString() << "\" ";
				}
				else
				{
					currentStream << "fill=\"none\" ";
				}
			}
			else if (member.first == "stroke")
			{
				if (IsType<PackedRecord>(value))
				{
					Color currentColor;
					ReadColorPacked(currentColor, As<PackedRecord>(value), transform);
					currentStream << "stroke=\"" << currentColor.toString() << "\" ";
				}
				else
				{
					currentStream << "stroke=\"none\" ";
				}
			}
			else if (member.first == "stroke_width")
			{
				if (!IsNum(value))
				{
					CGL_Error("stroke_widthに数値以外の値が指定されました");
				}
				currentStream << "stroke-width=\"" << AsDouble(value) << "\" ";
			}
			else if (IsType<PackedRecord>(value))
			{
				hasShape = GeosFromRecordImpl2Packed(currentChildStream, As<PackedRecord>(value), member.first, depth + 1, pContext, transform) || hasShape;
			}
			else if (IsType<PackedList>(value))
			{
				hasShape = GeosFromList2Packed(currentChildStream, As<PackedList>(value), member.first, depth + 1, pContext, transform) || hasShape;
			}
		}

		currentStream << ">\n";

		const auto writePolygon = [&depth](std::ostream& os, const gg::Polygon* polygon)
		{
			//穴がない場合ー＞Polygonで描画
			if (polygon->getNumInteriorRing() == 0)
			{
				const gg::LineString* outer = polygon->getExteriorRing();

				os << getIndent(depth + 1) << "<polygon " << "points=\"";
				if (IsClockWise(outer))
				{
					for (int i = 0; i < outer->getNumPoints(); ++i)
					{
						auto p = outer->getPointN(i);
						os << p->getX() << "," << p->getY() << " ";
					}
				}
				else
				{
					for (int i = outer->getNumPoints() - 1; 0 <= i; --i)
					{
						auto p = outer->getPointN(i);
						os << p->getX() << "," << p->getY() << " ";
					}
				}

				os << "\"/>\n";
			}
			//穴がある場合ー＞Pathで描画
			else
			{
				os << getIndent(depth + 1) << "<path " << "d=\"";

				{
					const gg::LineString* outer = polygon->getExteriorRing();

					if (outer->getNumPoints() != 0)
					{
						if (IsClockWise(outer))
						{
							auto p = outer->getPointN(0);
							os << "M" << p->getX() << "," << p->getY() << " ";

							for (int i = 1; i < outer->getNumPoints(); ++i)
							{
								auto p = outer->getPointN(i);
								os << "L" << p->getX() << "," << p->getY() << " ";
							}
						}
						else
						{
							auto p = outer->getPointN(outer->getNumPoints() - 1);
							os << "M" << p->getX() << "," << p->getY() << " ";

							for (int i = outer->getNumPoints() - 2; 0 < i; --i)
							{
								auto p = outer->getPointN(i);
								os << "L" << p->getX() << "," << p->getY() << " ";
							}
						}

						os << "z ";
					}
				}

				for (size_t i = 0; i < polygon->getNumInteriorRing(); ++i)
				{
					const gg::LineString* hole = polygon->getInteriorRingN(i);

					if (hole->getNumPoints() != 0)
					{
						if (IsClockWise(hole))
						{
							auto p = hole->getPointN(hole->getNumPoints() - 1);
							os << "M" << p->getX() << "," << p->getY() << " ";

							for (int n = hole->getNumPoints() - 2; 0 < n; --n)
							{
								auto p = hole->getPointN(n);
								os << "L" << p->getX() << "," << p->getY() << " ";
							}
						}
						else
						{
							auto p = hole->getPointN(0);
							os << "M" << p->getX() << "," << p->getY() << " ";

							for (int n = 1; n < hole->getNumPoints(); ++n)
							{
								auto p = hole->getPointN(n);
								os << "L" << p->getX() << "," << p->getY() << " ";
							}
						}

						os << "z ";
					}
				}

				os << "\"/>\n";
			}
		};

		const auto writeLine = [&depth](std::ostream& os, const gg::LineString* lineString)
		{
			os << getIndent(depth + 1) << "<polyline " << "fill=\"none\" points=\"";
			//os << getIndent(depth + 1) << "<polyline " << "points=\"";
			for (int i = 0; i < lineString->getNumPoints(); ++i)
			{
				const gg::Point* p = lineString->getPointN(i);
				os << p->getX() << "," << p->getY() << " ";
			}
			os << "\"/>\n";
		};

		const auto writePolygons = [&wholePolygons, &writePolygon, &writeLine](std::ostream& os)->bool
		{
			bool hasShape = false;
			for (const auto& geometry : wholePolygons)
			{
				if (geometry.shape->getGeometryTypeId() == gg::GeometryTypeId::GEOS_POLYGON)
				{
					hasShape = true;
					const gg::Polygon* polygon = dynamic_cast<const gg::Polygon*>(geometry.shape);
					writePolygon(os, polygon);
				}
				else if (geometry.shape->getGeometryTypeId() == gg::GeometryTypeId::GEOS_MULTIPOLYGON)
				{
					const gg::MultiPolygon* polygons = dynamic_cast<const gg::MultiPolygon*>(geometry.shape);
					for (int i = 0; i < polygons->getNumGeometries(); ++i)
					{
						hasShape = true;
						const gg::Polygon* polygon = dynamic_cast<const gg::Polygon*>(polygons->getGeometryN(i));
						writePolygon(os, polygon);
					}
				}
				else if (geometry.shape->getGeometryTypeId() == gg::GeometryTypeId::GEOS_LINESTRING)
				{
					hasShape = true;
					const gg::LineString* lineString = dynamic_cast<const gg::LineString*>(geometry.shape);
					writeLine(os, lineString);
				}
			}
			return hasShape;
		};

		const auto writeWholeData = [&]()
		{
			hasShape = writePolygons(currentStream) || hasShape;
			currentStream << currentChildStream.str();
			currentStream << getIndent(depth) << "</g>\n";

			if (hasShape)
			{
				os << currentStream.str();
			}
		};

		auto factory = gg::GeometryFactory::create();

		if (resultPolygons.empty() && currentLines.empty())
		{
			writeWholeData();

			return hasShape;
		}
		else if (resultPolygons.empty())
		{
			//パスの色はどうするか　別で指定する必要がある？
			//図形の境界線も考慮すると、塗りつぶしの色と線の色は別の名前で指定できるようにすべき
			for (gg::Geometry* line : currentLines)
			{
				wholePolygons.emplace_back(line, Color());
			}
			writeWholeData();

			return true;
		}
		else
		{
			for (gg::Geometry* geometry : resultPolygons)
			{
				wholePolygons.emplace_back(geometry, Color());
			}

			for (gg::Geometry* line : currentLines)
			{
				wholePolygons.emplace_back(line, Color());
			}

			writeWholeData();

			return true;
		}
	}

	bool GeosFromList2Packed(std::ostream& os, const PackedList& list, const std::string& name, int depth, std::shared_ptr<Context> pContext, const TransformPacked& transform)
	{
		std::vector<PitaGeometry> currentPolygons;

		bool hasShape = false;
		int i = 0;
		for (const auto& member : list.data)
		{
			const auto& value = member.value;

			std::stringstream currentName;
			currentName << name << "[" << i << "]";

			if (IsType<PackedRecord>(value))
			{
				hasShape = GeosFromRecordImpl2Packed(os, As<PackedRecord>(value), currentName.str(), depth + 1, pContext, transform) || hasShape;
			}
			else if (IsType<PackedList>(value))
			{
				hasShape = GeosFromList2Packed(os, As<PackedList>(value), currentName.str(), depth + 1, pContext, transform) || hasShape;
			}

			++i;
		}

		return hasShape;
	}

	bool GeosFromRecord2Packed(std::ostream& os, const PackedVal& value, const std::string& name, int depth, std::shared_ptr<Context> pContext, const TransformPacked& transform)
	{
		if (IsType<PackedRecord>(value))
		{
			const PackedRecord& record = As<PackedRecord>(value);
			return GeosFromRecordImpl2Packed(os, record, name, depth, pContext, transform);
		}
		if (IsType<PackedList>(value))
		{
			const PackedList& list = As<PackedList>(value);
			return GeosFromList2Packed(os, list, name, depth, pContext, transform);
		}

		return{};
	}

	bool OutputSVG2(std::ostream& os, const PackedVal& value, const std::string& name, std::shared_ptr<Context> pContext)
	{
		auto boundingBoxOpt = IsType<PackedRecord>(value) ? boost::optional<BoundingRect>(BoundingRectRecordPacked(value, pContext)) : boost::none;
		if (IsType<PackedRecord>(value) && boundingBoxOpt)
		{
			const BoundingRect& rect = boundingBoxOpt.get();
			//const auto pos = rect.pos();
			const auto widthXY = rect.width();
			const auto center = rect.center();

			const double margin = 5.0;
			//const double width = std::max(widthXY.x(), widthXY.y());
			//const double halfWidth = width * 0.5;
			const double width = widthXY.x() + margin;
			const double height = widthXY.y() + margin;

			const Eigen::Vector2d pos = center - EigenVec2(width*0.5, height*0.5);

			/*os << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.2" baseProfile="tiny" width=")" << width << R"(" height=")" << height << R"(" viewBox=")" << pos.x() << " " << pos.y() << " " << width << " " << height
				<< R"(" viewport-fill="black" viewport-fill-opacity="0.1)"  << R"(">)" << "\n";*/
			os << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << width << R"(" height=")" << height << R"(" viewBox=")" << pos.x() << " " << pos.y() << " " << width << " " << height << R"(">)" << "\n";
			GeosFromRecord2Packed(os, value, name, 0, pContext);
			os << "</svg>" << "\n";

			return true;
		}

		return false;
	}
}

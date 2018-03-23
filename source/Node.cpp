#pragma warning(disable:4996)
#include <functional>
#include <thread>
#include <mutex>

#include <boost/functional/hash.hpp>

#include <cppoptlib/meta.h>
#include <cppoptlib/problem.h>
#include <cppoptlib/solver/bfgssolver.h>

#include <cmaes.h>

#include <Unicode.hpp>

#include <Pita/Node.hpp>
#include <Pita/Context.hpp>
#include <Pita/OptimizationEvaluator.hpp>
#include <Pita/Parser.hpp>
#include <Pita/Evaluator.hpp>
#include <Pita/Printer.hpp>

namespace cgl
{
	//manip::LocationInfoPrinter LocationInfo::printLoc() const { return { *this }; }
	std::string LocationInfo::getInfo() const
	{
		std::stringstream ss;
		ss << "[L" << locInfo_lineBegin << ":" << locInfo_posBegin << "-" << "L" << locInfo_lineEnd << ":" << locInfo_posEnd << "]";
		return ss.str();
	}

	Identifier Identifier::MakeIdentifier(const std::u32string& name_)
	{
		return Identifier(Unicode::UTF32ToUTF8(name_));
	}

	LRValue LRValue::Float(const std::u32string& str)
	{
		return LRValue(std::stod(Unicode::UTF32ToUTF8(str)));
	}

	bool LRValue::isValid() const
	{
		return IsType<Address>(value)
			? As<Address>(value).isValid()
			: true; //Reference と Val は常に有効であるものとする
	}

	std::string LRValue::toString() const
	{
		if (isAddress())
		{
			return std::string("Address(") + As<Address>(value).toString() + std::string(")");
		}
		else if (isReference())
		{
			return std::string("Reference(") + As<Reference>(value).toString() + std::string(")");
		}
		else
		{
			return std::string("Val(...)");
		}
	}

	Address LRValue::address(const Context & env) const
	{
		return IsType<Address>(value)
			? As<Address>(value)
			: env.getReference(As<Reference>(value));
	}

	class ExprImportForm : public boost::static_visitor<Expr>
	{
	public:
		ExprImportForm(bool isTopLevel)
			:isTopLevel(isTopLevel)
		{}

		bool isTopLevel;

		Expr operator()(const Lines& node)const
		{
			if (!isTopLevel)
			{
				return node;
			}

			RecordConstractor result;
			for (const auto& expr : node.exprs)
			{
				result.add(boost::apply_visitor(ExprImportForm(false), expr));
			}

			return result;
		}

		Expr operator()(const BinaryExpr& node)const
		{
			if (node.op != BinaryOp::Assign)
			{
				return node;
			}

			KeyExpr keyExpr(As<Identifier>(node.lhs));
			KeyExpr::SetExpr(keyExpr, node.rhs);
			return keyExpr;
		}

		Expr operator()(const LRValue& node)const { return node; }
		Expr operator()(const Identifier& node)const { return node; }
		Expr operator()(const Import& node)const { return node; }
		Expr operator()(const UnaryExpr& node)const { return node; }
		Expr operator()(const Range& node)const { return node; }
		Expr operator()(const DefFunc& node)const { return node; }
		Expr operator()(const If& node)const { return node; }
		Expr operator()(const For& node)const { return node; }
		Expr operator()(const Return& node)const { return node; }
		Expr operator()(const ListConstractor& node)const { return node; }
		Expr operator()(const KeyExpr& node)const { return node; }
		Expr operator()(const RecordConstractor& node)const { return node; }
		Expr operator()(const RecordInheritor& node)const { return node; }
		Expr operator()(const Accessor& node)const { return node; }
		Expr operator()(const DeclSat& node)const { return node; }
		Expr operator()(const DeclFree& node)const { return node; }
	};

	Expr ToImportForm(const Expr& expr)
	{
		ExprImportForm converter(true);
		return boost::apply_visitor(converter, expr);
	}

	Import::Import(const std::u32string& filePath)
	{
		const std::string u8FilePath = Unicode::UTF32ToUTF8(filePath);
		const auto path = cgl::filesystem::path(u8FilePath);

		std::string sourceCode;

		if (path.is_absolute())
		{
			const std::string pathStr = filesystem::canonical(path).string();

			importPath = pathStr;

			/*if (alreadyImportedFiles.find(filesystem::canonical(path)) != alreadyImportedFiles.end())
			{
				std::cout << "File \"" << path.string() << "\" has been already imported.\n";
				originalParseTree = boost::none;
				return;
			}

			alreadyImportedFiles.emplace(filesystem::canonical(path));

			std::ifstream ifs(u8FilePath);
			if (!ifs.is_open())
			{
				CGL_Error(std::string() + "Error: import file \"" + u8FilePath + "\" does not exists.");
			}

			sourceCode = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			cgl::workingDirectories.emplace(path.parent_path());*/
		}
		else
		{
			const auto currentDirectory = workingDirectories.top();
			const auto currentFilePath = currentDirectory / path;
			const std::string pathStr = filesystem::canonical(currentFilePath).string();

			importPath = pathStr;
			/*if (alreadyImportedFiles.find(filesystem::canonical(currentFilePath)) != alreadyImportedFiles.end())
			{
				std::cout << "File \"" << filesystem::canonical(currentFilePath).string() << "\" has been already imported.\n";
				originalParseTree = boost::none;
				return;
			}

			alreadyImportedFiles.emplace(filesystem::canonical(currentFilePath));

			std::ifstream ifs(currentFilePath.string());
			if (!ifs.is_open())
			{
				CGL_Error(std::string() + "Error: import file \"" + currentFilePath.string() + "\" does not exists.");
			}

			sourceCode = std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			cgl::workingDirectories.emplace(currentFilePath.parent_path());*/
		}

		/*if (auto opt = Parse(sourceCode))
		{
			originalParseTree = opt;
		}
		else
		{
			CGL_Error("Parse failed.");
		}*/
		updateHash();
	}

	Import::Import(const std::u32string& path, const Identifier& name):
		Import(path)
	{
		//name = importName;
		importName = name;
		updateHash();
	}

	LRValue Import::eval(std::shared_ptr<Context> pContext)const
	{
		auto it = importedParseTrees.find(seed);
		if (it == importedParseTrees.end() || !it->second)
		{
			CGL_Error("ファイルのimportに失敗");
			return RValue(0);
		}

		if (!importName.empty())
		{
			const Expr importParseTree = BinaryExpr(Identifier(importName), ToImportForm(it->second.value()), BinaryOp::Assign);
			printExpr(importParseTree, pContext, std::cout);
			Eval evaluator(pContext);
			return boost::apply_visitor(evaluator, importParseTree);
		}

		if (IsType<Lines>(it->second.value()))
		{
			Eval evaluator(pContext);

			LRValue result;
			const auto& lines = As<Lines>(it->second.value());
			for (const auto& expr : lines.exprs)
			{
				result = boost::apply_visitor(evaluator, expr);
			}

			return result;
		}

		CGL_Error("ファイルのimportに失敗");

		/*
		if(!originalParseTree)
		{
			//CGL_Error("ファイルのimportに失敗");
			return RValue(0);
		}

		if (name)
		{
			const Expr importParseTree = BinaryExpr(name.value(), ToImportForm(originalParseTree.value()), BinaryOp::Assign);
			printExpr(importParseTree, pContext, std::cout);
			Eval evaluator(pContext);
			return boost::apply_visitor(evaluator, importParseTree);
		}

		if (IsType<Lines>(originalParseTree.value()))
		{
			Eval evaluator(pContext);

			LRValue result;
			const auto& lines = As<Lines>(originalParseTree.value());
			for (const auto& expr : lines.exprs)
			{
				result = boost::apply_visitor(evaluator, expr);
			}

			return result;
		}
		//if (name)
		//{

		//}

		//if (IsType<Lines>(originalParseTree.value()))
		//{
		//	Eval evaluator(pContext);

		//	LRValue result;
		//	const auto& lines = As<Lines>(originalParseTree.value());
		//	for (const auto& expr : lines.exprs)
		//	{
		//		//std::cout << "====================================================================================" << std::endl;
		//		//printExpr(expr, pContext, std::cout);
		//		result = boost::apply_visitor(evaluator, expr);
		//		//pContext->printContext(std::cout);
		//		//std::cout << "------------------------------------------------------------------------------------" << std::endl;
		//	}

		//	//std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
		//	//pContext->printContext(std::cout);

		//	//printVal(pContext->expand(result), pContext, std::cout);
		//	return result;
		//}

		CGL_Error("ファイルのimportに失敗");

		*/
	}

	void Import::SetName(Import& node, const Identifier& name)
	{
		node.importName = name;
		node.updateHash();
	}

	void Import::updateHash()
	{
		seed = 0;
		boost::hash_combine(seed, importPath);
		boost::hash_combine(seed, importName);
	}

	Expr BuildString(const std::u32string& str32)
	{
		Expr expr;
		expr = LRValue(CharString(str32));

		return expr;
	}

	class ConstraintProblem : public cppoptlib::Problem<double>
	{
	public:
		using typename cppoptlib::Problem<double>::TVector;

		std::function<double(const TVector&)> evaluator;
		Record originalRecord;
		std::vector<Identifier> keyList;
		std::shared_ptr<Context> pEnv;

		bool callback(const cppoptlib::Criteria<cppoptlib::Problem<double>::Scalar>& state, const TVector& x) override;

		double value(const TVector &x) override
		{
			return evaluator(x);
		}
	};

	bool ConstraintProblem::callback(const cppoptlib::Criteria<cppoptlib::Problem<double>::Scalar> &state, const TVector &x)
	{
		/*
		Record tempRecord = originalRecord;

		for (size_t i = 0; i < x.size(); ++i)
		{
		Address address = originalRecord.freeVariableRefs[i];
		pEnv->assignToObject(address, x[i]);
		}

		for (const auto& key : keyList)
		{
		Address address = pEnv->findAddress(key);
		tempRecord.append(key, address);
		}

		ProgressStore::TryWrite(pEnv, tempRecord);
		*/
		return true;
	}

	void OptimizationProblemSat::addUnitConstraint(const Expr& logicExpr)
	{
		if (expr)
		{
			expr = BinaryExpr(expr.value(), logicExpr, BinaryOp::And);
		}
		else
		{
			expr = logicExpr;
		}
	}

	//void OptimizationProblemSat::constructConstraint(std::shared_ptr<Context> pEnv, std::vector<std::pair<Address, VariableRange>>& freeVariables)
	void OptimizationProblemSat::constructConstraint(std::shared_ptr<Context> pEnv)
	{
		refs.clear();
		invRefs.clear();
		hasPlateausFunction = false;

		if (!expr || freeVariableRefs.empty())
		{
			return;
		}

		/*Expr2SatExpr evaluator(0, pEnv, freeVariables);
		expr = boost::apply_visitor(evaluator, candidateExpr.value());
		refs.insert(refs.end(), evaluator.refs.begin(), evaluator.refs.end());

		{
			CGL_DebugLog("Print:");
			Printer printer;
			boost::apply_visitor(printer, expr.value());
			CGL_DebugLog("");
		}*/

		std::unordered_set<Address> appearingList;

		CGL_DebugLog("freeVariables:");
		for (const auto& val : freeVariableRefs)
		{
			CGL_DebugLog(std::string("  Address(") + val.toString() + ")");
		}
		
		std::vector<char> usedInSat(freeVariableRefs.size(), 0);
		//SatVariableBinder binder(pEnv, freeVariables);
		SatVariableBinder binder(pEnv, freeVariableRefs, usedInSat, refs, appearingList, invRefs, hasPlateausFunction);
		if (boost::apply_visitor(binder, expr.value()))
		{
			//refs = binder.refs;
			//invRefs = binder.invRefs;
			//hasPlateausFunction = binder.hasPlateausFunction;

			//satに出てこないfreeVariablesの削除
			for (int i = static_cast<int>(freeVariableRefs.size()) - 1; 0 <= i; --i)
			{
				if (usedInSat[i] == 0)
				{
					freeVariableRefs.erase(freeVariableRefs.begin() + i);
				}
			}
		}
		else
		{
			refs.clear();
			invRefs.clear();
			freeVariableRefs.clear();
			hasPlateausFunction = false;
		}

		/*{
			CGL_DebugLog("env:");
			pEnv->printContext(true);

			CGL_DebugLog("expr:");
			printExpr(candidateExpr.value());
		}*/
	}

	bool OptimizationProblemSat::initializeData(std::shared_ptr<Context> pEnv)
	{
		data.resize(refs.size());

		for (size_t i = 0; i < data.size(); ++i)
		{
			const auto opt = pEnv->expandOpt(refs[i]);
			if (!opt)
			{
				CGL_Error("参照エラー");
			}
			const Val& val = opt.value();
			if (auto opt = AsOpt<double>(val))
			{
				CGL_DebugLog(ToS(i) + " : " + ToS(opt.value()));
				data[i] = opt.value();
			}
			else if (auto opt = AsOpt<int>(val))
			{
				CGL_DebugLog(ToS(i) + " : " + ToS(opt.value()));
				data[i] = opt.value();
			}
			else
			{
				CGL_Error("存在しない参照をsatに指定した");
			}
		}

		return true;
	}

	std::vector<double> OptimizationProblemSat::solve(std::shared_ptr<Context> pEnv, const Record currentRecord, const std::vector<Identifier>& currentKeyList)
	{
		constructConstraint(pEnv);

		std::cout << "Current constraint freeVariablesSize: " << std::to_string(freeVariableRefs.size()) << std::endl;

		if (!initializeData(pEnv))
		{
			CGL_Error("制約の初期化に失敗");
		}

		std::vector<double> resultxs;

		if (!freeVariableRefs.empty())
		{
			//varのアドレス(の内実際にsatに現れるもののリスト)から、OptimizationProblemSat中の変数リストへの対応付けを行うマップを作成
			std::unordered_map<int, int> variable2Data;
			for (size_t freeIndex = 0; freeIndex < freeVariableRefs.size(); ++freeIndex)
			{
				CGL_DebugLog(ToS(freeIndex));
				CGL_DebugLog(std::string("Address(") + freeVariableRefs[freeIndex].toString() + ")");
				const auto& ref1 = freeVariableRefs[freeIndex];

				bool found = false;
				for (size_t dataIndex = 0; dataIndex < refs.size(); ++dataIndex)
				{
					CGL_DebugLog(ToS(dataIndex));
					CGL_DebugLog(std::string("Address(") + refs[dataIndex].toString() + ")");

					const auto& ref2 = refs[dataIndex];

					if (ref1.first == ref2)
					{
						//std::cout << "    " << freeIndex << " -> " << dataIndex << std::endl;

						found = true;
						variable2Data[freeIndex] = dataIndex;
						break;
					}
				}

				//DeclFreeにあってDeclSatに無い変数は意味がない。
				//単に無視しても良いが、恐らく入力のミスと思われるので警告を出す
				if (!found)
				{
					CGL_WarnLog("freeに指定された変数が無効です");
				}
			}
			CGL_DebugLog("End Record MakeMap");

			if (hasPlateausFunction)
			{
				std::cout << "Solve constraint by CMA-ES...\n";

				libcmaes::FitFunc func = [&](const double *x, const int N)->double
				{
					for (int i = 0; i < N; ++i)
					{
						update(variable2Data[i], x[i]);
					}

					{
						for (const auto& keyval : invRefs)
						{
							pEnv->TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(keyval.first, data[keyval.second]);
						}
					}

					pEnv->switchFrontScope();
					double result = eval(pEnv);
					pEnv->switchBackScope();

					CGL_DebugLog(std::string("cost: ") + ToS(result, 17));

					return result;
				};

				CGL_DebugLog("");

				std::vector<double> x0(freeVariableRefs.size());
				for (int i = 0; i < x0.size(); ++i)
				{
					x0[i] = data[variable2Data[i]];
					CGL_DebugLog(ToS(i) + " : " + ToS(x0[i]));
				}

				CGL_DebugLog("");

				const double sigma = 0.1;

				const int lambda = 100;

				libcmaes::CMAParameters<> cmaparams(x0, sigma, lambda, 1);
				CGL_DebugLog("");
				libcmaes::CMASolutions cmasols = libcmaes::cmaes<>(func, cmaparams);
				CGL_DebugLog("");
				resultxs = cmasols.best_candidate().get_x();
				CGL_DebugLog("");

				std::cout << "solved\n";
			}
			else
			{
				std::cout << "Solve constraint by BFGS...\n";

				ConstraintProblem constraintProblem;
				constraintProblem.evaluator = [&](const ConstraintProblem::TVector& v)->double
				{
					//-1000 -> 1000
					for (int i = 0; i < v.size(); ++i)
					{
						update(variable2Data[i], v[i]);
						//problem.update(variable2Data[i], (v[i] - 0.5)*2000.0);
					}

					{
						for (const auto& keyval : invRefs)
						{
							pEnv->TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(keyval.first, data[keyval.second]);
						}
					}

					pEnv->switchFrontScope();
					double result = eval(pEnv);
					pEnv->switchBackScope();

					CGL_DebugLog(std::string("cost: ") + ToS(result, 17));
					//std::cout << std::string("cost: ") << ToS(result, 17) << "\n";
					return result;
				};
				constraintProblem.originalRecord = currentRecord;
				constraintProblem.keyList = currentKeyList;
				constraintProblem.pEnv = pEnv;

				Eigen::VectorXd x0s(freeVariableRefs.size());
				for (int i = 0; i < x0s.size(); ++i)
				{
					x0s[i] = data[variable2Data[i]];
					//x0s[i] = (problem.data[variable2Data[i]] / 2000.0) + 0.5;
					CGL_DebugLog(ToS(i) + " : " + ToS(x0s[i]));
				}

				cppoptlib::BfgsSolver<ConstraintProblem> solver;
				solver.minimize(constraintProblem, x0s);

				resultxs.resize(x0s.size());
				for (int i = 0; i < x0s.size(); ++i)
				{
					resultxs[i] = x0s[i];
				}

				//std::cout << "solved\n";
			}
		}

		return resultxs;
	}

	double OptimizationProblemSat::eval(std::shared_ptr<Context> pEnv)
	{
		if (!expr)
		{
			return 0.0;
		}

		if (data.empty())
		{
			CGL_WarnLog("free式に有効な変数が指定されていません。");
			return 0.0;
		}

		/*{
			CGL_DebugLog("data:");
			for(int i=0;i<data.size();++i)
			{
				CGL_DebugLog(std::string("  ID(") + ToS(i) + ") -> " + ToS(data[i]));
			}

			CGL_DebugLog("refs:");
			for (int i = 0; i<refs.size(); ++i)
			{
				CGL_DebugLog(std::string("  ID(") + ToS(i) + ") -> Address(" + refs[i].toString() + ")");
			}

			CGL_DebugLog("invRefs:");
			for(const auto& keyval : invRefs)
			{
				CGL_DebugLog(std::string("  Address(") + keyval.first.toString() + ") -> ID(" + ToS(keyval.second) + ")");
			}

			CGL_DebugLog("env:");
			pEnv->printContext();

			CGL_DebugLog("expr:");
			printExpr(expr.value());
		}*/
		
		EvalSatExpr evaluator(pEnv, data, refs, invRefs);
		const Val evaluated = boost::apply_visitor(evaluator, expr.value());
		
		if (IsType<double>(evaluated))
		{
			return As<double>(evaluated);
		}
		else if (IsType<int>(evaluated))
		{
			return As<int>(evaluated);
		}

		CGL_Error("sat式の評価結果が不正");
	}

	//中身のアドレスを全て一つの値にまとめる
	class ValuePacker : public boost::static_visitor<PackedVal>
	{
	public:
		ValuePacker(const Context& context) :
			context(context)
		{}

		const Context& context;

		PackedVal operator()(bool node)const { return node; }
		PackedVal operator()(int node)const { return node; }
		PackedVal operator()(double node)const { return node; }
		PackedVal operator()(const CharString& node)const { return node; }
		PackedVal operator()(const List& node)const { return node.packed(context); }
		PackedVal operator()(const KeyValue& node)const { return node; }
		PackedVal operator()(const Record& node)const { return node.packed(context); }
		PackedVal operator()(const FuncVal& node)const { return node; }
		PackedVal operator()(const Jump& node)const { return node; }
	};

	//中身のアドレスを全て展開する
	class ValueUnpacker : public boost::static_visitor<Val>
	{
	public:
		ValueUnpacker(Context& context) :
			context(context)
		{}

		Context& context;

		Val operator()(bool node)const { return node; }
		Val operator()(int node)const { return node; }
		Val operator()(double node)const { return node; }
		Val operator()(const CharString& node)const { return node; }
		Val operator()(const PackedList& node)const { return node.unpacked(context); }
		Val operator()(const KeyValue& node)const { return node; }
		Val operator()(const PackedRecord& node)const { return node.unpacked(context); }
		Val operator()(const FuncVal& node)const { return node; }
		Val operator()(const Jump& node)const { return node; }
	};

	PackedVal Packed(const Val& value, const Context& context)
	{
		ValuePacker packer(context);
		return boost::apply_visitor(packer, value);
	}

	Val Unpacked(const PackedVal& packedValue, Context& context)
	{
		ValueUnpacker unpacker(context);
		return boost::apply_visitor(unpacker, packedValue);
	}

	Val PackedList::unpacked(Context& context)const
	{
		ValueUnpacker unpacker(context);

		List result;

		for (const auto& val : data)
		{
			const Address address = val.address;

			const PackedVal& packedValue = val.value;
			const Val value = boost::apply_visitor(unpacker, packedValue);

			if (address.isValid())
			{
				context.TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(address, value);
				result.add(address);
			}
			else
			{
				result.add(context.makeTemporaryValue(value));
			}
		}

		return result;
	}

	PackedVal List::packed(const Context& context)const
	{
		ValuePacker packer(context);

		PackedList result;

		for (const Address address : data)
		{
			const auto& opt = context.expandOpt(address);
			if (!opt)
			{
				CGL_Error("参照エラー");
			}
			const PackedVal packedValue = boost::apply_visitor(packer, opt.value());

			result.add(address, packedValue);
		}

		return result;
	}

	Val PackedRecord::unpacked(Context& context)const
	{
		ValueUnpacker unpacker(context);

		Record result;

		for (const auto& keyval : values)
		{
			const Address address = keyval.second.address;

			const PackedVal& packedValue = keyval.second.value;
			const Val value = boost::apply_visitor(unpacker, packedValue);

			if (address.isValid())
			{
				context.TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(address, value);
				result.add(keyval.first, address);
			}
			else
			{
				result.add(keyval.first, context.makeTemporaryValue(value));
			}
		}

		result.problems = problems;
		result.freeVariables = freeVariables;
		//result.freeVariableRefs = freeVariableRefs;
		result.freeRanges = freeRanges;
		result.type = type;
		result.isSatisfied = isSatisfied;
		result.pathPoints = pathPoints;
		result.constraint = constraint;

		//result.unitConstraints = unitConstraints;
		//result.variableAppearances = variableAppearances;
		////result.constraintGroups = constraintGroups;
		//result.groupConstraints = groupConstraints;
		result.original = original;

		return result;
	}

	PackedVal Record::packed(const Context& context)const
	{
		ValuePacker packer(context);

		PackedRecord result;

		for (const auto& keyval : values)
		{
			const auto& opt = context.expandOpt(keyval.second);
			if (!opt)
			{
				CGL_Error("参照エラー");
			}
			const PackedVal packedValue = boost::apply_visitor(packer, opt.value());

			result.add(keyval.first, keyval.second, packedValue);
		}

		result.problems = problems;
		result.freeVariables = freeVariables;
		//result.freeVariableRefs = freeVariableRefs;
		result.freeRanges = freeRanges;
		result.type = type;
		result.isSatisfied = isSatisfied;
		result.pathPoints = pathPoints;
		result.constraint = constraint;

		//result.unitConstraints = unitConstraints;
		//result.variableAppearances = variableAppearances;
		////result.constraintGroups = constraintGroups;
		//result.groupConstraints = groupConstraints;
		result.original = original;

		return result;
	}
}

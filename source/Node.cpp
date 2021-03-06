#pragma warning(disable:4996)
#include <functional>
#include <thread>
#include <mutex>

#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>

#include <cppoptlib/meta.h>
#include <cppoptlib/problem.h>
#include <cppoptlib/solver/bfgssolver.h>

#include <cmaes.h>

#include <Unicode.hpp>

//#define USE_OPTIONAL_LIB

#ifdef USE_OPTIONAL_LIB
#define USE_NLOPT
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <nlopt.hpp>

#include <limbo/acqui.hpp>
#include <limbo/bayes_opt/boptimizer.hpp>
#include <limbo/kernel/matern_five_halves.hpp>
#include <limbo/mean/data.hpp>
#include <limbo/model/gp.hpp>
#include <limbo/stat.hpp>
#include <limbo/tools/macros.hpp>

#ifdef _DEBUG
#pragma comment(lib, "Debug/nlopt_cxx.lib")
#else
#pragma comment(lib, "Release/nlopt_cxx.lib")
#endif

//struct Params {
//	struct acqui_gpucb : public limbo::defaults::acqui_gpucb {
//	};
//
//#ifdef USE_NLOPT
//	struct opt_nloptnograd : public limbo::defaults::opt_nloptnograd {
//	};
//#elif defined(USE_LIBCMAES)
//	struct opt_cmaes : public defaults::opt_cmaes {
//	};
//#else
//	struct opt_gridsearch : public limbo::defaults::opt_gridsearch {
//	};
//#endif
//	struct acqui_ucb {
//		BO_PARAM(double, alpha, 0.1);
//	};
//
//	struct kernel : public limbo::defaults::kernel {
//		BO_PARAM(double, noise, 0.001);
//	};
//
//	struct kernel_maternfivehalves {
//		BO_PARAM(double, sigma_sq, 1);
//		BO_PARAM(double, l, 0.2);
//	};
//	struct kernel_exp : public limbo::defaults::kernel_exp {
//	};
//	struct bayes_opt_bobase : public limbo::defaults::bayes_opt_bobase {
//		BO_PARAM(bool, stats_enabled, true);
//	};
//
//	struct bayes_opt_boptimizer : public limbo::defaults::bayes_opt_boptimizer {
//	};
//
//	struct init_randomsampling {
//		BO_PARAM(int, samples, 5);
//	};
//
//	struct stop_maxiterations {
//		BO_PARAM(int, iterations, 20);
//	};
//	struct stat_gp {
//		BO_PARAM(int, bins, 20);
//	};
//
//	struct kernel_squared_exp_ard : public limbo::defaults::kernel_squared_exp_ard {
//	};
//
//	struct opt_rprop : public limbo::defaults::opt_rprop {
//	};
//};

struct Params {
	struct bayes_opt_boptimizer : public limbo::defaults::bayes_opt_boptimizer {
		BO_PARAM(int, hp_period, 10);
	};
	struct bayes_opt_bobase : public limbo::defaults::bayes_opt_bobase {
		BO_PARAM(int, stats_enabled, true);
	};
	
	struct init_randomsampling {
		BO_PARAM(int, samples, 100);
	};
	struct stop_maxiterations {
		BO_PARAM(int, iterations, 500);
	};
	struct stop_mintolerance {
		BO_PARAM(double, tolerance, -0.1);
	};

	struct acqui_ei {
		BO_PARAM(double, jitter, 0.0);
	};

	//struct acqui_gpucb : public limbo::defaults::acqui_gpucb {
	//};
	//struct acqui_ucb {
	//	//BO_PARAM(double, alpha, 0.1);
	//	//BO_PARAM(double, alpha, 0.3);
	//	BO_PARAM(double, alpha, 0.5);
	//};

	/*struct kernel : public limbo::defaults::kernel {
		BO_PARAM(double, noise, 0.001);
	};*/
	struct kernel : public limbo::defaults::kernel {
		//BO_PARAM(double, noise, 1.e-10);
		BO_PARAM(double, noise, 0.01);
	};
	struct kernel_squared_exp_ard : public limbo::defaults::kernel_squared_exp_ard {
	};
	struct kernel_maternfivehalves {
		BO_PARAM(double, sigma_sq, 1);
		BO_PARAM(double, l, 0.2);
	};
	struct kernel_exp : public limbo::defaults::kernel_exp {
	};

	struct stat_gp {
		BO_PARAM(int, bins, 3);
	};

	struct opt_nloptnograd : public limbo::defaults::opt_nloptnograd {
	};
	struct opt_rprop : public limbo::defaults::opt_rprop {
	};
};

struct LimboFitFunc {
	std::function<double(const Eigen::VectorXd& x)> func;
	size_t numOfVars;
	size_t dim_in()const { return numOfVars; }
	size_t dim_out()const { return 1; }

	Eigen::VectorXd operator()(const Eigen::VectorXd& x) const
	{
		return limbo::tools::make_vector(func(x));
	}
};
#endif

#include <Pita/Node.hpp>
#include <Pita/Context.hpp>
#include <Pita/OptimizationEvaluator.hpp>
#include <Pita/Parser.hpp>
#include <Pita/Evaluator.hpp>
#include <Pita/Printer.hpp>
#include <Pita/IntrinsicGeometricFunctions.hpp>

extern bool printAddressInsertion;
extern double cloneTime;
extern unsigned cloneCount;
extern bool isDebugMode;

namespace cgl
{
	std::string UnaryOpToStr(UnaryOp op)
	{
		switch (op)
		{
		case UnaryOp::Not:     return "Not";
		case UnaryOp::Plus:    return "Plus";
		case UnaryOp::Minus:   return "Minus";
		case UnaryOp::Dynamic: return "Dynamic";
		}

		return "UnknownUnaryOp";
	}

	std::string BinaryOpToStr(BinaryOp op)
	{
		switch (op)
		{
		case BinaryOp::And: return "And";
		case BinaryOp::Or:  return "Or";

		case BinaryOp::Equal:        return "Equal";
		case BinaryOp::NotEqual:     return "NotEqual";
		case BinaryOp::LessThan:     return "LessThan";
		case BinaryOp::LessEqual:    return "LessEqual";
		case BinaryOp::GreaterThan:  return "GreaterThan";
		case BinaryOp::GreaterEqual: return "GreaterEqual";

		case BinaryOp::Add: return "Add";
		case BinaryOp::Sub: return "Sub";
		case BinaryOp::Mul: return "Mul";
		case BinaryOp::Div: return "Div";

		case BinaryOp::Pow:    return "Pow";
		case BinaryOp::Assign: return "Assign";

		case BinaryOp::Concat: return "Concat";

		case BinaryOp::SetDiff: return "SetDiff";
		}

		return "UnknownBinaryOp";
	}

	bool IsVec2(const Val& value)
	{
		if (!IsType<Record>(value))
		{
			return false;
		}

		const auto& values = As<Record>(value).values;
		return values.find("x") != values.end() && values.find("y") != values.end();
	}

	bool IsShape(const Val& value)
	{
		if (!IsType<Record>(value))
		{
			return false;
		}

		const auto& values = As<Record>(value).values;
		return values.find("x") == values.end() || values.find("y") == values.end();
	}

	Eigen::Vector2d AsVec2(const Val& value, const Context& context)
	{
		const auto& values = As<Record>(value).values;

		const Val xval = context.expand(LRValue(values.find("x")->second), LocationInfo());
		const Val yval = context.expand(LRValue(values.find("y")->second), LocationInfo());

		return Eigen::Vector2d(AsDouble(xval), AsDouble(yval));
	}

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

	bool Identifier::isDeferredCall()const
	{
		return boost::starts_with(name, "#DeferredCall");
	}

	Identifier Identifier::asDeferredCall(const ScopeAddress& identifierScopeInfo)const
	{
		if (isDeferredCall())
		{
			return *this;
		}

		std::string head("#DeferredCall(");
		for (unsigned scopeIndex : identifierScopeInfo)
		{
			head += std::to_string(scopeIndex) + ",";
		}
		head += ")";
		
		return Identifier(head + name);
	}

	bool Identifier::isMakeClosure()const
	{
		return boost::starts_with(name, "#MakeClosure");
	}

	Identifier Identifier::asMakeClosure(const ScopeAddress& identifierScopeInfo)const
	{
		if (isMakeClosure())
		{
			return *this;
		}

		std::string head("#MakeClosure(");
		for (unsigned scopeIndex : identifierScopeInfo)
		{
			head += std::to_string(scopeIndex) + ",";
		}
		head += ")";

		return Identifier(head + name);
	}

	std::pair<ScopeAddress, Identifier> Identifier::decomposed()const
	{
		if (!isDeferredCall() && !isMakeClosure())
		{
			return { {},*this };
		}

		const size_t tagEndIndex = name.find_first_of(')');

		ScopeAddress scopeAddress;
		{
			const std::string scopeIndices(name.begin() + name.find_first_of('(') + 1, name.begin() + tagEndIndex);
			size_t currentPos = 0;
			size_t nextPos = scopeIndices.find_first_of(',', currentPos);
			while (nextPos != std::string::npos)
			{
				const std::string currentStr(scopeIndices.begin() + currentPos, scopeIndices.begin() + nextPos);
				const int scopeIndex = std::stoi(currentStr);
				scopeAddress.push_back(scopeIndex);
				currentPos = nextPos + 1;
				nextPos = scopeIndices.find_first_of(',', currentPos);
			}
		}

		const Identifier rawIdentifier(std::string(name.begin() + tagEndIndex + 1, name.end()));
		return { scopeAddress,rawIdentifier };
	}

	bool EitherReference::localReferenciable(const Context& context)const
	{
		return local && context.existsInLocalScope(local.get());
	}

	std::string EitherReference::toString()const
	{
		std::stringstream ss;
		ss << (local ? local.get().toString() : std::string("None"));
		ss << " | " << "Address(" << replaced.toString() << ")";
		return ss.str();
	}

	LRValue LRValue::Bool(bool a)
	{
		return LRValue(Val(a));
	}
	
	LRValue LRValue::Int(int a)
	{
		return LRValue(Val(a));
	}

	LRValue LRValue::Float(const std::u32string& str)
	{
		return LRValue(std::stod(Unicode::UTF32ToUTF8(str)));
	}

	LRValue& LRValue::setLocation(const LocationInfo& info)
	{
		locInfo_lineBegin = info.locInfo_lineBegin;
		locInfo_lineEnd = info.locInfo_lineEnd;
		locInfo_posBegin = info.locInfo_posBegin;
		locInfo_posEnd = info.locInfo_posEnd;
		return *this;
	}

	bool LRValue::isValid() const
	{
		return IsType<Address>(value)
			? As<Address>(value).isValid()
			: true; //EitherReference/Reference/Val は常に有効であるものとする
	}

	boost::optional<Address> LRValue::deref(const Context& env)const
	{
		if (isRValue())
		{
			return boost::none;
		}

		return address(env);
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
		else if (isEitherReference())
		{
			return std::string("EitherReference(") + As<EitherReference>(value).toString() + std::string(")");
		}
		else
		{
			return std::string("Val(...)");
		}
	}

	std::string LRValue::toString(Context& context) const
	{
		if (isAddress())
		{
			return std::string("Address(") + As<Address>(value).toString() + std::string(")");
		}
		else if (isReference())
		{
			return std::string("Reference(") + As<Reference>(value).toString() + std::string(")");
		}
		else if (isEitherReference())
		{
			return std::string("EitherReference(") + As<EitherReference>(value).toString() + std::string(")");
		}
		else
		{
			return std::string("Val(...)");
		}
	}

	Address LRValue::address(const Context & context) const
	{
		return IsType<Address>(value)
			? As<Address>(value)
			: (IsType<EitherReference>(value)
				? As<EitherReference>(value).replaced
				: context.getReference(As<Reference>(value)));
	}

	Address LRValue::getReference(const Context& context)const
	{
		return context.getReference(As<Reference>(value));
	}

	Address LRValue::makeTemporaryValue(Context& context)const
	{
		return context.makeTemporaryValue(evaluated());
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
#ifdef USE_IMPORT
		const std::string u8FilePath = Unicode::UTF32ToUTF8(filePath);
		const auto path = cgl::filesystem::path(u8FilePath);

		CGL_DBG1(std::string("import path: \"") + u8FilePath + "\"");

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

			CGL_DBG1(std::string("canonical path: \"") + pathStr + "\"");

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
#else
		CGL_Error("Filesystem is disabled.");
#endif
	}

	Import::Import(const std::u32string& path, const Identifier& name):
		Import(path)
	{
		importName = name;
		updateHash();
	}

	LRValue Import::eval(std::shared_ptr<Context> pContext)const
	{
#ifdef USE_IMPORT
		auto it = importedParseTrees.find(seed);
		if (it == importedParseTrees.end() || !it->second)
		{
			CGL_Error("ファイルのimportに失敗");
		}

		const auto& expr = it->second.get();

		//通常のインポート
		//import filename
		if (importName.empty())
		{
			return ExecuteProgramWithRec(expr, pContext);
		}
		//修飾付きインポート
		//import filename as name
		//トップレベルの代入式をレコードの要素に包んで評価する
		else
		{
			const Expr importParseTree = BinaryExpr(Identifier(importName), ToImportForm(expr), BinaryOp::Assign);
			//printExpr(importParseTree, pContext, std::cout);
			return ExecuteProgramWithRec(importParseTree, pContext);
		}
#else
		CGL_Error("Import is disabled.");
#endif
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

	//Expr BuildShapeExpander(const Accessor& accessor)
	//{
	//	Expr expr;
	//	//expr = LRValue(CharString(str32));
	//	//Accessor callFunction;
	//	//callFunction.AppendFunction(FunctionAccess());
	//	//FunctionAccess f;

	//	/*FuncVal({},
	//		MakeRecordConstructor(
	//			Identifier("line"), MakeListConstractor(
	//				MakeRecordConstructor(Identifier("x"), Expr(LRValue(minX)), Identifier("y"), Expr(LRValue(minY))),
	//				MakeRecordConstructor(Identifier("x"), Expr(LRValue(maxX)), Identifier("y"), Expr(LRValue(minY)))
	//			)
	//		)*/
	//	return expr;
	//}

#ifdef commentout
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
#endif

	class ConstraintProblem : public cppoptlib::Problem<double>
	{
	public:
		using typename cppoptlib::Problem<double>::TVector;

		std::function<double(const TVector&)> evaluator;
		Record originalRecord;
		std::vector<Identifier> keyList;
		std::shared_ptr<Context> pEnv;

		double beginTime;

		bool callback(const cppoptlib::Criteria<cppoptlib::Problem<double>::Scalar>& state, const TVector& x) override;

		double value(const TVector &x) override
		{
			return evaluator(x);
		}
	};

	bool ConstraintProblem::callback(const cppoptlib::Criteria<cppoptlib::Problem<double>::Scalar> &state, const TVector &x)
	{
		if (!pEnv->hasTimeLimit())
		{
			return true;
		}

		if (pEnv->timeLimit() < GetSec() - beginTime)
		{
			return false;
		}

		return true;
	}

	void OptimizationProblemSat::addUnitConstraint(const Expr& logicExpr)
	{
		if (expr)
		{
			expr = BinaryExpr(expr.get(), logicExpr, BinaryOp::And);
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

		//if (!expr || freeVariableRefs.empty())
		if (!expr)
		{
			CGL_DBG1("Warning: constraint expression is empty.");
			return;
		}
		//CGL_DBG1("Expr: ");
		//printExpr2(expr.get(), pEnv, std::cout);
		if (freeVariableRefs.empty())
		{
			CGL_DBG1("Warning: free variable set in constraint is empty.");
			return;
		}

		std::unordered_set<Address> appearingList;

		/*CGL_DBG1("freeVariables:");
		for (const auto& val : freeVariableRefs)
		{
			CGL_DBG1(std::string("  Address(") + val.first.toString() + ")");
		}*/

		std::vector<char> usedInSat(freeVariableRefs.size(), 0);

		SatVariableBinder binder(pEnv, freeVariableRefs, usedInSat, refs, appearingList, invRefs, hasPlateausFunction);

		/*CGL_DBG1("appearingList:");
		for (const auto& a : appearingList)
		{
			CGL_DBG1(std::string("  Address(") + a.toString() + ")");
		}*/

		if (boost::apply_visitor(binder, expr.get()))
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
	}

	bool OptimizationProblemSat::initializeData(std::shared_ptr<Context> pEnv)
	{
		data.resize(refs.size());

		for (size_t i = 0; i < data.size(); ++i)
		{
			const auto opt = pEnv->expandOpt(LRValue(refs[i]));
			if (!opt)
			{
				CGL_Error("参照エラー");
			}
			const Val& val = opt.get();
			if (auto opt = AsOpt<double>(val))
			{
				CGL_DebugLog(ToS(i) + " : " + ToS(opt.get()));
				data[i] = opt.get();
			}
			else if (auto opt = AsOpt<int>(val))
			{
				CGL_DebugLog(ToS(i) + " : " + ToS(opt.get()));
				data[i] = opt.get();
			}
			else
			{
				CGL_Error("存在しない参照をsatに指定した");
			}
		}

		return true;
	}

	std::vector<double> OptimizationProblemSat::solve(std::shared_ptr<Context> pEnv, const LocationInfo& info, const Record currentRecord, const std::vector<Identifier>& currentKeyList)
	{
		std::cerr << "OptimizationProblemSat::solve : " << std::endl;
		printExpr2(expr.get(), pEnv, std::cerr);		

		constructConstraint(pEnv);
		CGL_DBG1(std::string("Current constraint freeVariablesSize: ") + ToS(freeVariableRefs.size()));

		/*if (isDebugMode && expr)
		{
			std::ofstream graphFile;
			graphFile.open("constraint_CFG.dot");
			MakeGraph(*pEnv, expr.get(), graphFile);
		}*/

		std::ofstream logger;
		if (isDebugMode)
		{
			logger.open("optimize_log.cgl");

			for (const auto& ref : freeVariableRefs)
			{
				CGL_DBG1(pEnv->makeLabel(ref.address));
			}

			logger << "{\n";
			logger << "\tsize: " << freeVariableRefs.size() << "\n";
			logger << "\tlabels: [\n";
			for (const auto& ref : freeVariableRefs)
			{
				logger << "\t\t\"" << pEnv->makeLabel(ref.address) << "\"\n";
			}
			logger << "\t]\n";

			logger << "\tdata: [\n";
		}

		/*{
			std::stringstream ss;
			for (const auto& r : optimizeRegions)
			{
				ss << "index(" << r.startIndex << "," << (r.startIndex + r.numOfIndices) << "), ";
			}
			CGL_DBG1(ss.str());
		}*/

		std::vector<Interval> rangeList;
		{
			for (const auto& r: optimizeRegions)
			{
				//std::cout << "index(" << r.startIndex << "," << (r.startIndex + r.numOfIndices) << ")\n";

				if (IsType<PackedVal>(r.region))
				{
					const auto& val = As<PackedVal>(r.region);

					//varに範囲指定がないときはmakePackedRangesを通るときに仮として0が設定されている。
					if (IsType<int>(val))
					{
						for (const Address address : r.addresses)
						{
							auto it = std::find_if(freeVariableRefs.begin(), freeVariableRefs.end(), 
								[&](const RegionVariable& regionVariable) {
								return regionVariable.address == address;
							});

							if (it == freeVariableRefs.end())
							{
								CGL_Error("恐らく Evaluator.cpp の maskedRegionVariables() のバグ");
							}

							double minVal;
							double maxVal;
							if (it->has(RegionVariable::Position))
							{
								minVal = -1000;
								maxVal = +1000;
							}
							else if (it->has(RegionVariable::Scale))
							{
								minVal = 1.e-3;
								maxVal = 1.e+3;
							}
							else if (it->has(RegionVariable::Angle))
							{
								minVal = -180;
								maxVal = +180;
							}
							else if (it->has(RegionVariable::Other))
							{
								minVal = -1.e+6;
								maxVal = +1.e+6;
							}
							else
							{
								CGL_Error("不明な属性");
							}

							rangeList.push_back(Interval(minVal, maxVal));
						}

						/*for (int i = 0; i < r.numOfIndices; ++i)
						{
							const int currentIndex = r.startIndex + i;

							double minVal;
							double maxVal;
							if (freeVariableRefs[currentIndex].has(RegionVariable::Position))
							{
								minVal = -1000;
								maxVal = +1000;
							}
							else if (freeVariableRefs[currentIndex].has(RegionVariable::Scale))
							{
								minVal = 1.e-3;
								maxVal = 1.e+3;
							}
							else if (freeVariableRefs[currentIndex].has(RegionVariable::Angle))
							{
								minVal = -180;
								maxVal = +180;
							}
							else if (freeVariableRefs[currentIndex].has(RegionVariable::Other))
							{
								minVal = -1.e+6;
								maxVal = +1.e+6;
							}
							else
							{
								CGL_Error("不明な属性");
							}

							rangeList.push_back(Interval(minVal, maxVal));
						}*/
					}
					else if (IsType<PackedRecord>(val))
					{
						const auto& shapeRegion = As<PackedRecord>(val);
						const auto& values = shapeRegion.values;
						if (values.find("pos") == values.end() ||
							values.find("scale") == values.end() ||
							values.find("angle") == values.end())
						{
							CGL_Error("範囲の型が不正");
						}
						const auto bb = GetBoundingBox(shapeRegion, pEnv);
						const auto minRecord = As<PackedRecord>(bb.values.find("min")->second.value);
						const auto maxRecord = As<PackedRecord>(bb.values.find("max")->second.value);

						const double minX = AsDouble(minRecord.values.find("x")->second.value);
						const double minY = AsDouble(minRecord.values.find("y")->second.value);
						const double maxX = AsDouble(maxRecord.values.find("x")->second.value);
						const double maxY = AsDouble(maxRecord.values.find("y")->second.value);

						//TODO: ちゃんとインデックスを見て対応付ける
						//現在はvarはVec2のみでx,yの順に並んでいると仮定している
						rangeList.push_back(Interval(minX, maxX));
						rangeList.push_back(Interval(minY, maxY));
					}
					else if (IsType<PackedList>(val))
					{
						const auto& intervalRegion = As<PackedList>(val);
						if (intervalRegion.data.size() != 2 ||
							!IsNum(intervalRegion.data[0].value) ||
							!IsNum(intervalRegion.data[1].value))
						{
							CGL_Error("範囲の型が不正");
						}
						const double minV = AsDouble(intervalRegion.data[0].value);
						const double maxV = AsDouble(intervalRegion.data[1].value);

						/*for (int i = 0; i < r.numOfIndices; ++i)
						{
							rangeList.push_back(Interval(minV, maxV));
						}*/

						for (const Address address : r.addresses)
						{
							rangeList.push_back(Interval(minV, maxV));
						}
					}
					else
					{
						CGL_Error("範囲の型が不正");
					}
				}
				else
				{
					CGL_Error("範囲の型が不正");
				}
			}
		}

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

					if (ref1.address == ref2)
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
			if (hasPlateausFunction/*,false*/)
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
					pEnv->enterScope();
					double result = eval(pEnv, info);
					pEnv->exitScope();
					pEnv->switchBackScope();

					CGL_DebugLog(std::string("cost: ") + ToS(result, 17));

					return result;
				};

				std::vector<double> x0(freeVariableRefs.size());
				for (int i = 0; i < x0.size(); ++i)
				{
					x0[i] = data[variable2Data[i]];
					CGL_DebugLog(ToS(i) + " : " + ToS(x0[i]));
				}

				const double sigma = 0.1;

				const int lambda = 100;

				libcmaes::CMAParameters<> cmaparams(x0, sigma, lambda, 1);

				if (pEnv->hasTimeLimit())
				{
					cmaparams.set_max_calc_time(pEnv->timeLimit());
					cmaparams.set_current_time(GetSec());
				}

				libcmaes::CMASolutions cmasols = libcmaes::cmaes<>(func, cmaparams);
				resultxs = cmasols.best_candidate().get_x();

				std::cout << "solved\n";
			}
			else if(true)
			{
				std::cout << "Solve constraint by BFGS...\n";

				ConstraintProblem constraintProblem;
				constraintProblem.evaluator = [&](const ConstraintProblem::TVector& v)->double
				{
					for (int i = 0; i < v.size(); ++i)
					{
						update(variable2Data[i], v[i]);
					}

					for (const auto& keyval : invRefs)
					{
						pEnv->TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(keyval.first, data[keyval.second]);
					}

					pEnv->switchFrontScope();
					pEnv->enterScope();
					double result = eval(pEnv, info);
					pEnv->exitScope();
					pEnv->switchBackScope();

					//CGL_DebugLog(std::string("cost: ") + ToS(result, 17));
					//std::cout << std::string("cost: ") << ToS(result, 17) << "\n";
					return result*result;
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

				constraintProblem.beginTime = GetSec();

				cppoptlib::BfgsSolver<ConstraintProblem> solver;
				solver.minimize(constraintProblem, x0s);

				resultxs.resize(x0s.size());
				for (int i = 0; i < x0s.size(); ++i)
				{
					resultxs[i] = x0s[i];
				}
			}
			else if(true)
			{
				std::cout << "Solve constraint by Random Search...\n";

				const auto targetFunc = [&](const std::vector<double>& v)->double
				{
					for (int i = 0; i < v.size(); ++i)
					{
						update(variable2Data[i], v[i]);
					}

					for (const auto& keyval : invRefs)
					{
						pEnv->TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(keyval.first, data[keyval.second]);
					}

					double result;

					pEnv->switchFrontScope();
					pEnv->enterScope();
					try
					{
						result = eval(pEnv, info);
					}
					catch (std::exception& e)
					{
						std::cout << "Eval: " << e.what() << std::endl;
						throw;
					}
					pEnv->exitScope();
					pEnv->switchBackScope();

					if (isDebugMode)
					{
						logger << "[";
						for (int i = 0; i < v.size(); ++i)
						{
							logger << v[i] << ", ";
						}
						logger << result << "], ";
					}

					//CGL_DebugLog(std::string("cost: ") + ToS(result, 17));
					return result;
				};

				std::vector<double> answer(freeVariableRefs.size());
				for (int i = 0; i < answer.size(); ++i)
				{
					answer[i] = data[variable2Data[i]];
				}

				double beginTime = GetSec();

				{
					double minimumCost = targetFunc(answer);
					std::vector<double> current(answer.size());
					if (current.size() != rangeList.size())
					{
						CGL_Error("範囲と変数の数が対応していない");
					}

					std::vector<std::uniform_real_distribution<double>> dists;
					for (size_t i = 0; i < rangeList.size(); ++i)
					{
						std::cout << "Range(" << i << "): [" << rangeList[i].minimum << ", " << rangeList[i].maximum << "]" << std::endl;
						dists.emplace_back(rangeList[i].minimum, rangeList[i].maximum);
					}

					int count = 0;
					std::mt19937 rng;
					//while (GetSec() - beginTime < 300.0)
					//while (count < 20000)
					//while (count < 6900)
					while(count < 20000)
					{
						cloneTime = 0.0;
						cloneCount = 0;
						/*if (6660 < count)
						{
							printAddressInsertion = true;
							std::cout << "----------------------------------\n";
							std::cout << count << ": ";
						}*/
						for (size_t i = 0; i < current.size(); ++i)
						{
							current[i] = dists[i](rng);
							if (printAddressInsertion)
							{
								std::cout << current[i] << ", ";
							}
						}

						if (printAddressInsertion)
						{
							std::cout << "\n";
						}

						const double currentCost = targetFunc(current);
						if (currentCost < minimumCost)
						{
							minimumCost = currentCost;
							answer = current;
						}
						if (printAddressInsertion)
						{
							std::cout << "cloneTime: " << cloneTime << " | " << cloneCount << "\n";
						}

						++count;

						if (count % 1000 == 0)
							//if (count % 10 == 0)
						{
							std::cout <<"it("<< count <<") | "<< "cloneTime: " << cloneTime << ", cloneCount: " << cloneCount << "\n";
							pEnv->garbageCollect(true);
						}
					}

					if (isDebugMode)
					{
						logger << "[";
						for (int i = 0; i < answer.size(); ++i)
						{
							logger << answer[i] << ", ";
						}
						logger << minimumCost << "]\n";

						logger << "\t]\n";
						logger << "}\n";
					}
				}

				resultxs.resize(answer.size());
				for (int i = 0; i < answer.size(); ++i)
				{
					resultxs[i] = answer[i];
				}
			}
			/*else if (true)
			{
				std::cout << "Solve constraint by nlopt...\n";

				auto targetFunc = [&](unsigned N, const double *x, double *grad, void *my_func_data)
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
					pEnv->enterScope();
					double result = eval(pEnv, info);
					pEnv->exitScope();
					pEnv->switchBackScope();

					CGL_DebugLog(std::string("cost: ") + ToS(result, 17));

					return result;
				};

				std::vector<double> lb, ub;
				for (size_t i = 0; i < rangeList.size(); ++i)
				{
					lb.push_back(rangeList[i].minimum);
					ub.push_back(rangeList[i].maximum);
				}

				nlopt_opt opt;
				opt = nlopt_create(NLOPT_LD_MMA, freeVariableRefs.size());
				nlopt_set_lower_bounds(opt, lb.data());
				nlopt_set_upper_bounds(opt, ub.data());
				nlopt_set_min_objective(opt, targetFunc, NULL);

				std::vector<double> xs(freeVariableRefs.size());
				for (int i = 0; i < xs.size(); ++i)
				{
					xs[i] = data[variable2Data[i]];
				}
			}*/
#ifdef USE_OPTIONAL_LIB
			else
			{
				std::cout << "Solve constraint by Limbo...\n";

				const auto targetFunc = [&](const Eigen::VectorXd& x)->double
				{
					for (int i = 0; i < x.size(); ++i)
					{
						//std::cout << (x[i] * (rangeList[i].maximum - rangeList[i].minimum) + rangeList[i].minimum) << ", ";
						update(variable2Data[i], x[i] * (rangeList[i].maximum - rangeList[i].minimum) + rangeList[i].minimum);
					}
					//std::cout << "\n";

					for (const auto& keyval : invRefs)
					{
						pEnv->TODO_Remove__ThisFunctionIsDangerousFunction__AssignToObject(keyval.first, data[keyval.second]);
					}

					double result;

					pEnv->switchFrontScope();
					pEnv->enterScope();
					try
					{
						result = eval(pEnv, info);
					}
					catch (std::exception& e)
					{
						std::cout << "Eval: " << e.what() << std::endl;
						throw;
					}
					pEnv->exitScope();
					pEnv->switchBackScope();

					//limbo maximizes target function
					return -result;
				};

				//*
				using Kernel_t = limbo::kernel::MaternFiveHalves<Params>;
				using Mean_t = limbo::mean::Data<Params>;

				using gp_opt_t = limbo::model::gp::KernelLFOpt<Params>;
				using GP_t = limbo::model::GP<Params, Kernel_t, Mean_t, gp_opt_t>;
				//using GP_t = limbo::model::GP<Params, Kernel_t, Mean_t>;
				
				//using Acqui_t = limbo::acqui::UCB<Params, GP_t>;
				using Acqui_t = limbo::acqui::EI<Params, GP_t>;
				
				using stat_t = boost::fusion::vector<limbo::stat::ConsoleSummary<Params>,
					limbo::stat::Samples<Params>,
					limbo::stat::Observations<Params>,
					limbo::stat::GP<Params>>;

				limbo::bayes_opt::BOptimizer<Params, limbo::modelfun<GP_t>, limbo::statsfun<stat_t>, limbo::acquifun<Acqui_t>> opt;
				//*/

				// example with basic HP opt
				//limbo::bayes_opt::BOptimizerHPOpt<Params> opt;

				LimboFitFunc target;

				target.numOfVars = freeVariableRefs.size();
				target.func = targetFunc;

				opt.optimize2(target);

				/*std::cout << opt.best_observation() << " res  "
					<< opt.best_sample().transpose() << std::endl;*/

				//const auto answer = opt.best_observation();
				const auto answer = opt.best_sample();
				resultxs.resize(answer.size());
				for (int i = 0; i < answer.size(); ++i)
				{
					std::cout << "answer[" << i << "]: " << answer[i] << "\n";
					resultxs[i] = answer[i] * (rangeList[i].maximum - rangeList[i].minimum) + rangeList[i].minimum;
				}
			}
#endif
		}

		return resultxs;
	}

	double OptimizationProblemSat::eval(std::shared_ptr<Context> pEnv, const LocationInfo& info)
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
			printExpr(expr.get());
		}*/
		
		EvalSatExpr evaluator(pEnv, data, refs, invRefs);
		const Val evaluated = pEnv->expand(boost::apply_visitor(evaluator, expr.get()), info);

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

	//値同士が等しいかを知りたいのでAddressはハッシュには含めない
	class ValueHasher : public boost::static_visitor<size_t>
	{
	public:
		ValueHasher() = default;

		size_t operator()(bool node)const { return std::hash<bool>()(node); }
		size_t operator()(int node)const { return std::hash<int>()(node); }
		size_t operator()(double node)const { return std::hash<double>()(node); }
		size_t operator()(const CharString& node)const { return std::hash<std::u32string>()(node.toString()); }
		size_t operator()(const PackedList& node)const
		{
			size_t result = 0;
			for (const auto& val : node.data)
			{
				boost::hash_combine(result, boost::apply_visitor(*this, val.value));
			}
			return result;
		}
		size_t operator()(const KeyValue& node)const
		{
			return 0;
		}
		size_t operator()(const PackedRecord& node)const
		{
			size_t result = 0;
			for (const auto& keyval : node.values)
			{
				boost::hash_combine(result, std::hash<std::string>()(keyval.first));
				boost::hash_combine(result, boost::apply_visitor(*this, keyval.second.value));
			}
			return result;
		}
		size_t operator()(const FuncVal& node)const
		{
			return 0;
		}
		size_t operator()(const Jump& node)const
		{
			return 0;
		}
	};

	size_t GetHash(const PackedVal& val)
	{
		ValueHasher hasher;
		return boost::apply_visitor(hasher, val);
	}

	size_t GetHash(const Val& val, const Context& context)
	{
		const PackedVal& packed = Packed(val, context);
		return GetHash(packed);
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
			const auto& opt = context.expandOpt(LRValue(address));
			if (!opt)
			{
				std::stringstream ss;
				ss << "リスト中の " << LRValue(address).toString() << " 参照に失敗しました。";
				CGL_Error("参照エラー: ");
			}
			const PackedVal packedValue = boost::apply_visitor(packer, opt.get());

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
		result.boundedFreeVariables = freeVariables;
		//result.freeVariableRefs = freeVariableRefs;
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
			const auto& opt = context.expandOpt(LRValue(keyval.second));
			if (!opt)
			{
				std::stringstream ss;
				ss << "レコード中のキー \"" << keyval.first << "\": " << LRValue(keyval.second).toString() << " の参照に失敗しました。";
				CGL_Error("参照エラー: " + ss.str());
			}
			const PackedVal packedValue = boost::apply_visitor(packer, opt.get());

			result.add(keyval.first, keyval.second, packedValue);
		}

		result.problems = problems;
		result.freeVariables = boundedFreeVariables;
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

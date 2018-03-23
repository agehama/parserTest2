#pragma once
#include <Eigen/Core>

#include "Evaluator.hpp"
#include "Node.hpp"
#include "Context.hpp"

namespace cgl
{
	class Program
	{
	public:
		Program() :
			pEnv(Context::Make()),
			evaluator(pEnv)
		{}

		//boost::optional<Val> execute(const std::string& program);

		//bool draw(const std::string& program, bool logOutput = true);

		void execute1(const std::string& input_filename, const std::string& output_filename, bool logOutput = true);
		//void run(const std::string& program, bool logOutput = true);

		void clear();

		bool test(const std::string& input_filepath, const Expr& expr);

		std::shared_ptr<Context> getContext()
		{
			return pEnv;
		}

		boost::optional<Val>& getVal()
		{
			return evaluated;
		}

		bool isSucceeded()const
		{
			return succeeded;
		}

		boost::optional<int> asIntOpt();
		boost::optional<double> asDoubleOpt();

	private:
		void printErrorPos(const std::string& input_filepath, const LocationInfo& locationInfo);

		std::shared_ptr<Context> pEnv;
		Eval evaluator;
		boost::optional<Val> evaluated;
		bool succeeded;
	};
}

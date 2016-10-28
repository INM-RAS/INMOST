#include "SolverMPTILUC.h"

namespace INMOST {

    SolverMPTILUC::SolverMPTILUC() {
        Method *preconditioner = new MTILUC_preconditioner(info);
        solver = new KSOLVER(preconditioner, info);
        matrix = NULL;
    }

    SolverMPTILUC::SolverMPTILUC(const SolverInterface *other) {
        //You should not really want to copy solver's information
        throw INMOST::SolverUnsupportedOperation;
    }

    void SolverMPTILUC::SetMatrix(Sparse::Matrix &A, bool ModifiedPattern, bool OldPreconditioner) {
        if (matrix != NULL) {
            delete matrix;
        }
        matrix = new Sparse::Matrix(A);
        info.PrepareMatrix(*matrix, parameters.get<INMOST_DATA_ENUM_TYPE>("additive_schwartz_overlap"));
        solver->ReplaceMAT(*matrix);

        solver->RealParameter(":tau") = parameters.get<INMOST_DATA_REAL_TYPE>("drop_tolerance");
        solver->RealParameter(":tau2") = parameters.get<INMOST_DATA_REAL_TYPE>("reuse_tolerance");
        solver->EnumParameter(":scale_iters") = parameters.get<INMOST_DATA_ENUM_TYPE>("rescale_iterations");
        solver->EnumParameter(":estimator") = parameters.get<INMOST_DATA_ENUM_TYPE>("condition_estimation");

        if (sizeof(KSOLVER) == sizeof(BCGSL_solver)) {
            solver->EnumParameter("levels") = parameters.get<INMOST_DATA_ENUM_TYPE>("gmres_substeps");
        }

        if (!solver->isInitialized()) {
            solver->Initialize();
        }
    }

    bool SolverMPTILUC::Solve(Sparse::Vector &RHS, Sparse::Vector &SOL) {
        solver->EnumParameter("maxits") = parameters.get<INMOST_DATA_ENUM_TYPE>("maximum_iterations");
        solver->RealParameter("rtol") = parameters.get<INMOST_DATA_REAL_TYPE>("relative_tolerance");
        solver->RealParameter("atol") = parameters.get<INMOST_DATA_REAL_TYPE>("absolute_tolerance");
        solver->RealParameter("divtol") = parameters.get<INMOST_DATA_REAL_TYPE>("divergence_tolerance");

        bool solve = solver->Solve(RHS, SOL);
        parameters.SetParameter("condition_number_L", std::to_string(solver->RealParameter(":condition_number_L")));
        parameters.SetParameter("condition_number_U", std::to_string(solver->RealParameter(":condition_number_U")));
        return solve;
    }

    const std::string SolverMPTILUC::SolverName() const {
        return "inner_mptiluc";
    }

    SolverMPTILUC::~SolverMPTILUC() {

    }

}
#ifndef INMOST_SOLVERINNER_H
#define INMOST_SOLVERINNER_H

#include "inmost_solver_interface.h"
#include "inmost_utils.h"
#include "solver_prototypes.hpp"
#include "solver_bcgsl.hpp"


namespace INMOST {

    class SolverInner : public SolverInterface {
    protected:
        Sparse::Matrix *matrix;
        KSOLVER *solver;
        Solver::OrderInfo info;

        INMOST_DATA_ENUM_TYPE maximum_iterations, rescale_iterations, condition_estimation, adapt_ddpq_tolerance, schwartz_overlap, gmres_substeps, reorder_nnz, fill_level;
        INMOST_DATA_REAL_TYPE atol, rtol, dtol, ddpq_tolerance, drop_tolerance, reuse_tolerance;
    public:
        SolverInner();

        SolverInner(const SolverInterface *other);

        virtual void Assign(const SolverInterface *other);

        virtual void Setup(int *argc, char ***argv, SolverParameters &p);

        virtual void SetMatrix(Sparse::Matrix &A, bool ModifiedPattern, bool OldPreconditioner) = 0;

        virtual bool Solve(Sparse::Vector &RHS, Sparse::Vector &SOL);

        virtual bool Clear();

        virtual bool isMatrixSet();

        virtual std::string GetParameter(std::string name) const;

        virtual void SetParameter(std::string name, std::string value);

        virtual const INMOST_DATA_ENUM_TYPE Iterations() const;

        virtual const INMOST_DATA_REAL_TYPE Residual() const;

        virtual const std::string ReturnReason() const;

        virtual const std::string SolverName() const = 0;

        virtual const INMOST_DATA_REAL_TYPE Condest(INMOST_DATA_REAL_TYPE tol, INMOST_DATA_ENUM_TYPE maxiter);

        virtual void Finalize();

        virtual ~SolverInner();

    };

}


#endif //INMOST_SOLVERINNER_H

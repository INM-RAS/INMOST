#ifndef INMOST_SOLVERFCBIILU2_H
#define INMOST_SOLVERFCBIILU2_H

#include "Source/Solver/SolverInterface.h"
#include "solver_fcbiilu2.h"

namespace INMOST {

    class SolverFCBIILU2: public SolverInterface {
    private:
        bcg_fcbiilu2 *solver_data;
        matrix_fcbiilu2 *matrix_data;
        INMOST_DATA_ENUM_TYPE local_size, global_size;

        double time_prec;
        double iter_time;
    public:
        SolverFCBIILU2();

        virtual SolverInterface *Copy(const SolverInterface *other);

        virtual void Assign(const SolverInterface *other);

        virtual void Setup(int *argc, char ***argv, SolverParameters &p);

        virtual void SetMatrix(Sparse::Matrix &A, bool ModifiedPattern, bool OldPreconditioner);

        virtual bool Solve(INMOST::Sparse::Vector &RHS, INMOST::Sparse::Vector &SOL);

        virtual bool Clear();

        virtual bool isMatrixSet();

        virtual std::string GetParameter(std::string name) const;

        virtual void SetParameter(std::string name, std::string value);

        virtual const INMOST_DATA_ENUM_TYPE Iterations() const;

        virtual const INMOST_DATA_REAL_TYPE Residual() const;

        virtual const std::string ReturnReason() const;

        virtual const std::string SolverName() const;

        virtual void Finalize();

        virtual ~SolverFCBIILU2();
    };

}




#endif //INMOST_SOLVERFCBIILU2_H

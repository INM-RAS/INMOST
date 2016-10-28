#ifndef INMOST_SOLVERSUPERLU_H
#define INMOST_SOLVERSUPERLU_H

#include "inmost_solver_interface.h"
#include "superlu/slu_ddefs.h"

namespace INMOST {

    class SolverSUPERLU : public SolverInterface {
    private:
        SuperMatrix A, L, U;
        int * perm_r;
        int * perm_c;
        int * remap;
        int a_size, info;
        superlu_options_t options;
        SuperLUStat_t stat;
    public:
        SolverSUPERLU();

        SolverSUPERLU(const SolverInterface *other);

        virtual void Assign(const SolverInterface *other);

        virtual void Initialize(int *argc, char ***argv, const char *parameters_file, std::string prefix);

        virtual void SetMatrix(Sparse::Matrix &A, bool ModifiedPattern, bool OldPreconditioner);

        virtual bool Solve(INMOST::Sparse::Vector &RHS, INMOST::Sparse::Vector &SOL);

        virtual bool Clear();

        virtual bool isMatrixSet();

        virtual void SetDefaultParameters();

        virtual SolverParameter GetParameter(std::string name) const;

        virtual void SetParameter(std::string name, std::string value);

        virtual const INMOST_DATA_ENUM_TYPE Iterations() const;

        virtual const INMOST_DATA_REAL_TYPE Residual() const;

        virtual const std::string ReturnReason() const;

        virtual const std::string SolverName() const;

        virtual void Finalize();

        virtual ~SolverSUPERLU();

    };

}


#endif //INMOST_SOLVERSUPERLU_H
//
// Created by Dmitri Bagaev on 30.09.16.
//

#ifndef INMOST_SOLVERTRILINOS_H
#define INMOST_SOLVERTRILINOS_H


#include <inmost.h>

#if defined(USE_MPI)

#include "Epetra_MpiComm.h"

#else
#include "Epetra_SerialComm.h"
#endif

#include "Epetra_Map.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "AztecOO.h"
#include "Ifpack.h"
#include "ml_include.h"
#include "ml_MultiLevelPreconditioner.h"
#include "BelosEpetraOperator.h"
#include "Teuchos_Comm.hpp"
#include "Teuchos_DefaultMpiComm.hpp"
#include "Teuchos_OpaqueWrapper.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_TestForException.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"

namespace INMOST {

    class SolverTrilinos : public SolverInterface {
    protected:
        std::pair<std::string, Epetra_LinearProblem> *Epetra_problem;
        Epetra_CrsMatrix *matrix;
        INMOST_DATA_ENUM_TYPE local_size, global_size;
        std::string parameters_file;
        INMOST_DATA_ENUM_TYPE lastIterations;
        INMOST_DATA_REAL_TYPE lastResidual;
        std::string returnReason;

        void TrilinosCheckStatus(int status_id, bool &success, std::string &reason);

        template <typename T>
        std::string to_string(T const& value) {
            std::stringstream sstr;
            sstr << value;
            return sstr.str();
        }

    public:
        SolverTrilinos();

        SolverTrilinos(const SolverInterface *other);

        virtual void Assign(const SolverInterface *other);

        virtual void Initialize(int *argc, char ***argv, const char *parameters_file, std::string prefix);

        virtual void SetMatrix(Sparse::Matrix &A, bool ModifiedPattern, bool OldPreconditioner);

        virtual bool Solve(INMOST::Sparse::Vector &RHS, INMOST::Sparse::Vector &SOL) = 0;

        virtual bool Clear();

        virtual bool isMatrixSet();

        virtual void SetDefaultParameters();

        virtual const INMOST_DATA_ENUM_TYPE Iterations() const;

        virtual const INMOST_DATA_REAL_TYPE Residual() const;

        virtual const std::string ReturnReason() const;

        virtual const std::string SolverName() const = 0;

        virtual void Finalize();

        virtual ~SolverTrilinos();
    };

}


#endif //INMOST_SOLVERTRILINOS_H
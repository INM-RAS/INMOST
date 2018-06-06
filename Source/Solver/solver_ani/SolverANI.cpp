#include "SolverANI.h"

namespace INMOST {

    SolverANI::SolverANI() {

    };

    SolverInterface *SolverANI::Copy(const SolverInterface *other) {
        throw INMOST::SolverUnsupportedOperation; //later
        (void) other;
    };

    void SolverANI::Assign(const SolverInterface *other) {
        throw INMOST::SolverUnsupportedOperation; //later
        (void) other;
    }

    void SolverANI::Setup(int *argc, char ***argv, SolverParameters &p) {
        solver.n = 0;
        m.n = 0;
        (void) argc; (void) argv; (void) p;
    }

    void SolverANI::SetMatrix(Sparse::Matrix &A, bool ModifiedPattern, bool OldPreconditioner) {
        bool modified_pattern = ModifiedPattern;
        if (m.n != 0) {
            modified_pattern = true;
        }
        if (modified_pattern) {
            local_size = A.Size();

            if (m.n != 0) {
                free(m.ia);
                free(m.ja);
                free(m.A);
                m.n = 0;
            }
            INMOST_DATA_ENUM_TYPE nnz = 0, k = 0, q = 1, shift = 1;
            int n = A.Size();
            int *ia = (int *) malloc(sizeof(int) * (n + 1));
            for (Sparse::Matrix::iterator it = A.Begin(); it != A.End(); it++) nnz += it->Size();
            int *ja = (int *) malloc(sizeof(int) * nnz);
            double *values = (double *) malloc(sizeof(double) * nnz);
            ia[0] = shift;
            for (Sparse::Matrix::iterator it = A.Begin(); it != A.End(); it++) {
                for (Sparse::Row::iterator jt = it->Begin(); jt != it->End(); jt++) {
                    ja[k] = jt->first + 1;
                    values[k] = jt->second;
                    k++;
                }
                shift += it->Size();
                ia[q++] = shift;
            }
            m.n = n;
            m.ia = ia;
            m.ja = ja;
            m.A = values;
        } else {
            INMOST_DATA_ENUM_TYPE nnz = 0, k = 0;
            for (Sparse::Matrix::iterator it = A.Begin(); it != A.End(); it++) nnz += it->Size();
            double *values = (double *) malloc(sizeof(double) * nnz);
            k = 0;
            for (Sparse::Matrix::iterator it = A.Begin(); it != A.End(); it++)
                for (Sparse::Row::iterator jt = it->Begin(); jt != it->End(); jt++)
                    values[k++] = jt->second;
            free(m.A);
            m.A = values;
        }
        if (solver.n == 0)
            initbcg(&solver, &m);
        else
            newmatrixbcg(&solver, &m, OldPreconditioner);
    }

    bool SolverANI::Solve(INMOST::Sparse::Vector &RHS, INMOST::Sparse::Vector &SOL) {
        INMOST_DATA_ENUM_TYPE vbeg, vend;
        RHS.GetInterval(vbeg, vend);
        vector rhs, solution;

        rhs.n = local_size;
        rhs.v = (double *) malloc(sizeof(double) * local_size);
        memcpy(rhs.v, &RHS[vbeg], sizeof(double) * local_size);

        solution.n = local_size;
        solution.v = (double *) malloc(sizeof(double) * local_size);
        memcpy(solution.v, &SOL[vbeg], sizeof(double) * local_size);

        bool result = (solvebcg_ani(&solver, &rhs, &solution) == 0);
        if (result) {
            memcpy(&SOL[vbeg], solution.v, sizeof(double) * local_size);
        }
        return result;
    }

    bool SolverANI::Clear() {
        if (m.n != 0) {
            free(m.ia);
            free(m.ja);
            free(m.A);
            m.n = 0;
        }
        if (solver.n != 0) {
            free(solver.rW);
            free(solver.iW);
            solver.n = 0;
        }
        return true;
    }

    bool SolverANI::isMatrixSet() {
        return m.n != 0;
    }

    std::string SolverANI::GetParameter(std::string name) const {
        std::cout << "SolverANI::GetParameter unsupported operation" << std::endl;
        //throw INMOST::SolverUnsupportedOperation;
        return "";
    }

    void SolverANI::SetParameter(std::string name, std::string value) {
        std::cout << "SolverANI::SetParameter unsupported operation" << std::endl;
        //throw INMOST::SolverUnsupportedOperation;
    }

    INMOST_DATA_ENUM_TYPE SolverANI::Iterations() const {
        return static_cast<INMOST_DATA_ENUM_TYPE>(solver.ITER);
    }

    INMOST_DATA_REAL_TYPE SolverANI::Residual() const {
        return solver.RESID;
    }

    const std::string SolverANI::ReturnReason() const {
        return "Unspecified for ANI";
    }

    const std::string SolverANI::SolverName() const {
        return "ani";
    }

    void SolverANI::Finalize() {

    }

    SolverANI::~SolverANI() {
        this->Clear();
    }
}

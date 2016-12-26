#include "solver_k3biilu2.h"

//#if defined(USE_SOLVER_K3BIILU2)

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <string>

#include "k3d.h"

#define T(x) //x // Trace of function calls. Use: "T(x) x" for trace and "T(x)" for silence

//TODO: ncycle niter_cycle niter_cycle2 ncoef nit->maxit

typedef struct
{
    int    ittype;       // 0 - BiCGStab; 1,2,3 - GMRES(niter_cycle); 2 - +Poly(ncoef)_BiCGStab; 3 - +Poly(ncoef)_GMRESb(niter_cycle2)
    int    niter_cycle;  // outer GMRES cycle (=kgmr by IEK); 1 - BiCGStab
    int    ncoef;        // polynomial degree (=kdeg by IEK); ittype=2,3
    int    niter_cycle2; // internal GMRES cycle size for ittype=3
    double eps;          // the residual precision: ||r|| < eps * ||b||; eps=1e-6
    int    maxit;        // number of iterations permitted; maxit=999
    int    ichk;         // number of skipped iterations to check the convergence
    int    msglev;       // messages level; msglev=0 for silent; msglev>0 to output solution statistics
} ParIter;

static k3d::SSolverParams parPrec; // preconditioner construction parameters
static ParIter            parIter; // iterative solver parameters
static k3d::CSolver<int,double,double> k3_solver; // the solver object

void ParIterDefault ()
{
    parIter.ittype       = 0;    // 0 - BiCGStab; 1,2,3 - GMRES(niter_cycle); 2 - +Poly(ncoef)_BiCGStab; 3 - +Poly(ncoef)_GMRESb(niter_cycle2)
    parIter.niter_cycle  = 30;   // outer GMRES cycle (=kgmr by IEK); 1 - BiCGStab
    parIter.ncoef        = 5;    // polynomial degree (=kdeg by IEK); ittype=2,3
    parIter.niter_cycle2 = 4;    // internal GMRES cycle size for ittype=3
    parIter.eps          = 1e-6; // the residual precision: ||r|| < eps * ||b||; eps=1e-6
    parIter.maxit        = 999;  // number of iterations permitted; maxit=999
    parIter.ichk         = 5;    // number of skipped iterations to check the convergence
    parIter.msglev       = 2;    // messages level; msglev=0 for silent; msglev>0 to output solution statistics
}

/* BiCGStab solver structure */
typedef struct 
{
    int    n;         // local number of unknowns at the local processor
    int    nproc;     // total number of processors
    int    * ibl;     // block splitting: ibl[0]=0; ibl[nproc]=nglob
    int    * ia;      // row pointers: ia[0]=0; ia[nloc]=nzloc
    int    * ja;      // global column numbers (NOTE: starting from 0 or 1); ja[nzloc]
    double * a;       // nonzero coefficients of local matrix A; a[nzloc]
    k3d::SSolverParams              *pParams;  // preconditioner construction parameters
    ParIter                         *pParIter; // preconditioner construction parameters
    k3d::CSolver<int,double,double> *pSolver;  // pointer to the solver structure
    int    ierr;      // error flag on return; ierr=0 for success
    int    istat[16]; // integer statistics array on return
    double dstat[16]; // double  statistics array on return
    double RESID;     // residual norm
    int    ITER;      // number of BiCGStab iterations performed
} bcg;

typedef struct
{
    int    n;         // local number of unknowns at the local processor
    int    nproc;     // total number of processors
    int    * ibl;     // block splitting: ibl[0]=0; ibl[nproc]=nglob
    int    * ia;      // row pointers: ia[0]=0; ia[nloc]=nzloc
    int    * ja;      // global column numbers (NOTE: starting from 0 or 1); ja[nzloc]
    double * A;       // nonzero coefficients of local matrix A; a[nzloc]
} matrix;

typedef struct
{
    int    n;         // local number of unknowns at the local processor
    double * v;       // local data vector 
} Vector;

/*****************************************************************************/
#include <stdlib.h>   // for malloc()
#if defined (USE_MPI)
#include <mpi.h>      // for MPI_COMM_WORLD etc.
#endif

// Solve the linear system A X = B by
//  k3biilu2_bcg solver with working memory allocations and statistics output
int k3biilu2_bcg (
        int    *ibl,   // block splitting: ibl[0]=0; ibl[nproc]=n
        int    *ia,    // row pointers: ia[0]=0; ia[nloc]=nzloc
        int    *ja,    // column numbers (NOTE: starting from 0 or 1); ja[nzloc]
        double *a,     // matrix A coefficients; a[nzloc]
        double *b,     // right-hand side B; b[nloc]
        double *x,     // initial guess to the solution X on entry; solution X on return; x[nloc]
	int    job,    // job number: 0 - construct preconditioner; 1 - use the previously constructed one
        int    maxit,  // number of iterations permitted; maxit=999; if(maxit==0) preconditioner construction only; it is more important than one in pParIter
        k3d::SSolverParams              *pParams,  // preconditioner construction parameters
        ParIter                         *pParIter, // iterative solver parameters
	k3d::CSolver<int,double,double> *pSolver,  // pointer to the solver structure
        int    *ierr,  // error flag on return; ierr=0 for success
        int    *istat, //[16],  // integer statistics array on return
        double *dstat) //[16]); // double  statistics array on return
//
// Here, in notation:
//      nproc - the number of processors (or blocks), nproc is equal to the MPI communicator size
//      nzloc - the local number of nonzero elements, nzloc=ia[[myid+1]]-ia[ibl[myid]]
//      nloc  - the local number of unknowns at the current processor, nloc=ibl[myid+1]-ibl[myid]
//      n     - the total number of unknowns in matrix A, n=ibl[nproc]
//
// ON ENTRY:
//      ibl, ia, ja, a, b, x, job, maxit, pParams, pParIter, pSolver
// ON RETURN:
//      x, pSolver, ierr, istat, dstat
//
{
    // Initialize MPI variables
    INMOST_MPI_Comm comm = INMOST_MPI_COMM_WORLD;
    int np=1, mp=0;
#if defined (USE_MPI)
    MPI_Comm_size(MPI_COMM_WORLD, &np);
    MPI_Comm_rank(MPI_COMM_WORLD, &mp);
#endif

    std::vector<long long> blks (np+1);
    std::vector<int> blk2cpu (np+1);

    long long *pblks = &blks[0];
    int *pblk2cpu = &blk2cpu[0];

    for (int i=0; i<=np; i++) pblks[i]    = ibl[i];
    for (int i=0; i<np;  i++) pblk2cpu[i] = i;

    T(cout<<"HHHHH pSolver = "<<pSolver<<"\n";)//DB!

    if (mp != 0) pParIter->msglev = 0;

    if (job == 0) {
    } else {
        T(cout<<"HHHHH k3biilu2_bcg: job == 0\n";)//DB!
	if (pParams->tau2 < -0.01) pParams->tau2 = pParams->tau1 * pParams->tau1;
        int nmodif;
        double prec_extend, density, scpiv_min, scpiv_max, piv_min, piv_max, dtime_fct;
        bool b_store_matrix = true;
	//if (maxit > 0) b_store_matrix = true;
		//TODO PIVMIN??? -> AUX = 0  <vars> / AUX
		//pParams->pivmin=0.0;
        pSolver->PrepareSolver ((void *)&comm, pParams, np, pblks, pblk2cpu,
                                b_store_matrix, ia, ja, a,
                                prec_extend, density, scpiv_min, scpiv_max, nmodif, piv_min, piv_max, dtime_fct);

        if (pParIter->msglev > 0) std::cout << " K3: prec_extend=" << prec_extend << "  density=" << density << "  scpiv_min=" << scpiv_min << "  scpiv_max=" << scpiv_max << "  nmodif=" << nmodif << "  piv_min=" << piv_min << "  piv_max=" << piv_max << "  dtime_fct=" << dtime_fct << endl;
        dstat[0] = density;
//    } else {
//        T(cout<<"HHHHH k3biilu2_bcg: else (job == 0)\n";)//DB!
        //pSolver->PrepareMatrix((void *)&comm, np, pblks, pblk2cpu,
        //                       ia, ja, a);
    }

    if (maxit > 0) {
        T(cout<<"HHHHH k3biilu2_bcg: 0<maxit="<<maxit<<"\n";)//DB!
        ofstream *pfout = NULL;
        int niter, nmvm;
        double rhs_norm, res_ini, res_fin, dtime_iter;

        pSolver->SolveIter (pParIter->ittype, maxit, pParIter->niter_cycle, pParIter->ncoef, pParIter->niter_cycle2,
                            pParIter->eps, pParIter->ichk, pParIter->msglev, pfout,
                            b, x,
                            rhs_norm, res_ini, niter, nmvm, res_fin, dtime_iter);

        if (pParIter->msglev > 0) std::cout << " K3: rhs_norm=" << rhs_norm << "  res_ini=" << res_ini << "  niter=" << niter << "  nmvm=" << nmvm << "  res_fin=" << res_fin << "  dtime_iter=" << dtime_iter << endl;
        istat[2] = niter;
        dstat[2] = (rhs_norm == 0e0) ? res_fin : res_fin/rhs_norm;
        pSolver->CleanMvmA(); //? prec iter iter
    }
    
    *ierr = 0; //TODO correct error status of construction or convergence

    return *ierr;
}

/*****************************************************************************/

/* Initialize bcg solver */
static int initbcg(bcg *s, matrix *A, double eps);
/* Reinitialize solver preconditioner with new matrix A */
static int renewbcg(bcg *s, double *A);
/* Solve linear system */
/*static*/ int solvebcg(bcg *s, Vector *b, Vector *x);
/* Free memory used by solver */
static void freebcg(bcg *s);

/*****************************************************************************/

/* Initialize solver with new matrix A */
static int newmatrixbcg(bcg *s, matrix *A, bool same_precond) 
{
    if( s->n != A->n && same_precond ) throw INMOST::CannotReusePreconditionerOfDifferentSize;
    s->n     = A->n;
    s->nproc = A->nproc;
    s->ibl   = A->ibl;
    s->ia    = A->ia;
    s->ja    = A->ja;
    if( !same_precond )
    {
	//do nothing...
        T(std::cout<<"##### inside newmatrixbcg bef. renewbcg \n";)//db!
	return renewbcg(s, A->A);
    }
    else return 0;
}

/* solver */
/* Initialize bcg solver */
int initbcg(bcg *s, matrix *A, double eps) 
{
    parIter.eps = eps;
    T(std::cout<<"##### inside initbcg bef. newmatrixbcg eps="<<eps<<" \n";)//db!
    return newmatrixbcg(s, A, false);
}

/* Reinitialize solver preconditioner with new matrix A */
int renewbcg(bcg *s, double *A) 
{
    //reinitialize matrix values
    s->a = A;
    //BIILU2 preconditioner construction...
    s->pParams  = &parPrec;
    s->pParIter = &parIter;

    int   job   = 0;
    int maxit   = 0;
    T(cout<<"HHHHH bef. Clean in renewbcg\n";)//DB!
    s->pSolver->Clean();

    int ierr = 0;
    T(std::cout<<"##### inside renewbcg bef. k3biilu2_bcg\n";)//db!
    k3biilu2_bcg (s->ibl, s->ia, s->ja, s->a,
                NULL, NULL,
		job, maxit, s->pParams, s->pParIter, s->pSolver,
	        &ierr, s->istat, s->dstat);
    T(std::cout<<"##### inside renewbcg aft. k3biilu2_bcg ierr="<<ierr<<"\n";)//db!
    if (ierr) std::cout << "initialization of k3biilu2 failed, ierr=" << ierr << endl; //TODO: myid

    return ierr;
}

/* Solve linear system */
int solvebcg(bcg *s, Vector *b, Vector *x) 
{
    s->pParams  = &parPrec;
    s->pParIter = &parIter;

    int job     = 1;
    int maxit   = s->pParIter->maxit;

    int ierr = 0;
    T(std::cout<<"##### inside solvebcg bef. k3biilu2_bcg\n";)//db!
    k3biilu2_bcg (s->ibl, s->ia, s->ja, s->a, b->v, x->v,
		job, maxit, s->pParams, s->pParIter, s->pSolver,
	        &ierr, s->istat, s->dstat);
    T(std::cout<<"##### inside solvebcg aft. k3biilu2_bcg\n";)//db!
    if (ierr) std::cout << "linear system solution by k3biilu2 failed, ierr=" << ierr << endl; //TODO: myid

    s->ITER  = s->istat[2];
    s->RESID = s->dstat[2];

    return ierr;
}

/* Free memory used by solver */
void freebcg(bcg *s) 
{
    T(std::cout<<"##### inside freebcg bef. clean\n";)//db!
    s->pSolver->Clean();
}

/*****************************************************************************/

void MatrixCopyDataK3biilu2(void ** ppA, void * pB)
{
	matrix * B = (matrix *)pB;
	if( ppA == NULL || pB == NULL ) throw INMOST::DataCorruptedInSolver;
	*ppA = malloc(sizeof(matrix));
	matrix * A = (matrix *)ppA;
	A->n = B->n;
	if( B->n != 0 )
	{
		int nnz = B->ia[B->n] - B->ia[0];
		A->nproc = B->nproc;
		A->ibl = (int *) malloc(sizeof(int)*(A->nproc+1));
		memcpy(A->ibl,B->ibl,sizeof(int)*(A->nproc+1));
		A->ia = (int *) malloc(sizeof(int)*(A->n+1));
		memcpy(A->ia,B->ia,sizeof(int)*(A->n+1));
		A->ja = (int *) malloc(sizeof(int)*nnz);
		memcpy(A->ja,B->ja,sizeof(int)*nnz);
		A->A = (double *) malloc(sizeof(double)*nnz);
		memcpy(A->A,B->A,sizeof(double)*nnz);
	}
}

void MatrixAssignDataK3biilu2(void * pA, void * pB)
{
	matrix * A = (matrix *)pA;
	matrix * B = (matrix *)pB;
	if( A == NULL || B == NULL ) throw INMOST::DataCorruptedInSolver;
	if( A != B )
	{
		if( A->n != 0 )
		{
			free(A->ibl);
			free(A->ia);
			free(A->ja);
			free(A->A);
		}
		if( B->n != 0 )
		{
			int nnz = B->ia[B->n] - B->ia[0];
			A->n = B->n;
			A->nproc = B->nproc;
			A->ibl = (int *) malloc(sizeof(int)*(A->nproc+1));
			memcpy(A->ibl,B->ibl,sizeof(int)*(A->nproc+1));
			A->ia = (int *) malloc(sizeof(int)*(A->n+1));
			memcpy(A->ia,B->ia,sizeof(int)*(A->n+1));
			A->ja = (int *) malloc(sizeof(int)*nnz);
			memcpy(A->ja,B->ja,sizeof(int)*nnz);
			A->A = (double *) malloc(sizeof(double)*nnz);
			memcpy(A->A,B->A,sizeof(double)*nnz);	
		}
	}
}

void MatrixInitDataK3biilu2(void ** ppA, INMOST_MPI_Comm comm, const char * name)
{
        T(std::cout<<"##### ins. MatrixInitDataK3biilu2 \n";)//db!
	if( ppA == NULL ) throw INMOST::DataCorruptedInSolver;
	if( *ppA == NULL )
	{
		*ppA = malloc(sizeof(matrix));
		matrix * A = (matrix *)*ppA;
		A->n = 0;
		A->nproc = 0;
                T(std::cout<<"##### ins. MatrixInitDataK3biilu2 n=nproc=0 \n";)//db!
	}
    (void) comm;
    (void) name;
}

void MatrixDestroyDataK3biilu2(void ** pA)
{
	matrix * A = (matrix *)(*pA);
	if( A != NULL )
	{
		if( A->n != 0 )
		{
			free(A->ibl);
			free(A->ia);
			free(A->ja);
			free(A->A);
                        T(std::cout<<"##### ins. MatrixDestroyDataK3biilu2 ...free \n";)//db!
		}
		free(*pA);
		*pA = NULL;
	}
}



void MatrixFillK3biilu2(void * pA, int size, int nproc, int * ibl, int * ia, int * ja, double * values)
{
        T(std::cout<<"##### ins. MatrixFillK3biilu2 n="<<size<<" nproc="<<nproc<<" \n";)//db!
	if( pA == NULL ) throw INMOST::DataCorruptedInSolver;
	matrix * A = (matrix *) pA;
	A->n = size;
	A->nproc = nproc;
	A->ibl = ibl;
	A->ia = ia;
	A->ja = ja;
	A->A = values;
}

void MatrixFillValuesK3biilu2(void * pA, double * values)
{
        T(std::cout<<"##### ins. MatrixFillValuesK3biilu2 \n";)//db!
	if( pA == NULL ) throw INMOST::DataCorruptedInSolver;
	matrix * A = (matrix *) pA;
	free(A->A);
	A->A = values;
}

void MatrixFinalizeK3biilu2(void * data)
{
	//don't need to do anything
    (void) data;
}

void VectorInitDataK3biilu2(void ** ppA, INMOST_MPI_Comm comm, const char * name)
{
	if( ppA == NULL ) throw INMOST::DataCorruptedInSolver;
	*ppA = malloc(sizeof(Vector));
	Vector * A = (Vector *)*ppA;
	A->n = 0;
    (void) comm;
    (void) name;
}

void VectorCopyDataK3biilu2(void ** ppA, void * pB)
{
        T(std::cout<<"##### ins. VectorCopyDataK3biilu2 \n";)//db!
	if( ppA == NULL || pB == NULL ) throw INMOST::DataCorruptedInSolver;
	*ppA = malloc(sizeof(Vector));
	Vector * A = (Vector *)*ppA;
	Vector * B = (Vector *)pB;
	A->n = B->n;
	if( B->n != 0 )
	{
		A->v = (double *)malloc(sizeof(double)*A->n);
		memcpy(A->v,B->v,sizeof(double)*A->n);
	}
}

void VectorAssignDataK3biilu2(void * pA, void * pB)
{
        T(std::cout<<"##### ins. VectorAssignDataK3biilu2 \n";)//db!
	Vector * A = (Vector *)pA;
	Vector * B = (Vector *)pB;
	if( A == NULL || B == NULL ) throw INMOST::DataCorruptedInSolver;
	if( A != B )
	{
		if( A->n != 0 ) free(A->v);
		A->n = B->n;
		if( B->n != 0 )
		{
			A->v = (double *) malloc(sizeof(double)*A->n);
			memcpy(A->v,B->v,sizeof(double)*A->n);
		}
	}
}

void VectorPreallocateK3biilu2(void * pA, int size)
{
	Vector * A = (Vector *)pA;
	if( A == NULL ) throw INMOST::DataCorruptedInSolver;
	A->n = size;
	A->v = (double *)malloc(sizeof(double)*size);
}

void VectorFillK3biilu2(void * pA, double * values)
{
	Vector * A = (Vector *)pA;
	if( A == NULL ) throw INMOST::DataCorruptedInSolver;
	memcpy(A->v,values,sizeof(double)*A->n);
}
void VectorLoadK3biilu2(void * pA, double * values)
{
	Vector * A = (Vector *)pA;
	if( A == NULL ) throw INMOST::DataCorruptedInSolver;
	memcpy(values,A->v,sizeof(double)*A->n);
}

void VectorFinalizeK3biilu2(void * data)
{
    (void) data;
}

void VectorDestroyDataK3biilu2(void ** ppA)
{
	if( ppA == NULL) throw INMOST::DataCorruptedInSolver;
	if( *ppA != NULL )
	{
		Vector * A = (Vector *)*ppA;
		free(A->v);
		free(*ppA);
		*ppA = NULL;
	}
}

void SolverInitializeK3biilu2(int * argc, char *** argv, const char * file_options)
{
    T(std::cout<<"##### ins. SolverInitializeK3biilu2 ("<<file_options<<") \n";)//db!
    if (file_options == NULL) return;
    std::string s = file_options;
    if (s == "" || s == " ") return;
    ParIterDefault();
    std::ifstream is;
    T(std::cout<<"##### ins. SolverInitializeK3biilu2: bef. open("<<file_options<<") \n";)//db!
    is.open(file_options, std::ifstream::in);
    if (s == "k3biilu2_options.txt") {
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.prec_float);   //01 prec_float
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.ncycle);       //02 ncycle
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.ordtype);      //03 ordtype
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.collap);       //04 collap
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.sctype);       //05 sctype
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.nitersc);      //06 nitersc
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.fcttype);      //07 fcttype
        getline(is, s); sscanf(s.c_str(), "%lg", &parPrec.pivmin);       //08 pivmin
        getline(is, s); sscanf(s.c_str(), "%lg", &parPrec.tau1);         //09 tau1
        getline(is, s); sscanf(s.c_str(), "%lg", &parPrec.tau2);         //10 tau2
        getline(is, s); sscanf(s.c_str(), "%lg", &parPrec.theta);        //11 theta
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.ittype);       //12 ittype
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.niter_cycle);  //13 niter_cycle
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.ncoef);        //14 ncoef
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.niter_cycle2); //15 niter_cycle2
        getline(is, s); sscanf(s.c_str(), "%lg", &parIter.eps);          //16 eps
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.maxit);        //17 maxit
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.ichk);         //18 ichk
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.msglev);       //19 msglev
    } else if (s == "ctrl_dat") {
        getline(is, s);                                                  //1 skip iext
        getline(is, s);                                                  //2 skip mtx filename
        getline(is, s);                                                  //3 skip rhs filename
        getline(is, s); sscanf(s.c_str(), "%lg", &parIter.eps);          //4 eps
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.maxit);        //5 maxit
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.niter_cycle);  //6 kgmr
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.ncoef);        //7 kdeg
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.ncycle);       //8 kovl
        getline(is, s); sscanf(s.c_str(), "%lg", &parPrec.tau1);         //9 tau
	parPrec.tau2 = -1.0;
	parIter.ittype  = (parIter.niter_cycle > 1) ? 1 : 0;
        //? msglev
    } else { // file: "biilu2_options.txt"
        getline(is, s); sscanf(s.c_str(), "%d",  &parPrec.ncycle); //1 kovl
        getline(is, s); sscanf(s.c_str(), "%lg", &parPrec.tau1);   //2 tau
        getline(is, s); sscanf(s.c_str(), "%lg", &parIter.eps);    //3 eps
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.maxit);  //4 maxit
        getline(is, s); sscanf(s.c_str(), "%d",  &parIter.msglev); //5 msglev
	parPrec.tau2 = -1.0;
    }
    T(std::cout<<"##### ins. SolverInitializeK3biilu2:  prec_float="<<parPrec.prec_float<<" ncycle="<<parPrec.ncycle<<" ordtype="<<parPrec.ordtype<<" collap="<<parPrec.collap<<" sctype="<<parPrec.sctype<<" nitersc="<<parPrec.nitersc<<" fcttype="<<parPrec.fcttype<<" pivmin="<<parPrec.pivmin<<" tau1="<<parPrec.tau1<<" tau2="<<parPrec.tau2<<" theta="<<parPrec.theta<<" ittype="<<parIter.ittype<<" niter_cycle="<<parIter.niter_cycle<<" ncoef="<<parIter.ncoef<<" niter_cycle2="<<parIter.niter_cycle2<<" eps="<<parIter.eps<<" maxit="<<parIter.maxit<<" ichk="<<parIter.ichk<<" msglev="<<parIter.msglev<<" from: "<<file_options<<" \n";)//db!
	(void) argc;
	(void) argv;
}

bool SolverIsFinalizedK3biilu2()
{
	return true; //no need to finalize
}

void SolverFinalizeK3biilu2()
{
}

void SolverDestroyDataK3biilu2(void ** data)
{
	if( data != NULL )
	{
		if( *data != NULL )
		{
			bcg * m = (bcg *)*data;
			freebcg(m);
			free(m);
		}
		*data = NULL;
	}
}

void SolverInitDataK3biilu2(void ** data, INMOST_MPI_Comm comm, const char * name)
{
        T(std::cout<<"##### ins. SolverInitDataK3biilu2 \n";)//db!
	*data = malloc(sizeof(bcg));
	((bcg *)*data)->n = 0;
	((bcg *)*data)->nproc = 0;
	((bcg *)*data)->pSolver = &k3_solver;
        T(cout<<"HHHHH bef. Clean in SolverInitDataK3biilu2 \n";)//DB!
	((bcg *)*data)->pSolver->Clean();
	(void) comm;
	(void) name;
}

void SolverCopyDataK3biilu2(void ** data, void * other_data, INMOST_MPI_Comm comm)
{
	throw INMOST::NotImplemented; //later
	(void) data;
	(void) other_data;
	(void) comm;
}

void SolverAssignDataK3biilu2(void * data, void * other_data)
{
	throw INMOST::NotImplemented; //later
	(void) data;
	(void) other_data;
}

void SolverSetMatrixK3biilu2(void * data, void * matrix_data, bool same_pattern, bool reuse_preconditioner)
{
        T(std::cout<<"##### ins. SolverSetMatrixK3biilu2 \n";)//db!
	bcg * m = (bcg *)data;
	matrix * A = (matrix *)matrix_data;
        //if( A == NULL) std::cout<<"##### A == NULL ... \n";//db!
        //if( m == NULL) std::cout<<"##### m == NULL ... \n";//db!
	if( A == NULL || m == NULL ) throw INMOST::DataCorruptedInSolver;
        T(std::cout<<"##### ins. SolverSetMatrixK3biilu2 bef. initbcg or newmatrixbcg \n";)//db!
	if( m->n == 0 )
		initbcg(m,A,parIter.eps);
	else
		newmatrixbcg(m,A,reuse_preconditioner);
	(void) same_pattern;
        T(std::cout<<"##### ins. SolverSetMatrixK3biilu2 bef. return \n";)//db!
}

bool SolverSolveK3biilu2(void * data, void * rhs_data, void * sol_data)
{
        T(std::cout<<"##### ins. SolverSolveK3biilu2 \n";)//db!
	bcg * m = (bcg *)data;
	Vector * rhs = (Vector*)rhs_data, * sol = (Vector *)sol_data;
	return solvebcg(m,rhs,sol) == 0;
}

int SolverIterationNumberK3biilu2(void * data)
{
	return ((bcg *)data)->ITER;
}

double SolverResidualNormK3biilu2(void * data)
{
	return ((bcg *)data)->RESID;
}

/*
void SolverAddOtherStatK3biilu2(void * data, unsigned int * pivmod, double * prdens, double * t_prec, double * t_iter)
{
	*pivmod += ((bcg *)data)->istat[0];
	*prdens += ((bcg *)data)->dstat[0];
	*t_prec += ((bcg *)data)->dstat[7];
	*t_iter += ((bcg *)data)->dstat[9];
	return;
}
*/

//#endif //USE_SOLVER_K3BIILU2

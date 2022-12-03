//
// Created by kazem on 04/30/21.
//

#include "triangularSolve/Triangular_BCSC.h"

#include <cassert>
#include <common/Sym_BLAS.h>
#include <iostream>


#ifdef  OPENMP
#include "omp.h"
#endif

#include "triangularSolve/BLAS.h"

namespace sym_lib {
 namespace parsy {


// TODO: enabling SYM_BLAS should be faster than MKL here
  int
  blockedLsolve(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup,
    int *sup2col, int supNo,
                double *x) {
   int p, j;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   double *tempVec = new double[n]();
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i = 1; i <= supNo; ++i) {// for each supernode
    int curCol = i != 0 ? sup2col[i - 1] : 0;
    int nxtCol = sup2col[i];
    int supWdt = nxtCol - curCol;
    int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
    double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
    //lSolve_dense(nSupR,supWdt,Ltrng,&x[curCol]);
    dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
    Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
    //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
    dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol], tempVec);
    for (int l = Li_ptr[curCol] + supWdt, k = 0; l < Li_ptr[nxtCol]; ++l, ++k) {
     x[Li[l]] -= tempVec[k];
     tempVec[k] = 0;
    }
#if 0
    for (int k = 0; k < 200; ++k) {
              std::cout<<","<<x[k];
          }
          std::cout<<"\n";
#endif
   }
   delete[]tempVec;
   return (1);
  }

  int blockedLsolve_mrhs(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col,
                         int supNo, double *x, int n_rhs, int max_col){
   int i, p, k;
   double one[2], zero[2];
   one[0] = 1.0;
   one[1] = 0.;
   zero[0] = 0.;
   zero[1] = 0.;
   int ione = 1;
   int off_set = max_col < 0 ? n : max_col;
   auto *tempvec = new double[n_rhs*off_set]();

   for (i = 0; i < supNo; i++) {
    int curCol = sup2col[i];
    int nxtCol = sup2col[i + 1];
    int supWdt = nxtCol - curCol;
    int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
    double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode

/*    SYM_DTRSM("L", "L", "N", "N", &supWdt,&n_rhs,one,Ltrng,
          &nSupR,&x[curCol],&n);*/

#ifdef CBLAS
    cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasTrans, CblasNonUnit, supWdt, n_rhs, 1.0,
                Ltrng, nSupR, &x[curCol], n);
#else
    SYM_DTRSM("L", "L", "T", "N", &supWdt, &ione, one, Ltrng,
              &nSupR, &x[curCol], &n);
#endif
    Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
    int tmpRow = nSupR - supWdt;



#ifdef CBLAS
    cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, tmpRow, n_rhs, supWdt, one[0], Ltrng,
              nSupR,
              &x[curCol], n,
              one[0], tempvec, off_set);

#else
    SYM_DGEMM("N", "N", &tmpRow, &n_rhs, &supWdt, one, Ltrng,
          &nSupR,
          &x[curCol], &n,
          one, tempvec, &off_set);

#endif

    for (int l = Li_ptr[curCol] + supWdt, k = 0; l < Li_ptr[nxtCol]; l++, k++) {
     int idx = Li[l];
     for (int j = 0; j < n_rhs; ++j) {
      auto tmp = k + j*off_set;
      x[idx+(j*n)] -= tempvec[tmp];
      tempvec[tmp] = 0;
     }
    }
   }
   delete[]tempvec;
   return 1;
  }


  int
  blockedLTsolve(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col, int supNo,
                 double *x) {
   int p, j;
   double one[2], zero[2];
   one[0] = 1.0;
   one[1] = 0.;
   zero[0] = 0.;
   zero[1] = 0.;
   double minus_one = -1;
   int ione = 1;
   double *tempVec = new double[n]();
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i = supNo; i > 0; --i) {// for each supernode
    int curCol = i != 0 ? sup2col[i - 1] : 0;
    int nxtCol = sup2col[i];
    int supWdt = nxtCol - curCol;
    int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode

    double *Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
    for (int l = 0; l < nSupR - supWdt; ++l) {
     tempVec[l] = x[Li[Li_ptr[curCol] + supWdt + l]];
    }
#ifdef SYM_BLAS
    dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else

    int tmpRow = nSupR - supWdt;
#ifdef CBLAS
    cblas_dgemv(CblasColMajor,CblasTrans,tmpRow, supWdt, minus_one, Ltrng,
      nSupR, tempVec, ione, 1.0, &x[curCol], ione);
#else
    SYM_DGEMV("T", &tmpRow, &supWdt, &minus_one, Ltrng, &nSupR, tempVec, &ione,
              one, &x[curCol], &ione);
#endif

#endif
    Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
    dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);//FIXME make it for transpose
#else
#ifdef CBLAS
    cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasTrans, CblasNonUnit, supWdt, ione, 1.0,
                Ltrng, nSupR, &x[curCol], n);
#else
    SYM_DTRSM("L", "L", "T", "N", &supWdt, &ione, one, Ltrng,
              &nSupR, &x[curCol], &n);
#endif

#endif

#if 0
    for (int k = 0; k < 200; ++k) {
              std::cout<<","<<x[k];
          }
          std::cout<<"\n";
#endif
   }
   delete[]tempVec;
   return (1);
  }

  int
  blockedLTsolve_mrhs(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col, int supNo,
                 double *x, int m_rhs, int max_col) {
   int p, j;
   double one[2], zero[2];
   one[0] = 1.0;
   one[1] = 0.;
   zero[0] = 0.;
   zero[1] = 0.;
   double minus_one = -1;
   int ione = 1;
   int off_set = max_col < 0 ? n : max_col;
   double *tempVec = new double[m_rhs*off_set]();
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i = supNo; i > 0; --i) {// for each supernode
    int curCol = i != 0 ? sup2col[i - 1] : 0;
    int nxtCol = sup2col[i];
    int supWdt = nxtCol - curCol;
    int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode

    double *Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
    for (int l = 0; l < nSupR - supWdt; ++l) {
     auto base = Li[Li_ptr[curCol] + supWdt + l];
     for (int k = 0; k < m_rhs; ++k) {
      auto idx = k * off_set;
      tempVec[l + idx ] = x[base + (k*n) ];
     }
    }
#ifdef SYM_BLAS
    dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else

    int tmpRow = nSupR - supWdt;
#ifdef CBLAS
    cblas_dgemm(CblasColMajor, CblasTrans, CblasNoTrans, supWdt, m_rhs, tmpRow, minus_one, Ltrng, nSupR, tempVec, off_set,
              one[0], &x[curCol], n);
#else
    SYM_DGEMM("T", "N", &supWdt, &m_rhs, &tmpRow, &minus_one, Ltrng, &nSupR, tempVec, &off_set,
              one, &x[curCol], &n);
#endif

#endif
    Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
    dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);//FIXME make it for transpose
#else
#ifdef CBLAS
    cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasConjTrans, CblasNonUnit, supWdt, ione, 1.0,
                Ltrng, nSupR, &x[curCol], n);
#else
    SYM_DTRSM("L", "L", "T", "N", &supWdt, &m_rhs, one, Ltrng,
              &nSupR, &x[curCol], &n);
#endif

#endif

#if 0
    for (int k = 0; k < 200; ++k) {
              std::cout<<","<<x[k];
          }
          std::cout<<"\n";
#endif
   }
   delete[]tempVec;
   return (1);
  }

  int LeveledBlockedLTsolve_update(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup,
                                   int *sup2col, int supNo, double *x, int levels, int *levelPtr, int *levelSet,
                                   int chunk, bool *marked, double *ws_dbl) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   double minus_one = -1;
   //double *tempVec = new double[n]();
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = levels - 1; i1 >= 0; --i1) {
    int li = 0;
#pragma omp parallel private(li)
    {
     //tempVec = new double[n]();
     double *tempVec;
     if (ws_dbl == NULL) {
      tempVec = (double *) calloc(n, sizeof(double));
     } else {//FIXME: the else part is not right
      //std::cout<<"-> "<<omp_get_thread_num()<<"\n";
#ifdef OPENMP
      tempVec = ws_dbl + omp_get_thread_num() * n;
#else
      tempVec = ws_dbl;
#endif
     }
#pragma omp for \
            schedule(static)
     for (li = levelPtr[i1]; li < levelPtr[i1 + 1]; ++li) {
      int i = levelSet[li];
      if (!marked[i])
       continue;
      int curCol = sup2col[i];
      int nxtCol = sup2col[i + 1];
      int supWdt = nxtCol - curCol;
      int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode

      double *Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
      for (int l = 0; l < nSupR - supWdt; ++l) {
       tempVec[l] = x[Li[Li_ptr[curCol] + supWdt + l]];
      }
#ifdef SYM_BLAS
      dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else
      int tmpRow = nSupR - supWdt;
#ifdef CBLAS
      cblas_dgemv(CblasColMajor,CblasTrans,tmpRow, supWdt, minus_one, Ltrng,
                  nSupR, tempVec, ione, 1.0, &x[curCol], ione);
#else
      SYM_DGEMV("T", &tmpRow, &supWdt, &minus_one, Ltrng, &nSupR, tempVec, &ione,
                one, &x[curCol], &ione);
#endif


#endif
      Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
      dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);//FIXME make it for transpose
#else
#ifdef CBLAS
      cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasConjTrans, CblasNonUnit, supWdt, ione, 1.0,
                 Ltrng, nSupR, &x[curCol], n);
#else
      SYM_DTRSM("L", "L", "T", "N", &supWdt, &ione, one, Ltrng,
                &nSupR, &x[curCol], &n);
#endif
#endif
      for (int l = 0; l < nSupR - supWdt; ++l) {
       tempVec[l] = 0;
      }
     }
     if (ws_dbl == NULL) {
      free(tempVec);
     }
    }
   }

   return (1);
  }

  int
  blockedPrunedLSolve(int n, int *Lp, int *Li, double *Lx, int NNZ, int *Li_ptr, int *BPSet, int PBSetSize,
                      int *sup2col,
                      int supNo, double *x) {
   int p, j, i;
   double tmp = 0;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   double *tempVec = new double[n]();
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   // for (int i = 2530; i < supNo; ++i) {// for each supernode
   for (int ps = 0; ps < PBSetSize; ++ps) {// for each supernode
    i = BPSet[ps];
    int curCol = i != 0 ? sup2col[i - 1] : 0;
    int nxtCol = sup2col[i];
    int supWdt = nxtCol - curCol;
    int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
    double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
    //lSolve_dense(nSupR,supWdt,Ltrng,&x[curCol]);
#ifdef SYM_BLAS
    dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);
#else
#ifdef CBLAS
    cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit, supWdt, ione, 1.0,
               Ltrng, nSupR, &x[curCol], n);
#else
    SYM_DTRSM("L", "L", "N", "N", &supWdt, &ione, one, Ltrng,
              &nSupR, &x[curCol], &n);
#endif

#endif
    Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
    //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#ifdef SYM_BLAS
    dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else
    int tmpRow = nSupR - supWdt;
#ifdef CBLAS
    cblas_dgemv(CblasColMajor,CblasNoTrans,tmpRow, supWdt, one[0], Ltrng,
                nSupR, &x[curCol], ione, zero[0], tempVec, ione);
#else
    SYM_DGEMV("N", &tmpRow, &supWdt, one, Ltrng, &nSupR, &x[curCol], &ione,
              zero, tempVec, &ione);
#endif

#endif
    for (int l = Li_ptr[curCol] + supWdt, k = 0; l < Li_ptr[nxtCol]; ++l, ++k) {
     x[Li[l]] -= tempVec[k];
     tempVec[k] = 0;
    }
#if 0
    for (int k = 0; k < 200; ++k) {
              std::cout<<","<<x[k];
          }
          std::cout<<"\n";
#endif
   }
   delete[]tempVec;
   return (1);
  }

  int leveledBlockedLsolve(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col,
                           int supNo, double *x, int levels, int *levelPtr, int *levelSet, int chunk) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   //double *tempVec = new double[n]();
   double *tempVec;
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int l = 0; l < levels; ++l) {
    int li = 0;
#pragma omp parallel private(li, tempVec)
    {
     //tempVec = new double[n]();
     tempVec = (double *) calloc(n, sizeof(double));
#pragma omp for \
            schedule(static)
     for (li = levelPtr[l]; li < levelPtr[l + 1]; ++li) {
      int i = levelSet[li];

      int curCol = sup2col[i];
      int nxtCol = sup2col[i + 1];
      int supWdt = nxtCol - curCol;
      assert(supWdt > 0);
      int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
      double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
      //lSolve_dense_col_sync(nSupR,supWdt,Ltrng,&x[curCol]);
      dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
      Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
      //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
      dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol],
                   tempVec);
//     #pragma omp critical
      for (int l = Li_ptr[curCol] + supWdt, k = 0;
           l < Li_ptr[nxtCol]; ++l, ++k) {
#pragma omp atomic
       x[Li[l]] -= tempVec[k];
       tempVec[k] = 0;
      }
     }
     free(tempVec);
    }
   }

   return (1);
  }

  int leveledBlockedLsolve_update(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup,
                                  int *sup2col, int supNo, double *x, int levels, int *levelPtr, int *levelSet,
                                  int chunk, bool *mask, double *ws_dbl) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   //double *tempVec = new double[n]();
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int l = 0; l < levels; ++l) {
    int li = 0;
#pragma omp parallel private(li)
    {
     double *tempVec;
     if (ws_dbl == NULL) {
      tempVec = (double *) calloc(n, sizeof(double));
     } else {//FIXME: the else part is not right
      //std::cout<<"-> "<<omp_get_thread_num()<<"\n";
#ifdef OPENMP
      tempVec = ws_dbl + omp_get_thread_num() * n;
#else
      tempVec = ws_dbl;
#endif
     }
#pragma omp for \
            schedule(static)
     for (li = levelPtr[l]; li < levelPtr[l + 1]; ++li) {
      int i = levelSet[li];
      if (!mask[i])
       continue;
      int curCol = sup2col[i];
      int nxtCol = sup2col[i + 1];
      int supWdt = nxtCol - curCol;
      assert(supWdt > 0);
      int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
      double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
      //lSolve_dense_col_sync(nSupR,supWdt,Ltrng,&x[curCol]);
      dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
      Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
      //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
      dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol],
                   tempVec);
//     #pragma omp critical
      for (int l = Li_ptr[curCol] + supWdt, k = 0;
           l < Li_ptr[nxtCol]; ++l, ++k) {
#pragma omp atomic
       x[Li[l]] -= tempVec[k];
       tempVec[k] = 0;
      }

     }
     free(tempVec);
    }
   }

   return (1);
  }

  int
  H2LeveledBlockedLsolve(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col,
                         int supNo, double *x, int levels, int *levelPtr, int *levelSet, int parts, int *parPtr,
                         int *partition, int chunk) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   //double *tempVec = new double[n]();
   double *tempVec;
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = 0; i1 < levels; ++i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1, tempVec)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      tempVec = (double *) calloc(n, sizeof(double));
      for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
       int i = partition[k1];
       int curCol = sup2col[i];
       int nxtCol = sup2col[i + 1];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
       double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
       dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
#else
#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit, supWdt, ione, 1.0,
                   Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "N", "N", &supWdt, &ione, one, Ltrng,
                 &nSupR, &x[curCol], &n);
#endif
#endif
       Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#ifdef SYM_BLAS
       dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol],
                    tempVec);
#else
       int tmpRow = nSupR - supWdt;
#ifdef CBLAS
       cblas_dgemv(CblasColMajor,CblasNoTrans,tmpRow, supWdt, one[0], Ltrng,
                       nSupR, &x[curCol], ione, zero[0], tempVec, ione);
#else
       SYM_DGEMV("N", &tmpRow, &supWdt, one, Ltrng, &nSupR, &x[curCol], &ione,
                 zero, tempVec, &ione);
#endif
#endif
       for (int l = Li_ptr[curCol] + supWdt, k = 0;
            l < Li_ptr[nxtCol]; ++l, ++k) {
#pragma omp atomic
        x[Li[l]] -= tempVec[k];
        tempVec[k] = 0;
       }
      }
      free(tempVec);
     }
    }

   }
   return (1);
  }


  int
  H2LeveledBlockedLsolve_mrhs(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col,
                         int supNo, double *x, int n_rhs, int levels, int *levelPtr, int *levelSet, int parts, int *parPtr,
                         int *partition, int chunk, int max_col) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   //double *tempVec = new double[n]();
   double *tempvec;
   int off_set = max_col < 0 ? n : max_col;

   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = 0; i1 < levels; ++i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1, tempvec)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      tempvec = new double[n_rhs*off_set]();
      for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
       int i = partition[k1];
       int curCol = sup2col[i];
       int nxtCol = sup2col[i + 1];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
       double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode

#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasConjTrans, CblasNonUnit, supWdt, n_rhs, 1.0,
                   Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "T", "N", &supWdt, &ione, one, Ltrng,
              &nSupR, &x[curCol], &n);
#endif
       Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       int tmpRow = nSupR - supWdt;

#ifdef CBLAS
       cblas_dgemm(CblasColMajor, CblasNoTrans, CblasNoTrans, tmpRow, n_rhs, supWdt, one[0], Ltrng,
                   nSupR,
                   &x[curCol], n,
                   one[0], tempvec, off_set);

#else
       SYM_DGEMM("N", "N", &tmpRow, &n_rhs, &supWdt, one, Ltrng,
          &nSupR,
          &x[curCol], &n,
          one, tempvec, &off_set);

#endif

       for (int l = Li_ptr[curCol] + supWdt, k = 0; l < Li_ptr[nxtCol]; l++, k++) {
        int idx = Li[l];
        for (int j = 0; j < n_rhs; ++j) {
         auto tmp = k + j*off_set;
         x[idx+(j*n)] -= tempvec[tmp];
         tempvec[tmp] = 0;
        }
       }
      }
      delete []tempvec;
     }
    }
   }
   return (1);
  }

  int H2LeveledBlockedLsolve_update(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup,
                                    int *sup2col, int supNo, double *x, int levels, int *levelPtr, int *levelSet,
                                    int parts, int *parPtr, int *partition, int chunk, bool *mask, double *ws_dbl) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;

   //double *tempVec = new double[n]();
   //double *tempVec;
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = 0; i1 < levels; ++i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      double *tempVec;
      if (ws_dbl == NULL) {
       tempVec = (double *) calloc(n, sizeof(double));
      } else {//FIXME: the else part is not right
       //std::cout<<"-> "<<omp_get_thread_num()<<"\n";
#ifdef OPENMP
       tempVec = ws_dbl + omp_get_thread_num() * n;
#else
       tempVec = ws_dbl;
#endif
      }
      for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
       int i = partition[k1];
       if (!mask[i])
        continue;
       int curCol = sup2col[i];
       int nxtCol = sup2col[i + 1];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
       double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
       //lSolve_dense(nSupR,supWdt,Ltrng,&x[curCol]);
#ifdef SYM_BLAS
       dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
#else
#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit, supWdt, ione, 1.0,
                       Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "N", "N", &supWdt, &ione, one, Ltrng,
                 &nSupR, &x[curCol], &n);
#endif
#endif
       Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#ifdef SYM_BLAS
       dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol],
                    tempVec);
#else
       int tmpRow = nSupR - supWdt;
#ifdef CBLAS
       cblas_dgemv(CblasColMajor,CblasNoTrans,tmpRow, supWdt, one[0], Ltrng,
                   nSupR, &x[curCol], ione, zero[0], tempVec, ione);
#else
       SYM_DGEMV("N", &tmpRow, &supWdt, one, Ltrng, &nSupR, &x[curCol], &ione,
                 zero, tempVec, &ione);
#endif
#endif
       // #pragma omp critical
       for (int l = Li_ptr[curCol] + supWdt, k = 0;
            l < Li_ptr[nxtCol]; ++l, ++k) {
#pragma omp atomic
        x[Li[l]] -= tempVec[k];
        tempVec[k] = 0;
       }
      }
      if (ws_dbl == NULL) {
       free(tempVec);
      }
     }
    }

   }
   return (1);
  }

  int
  H2LeveledBlockedLTsolve(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col,
                          int supNo, double *x, int levels, int *levelPtr, int *levelSet, int parts, int *parPtr,
                          int *partition, int chunk) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   double minus_one = -1;
   //double *tempVec = new double[n]();
   double *tempVec;
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = levels - 1; i1 >= 0; --i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1, tempVec)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      tempVec = (double *) calloc(n, sizeof(double));
      for (int k1 = parPtr[j1 + 1] - 1; k1 >= parPtr[j1]; --k1) {
       int i = partition[k1];
       int curCol = sup2col[i];
       int nxtCol = sup2col[i + 1];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode

       double *Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       for (int l = 0; l < nSupR - supWdt; ++l) {
        tempVec[l] = x[Li[Li_ptr[curCol] + supWdt + l]];
       }
#ifdef SYM_BLAS
       dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else
       int tmpRow = nSupR - supWdt;
#ifdef CBLAS
       cblas_dgemv(CblasColMajor,CblasTrans,tmpRow, supWdt, minus_one, Ltrng,
                   nSupR, tempVec, ione, one[0], &x[curCol], ione);
#else
       SYM_DGEMV("T", &tmpRow, &supWdt, &minus_one, Ltrng, &nSupR, tempVec, &ione,
                 one, &x[curCol], &ione);
#endif
#endif
       Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
       dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);//FIXME make it for transpose
#else
#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasTrans, CblasNonUnit, supWdt, ione, 1.0,
                       Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "T", "N", &supWdt, &ione, one, Ltrng,
                 &nSupR, &x[curCol], &n);
#endif
#endif

      }
      free(tempVec);
     }
    }

   }
   return (1);
  }


  int
  H2LeveledBlockedLTsolve_mrhs(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup, int *sup2col,
                          int supNo, double *x, int m_rhs, int levels, int *levelPtr, int *levelSet, int parts, int *parPtr,
                          int *partition, int chunk, int max_col) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   double minus_one = -1;
   //double *tempVec = new double[n]();
   int off_set = max_col < 0 ? n : max_col;

   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = levels - 1; i1 >= 0; --i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      double *tempVec = new double[m_rhs*off_set]();
      for (int k1 = parPtr[j1 + 1] - 1; k1 >= parPtr[j1]; --k1) {
       int i = partition[k1];

       int curCol = i != 0 ? sup2col[i - 1] : 0;
       int nxtCol = sup2col[i];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
       //std::cout<<" : "<<nSupR <<"\n";
       double *Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       for (int l = 0; l < nSupR - supWdt; ++l) {
        auto base = Li[Li_ptr[curCol] + supWdt + l];
        for (int k = 0; k < m_rhs; ++k) {
         auto idx = k * off_set;
         tempVec[l + idx] = 0;
         tempVec[l + idx ] = x[base + (k*n) ];
        }
       }

#ifdef SYM_BLAS
       dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else
       //std::cout<<" : "<<nSupR <<" \n";
       int tmpRow = nSupR - supWdt;
#ifdef CBLAS
       cblas_dgemv(CblasColMajor,CblasTrans,tmpRow, supWdt, minus_one, Ltrng,
      nSupR, tempVec, ione, 1.0, &x[curCol], ione);
#else
       SYM_DGEMM("T", "N", &supWdt, &m_rhs, &tmpRow, &minus_one, Ltrng, &nSupR, tempVec, &off_set,
                 one, &x[curCol], &n);
#endif

#endif
       Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
       dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);//FIXME make it for transpose
#else
#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasConjTrans, CblasNonUnit, supWdt, ione, 1.0,
                Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "T", "N", &supWdt, &m_rhs, one, Ltrng,
                 &nSupR, &x[curCol], &n);
#endif

#endif
      }
      delete []tempVec;
     }
    }
   }
   return (1);
  }


  int H2LeveledBlockedLTsolve_update(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup,
                                     int *sup2col, int supNo, double *x, int levels, int *levelPtr, int *levelSet,
                                     int parts, int *parPtr, int *partition, int chunk, bool *mask, double *ws_dbl) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   double minus_one = -1;
   //double *tempVec = new double[n]();
   //double *tempVec;
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = levels - 1; i1 >= 0; --i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      //tempVec = (double *) calloc(n , sizeof(double));
      double *tempVec;
      if (ws_dbl == NULL) {
       tempVec = (double *) calloc(n, sizeof(double));
      } else {//FIXME: the else part is not right
       //std::cout<<"-> "<<omp_get_thread_num()<<"\n";
#ifdef OPENMP
       tempVec = ws_dbl + omp_get_thread_num() * n;
#else
       tempVec = ws_dbl;
#endif
      }
      for (int k1 = parPtr[j1 + 1] - 1; k1 >= parPtr[j1]; --k1) {
       int i = partition[k1];
       if (!mask[i]) {
        continue;
       }
       int curCol = sup2col[i];
       int nxtCol = sup2col[i + 1];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode

       double *Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       for (int l = 0; l < nSupR - supWdt; ++l) {
        tempVec[l] = x[Li[Li_ptr[curCol] + supWdt + l]];
       }
#ifdef SYM_BLAS
       dmatvec_blas(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#else
       int tmpRow = nSupR - supWdt;
#ifdef CBLAS
       cblas_dgemv(CblasColMajor,CblasTrans,tmpRow, supWdt, minus_one, Ltrng,
                   nSupR, tempVec, ione, one[0], &x[curCol], ione);
#else
       SYM_DGEMV("T", &tmpRow, &supWdt, &minus_one, Ltrng, &nSupR, tempVec, &ione,
                 one, &x[curCol], &ione);
#endif
#endif
       Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
#ifdef SYM_BLAS
       dlsolve_blas_nonUnit(nSupR,supWdt,Ltrng,&x[curCol]);//FIXME make it for transpose
#else
#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasTrans, CblasNonUnit, supWdt, ione, 1.0,
                      Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "T", "N", &supWdt, &ione, one, Ltrng,
                 &nSupR, &x[curCol], &n);
#endif
#endif
       for (int l = 0; l < nSupR - supWdt; ++l) {
        tempVec[l] = 0;
       }
      }
      if (ws_dbl == NULL) {
       free(tempVec);
      }

     }
    }

   }
   return (1);
  }


#define SYM_BLAS

  int H2LeveledBlockedLsolve_Peeled(int n, size_t *Lp, int *Li, double *Lx, int NNZ, size_t *Li_ptr, int *col2sup,
                                    int *sup2col, int supNo, double *x, int levels, int *levelPtr, int *levelSet,
                                    int parts, int *parPtr, int *partition, int chunk, int threads) {
   //int chunk = 70;
   double one[2], zero[2];
   one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
   one[1] = 0.;
   zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
   zero[1] = 0.;
   int ione = 1;
   //double *tempVec = new double[n]();
   //MKL_Domain_Set_Num_Threads(1, MKL_DOMAIN_BLAS);
   double *tempVec;
   if (!Lp || !Li || !x) return (0);                     /* check inputs */
   for (int i1 = 0; i1 < levels - 1; ++i1) {
    int j1 = 0;
#pragma omp parallel //shared(lValues)//private(map, contribs)
    {
#pragma omp  for schedule(static) private(j1, tempVec)
     for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
      //tempVec = new double[n]();
      tempVec = (double *) calloc(n, sizeof(double));
      for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
       int i = partition[k1];
       int curCol = sup2col[i];
       int nxtCol = sup2col[i + 1];
       int supWdt = nxtCol - curCol;
       int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
       double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
       //lSolve_dense(nSupR,supWdt,Ltrng,&x[curCol]);
#ifdef SYM_BLAS
       dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
#else
#ifdef CBLAS
       cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit, supWdt, ione, 1.0,
                   Ltrng, nSupR, &x[curCol], n);
#else
       SYM_DTRSM("L", "L", "N", "N", &supWdt, &ione, one, Ltrng,
             &nSupR, &x[curCol], &n);
#endif

#endif
       Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
       //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#ifdef SYM_BLAS
       dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol],
                    tempVec);
#else
       int tmpRow=nSupR - supWdt;
#ifdef CBLAS
           cblas_dgemv(CblasColMajor,CblasNoTrans,tmpRow, supWdt, one[0], Ltrng,
                       nSupR, &x[curCol], ione, zero[0], tempVec, ione);
#else
         SYM_DGEMV("N",&tmpRow,&supWdt,one,Ltrng,&nSupR,&x[curCol],&ione,
               zero,tempVec,&ione);

#endif
#endif

#pragma omp critical
       for (int l = Li_ptr[curCol] + supWdt, k = 0;
            l < Li_ptr[nxtCol]; ++l, ++k) {
#pragma omp atomic
        x[Li[l]] -= tempVec[k];
        tempVec[k] = 0;
       }
      }
      free(tempVec);
     }
    }

   }
#undef SYM_BLAS

   SET_BLAS_THREAD(threads);
/*#ifdef OPENBLAS
  openblas_set_num_threads(threads);
#else
  MKL_Domain_Set_Num_Threads(threads, MKL_DOMAIN_BLAS);
#endif*/
   //for (int i1 = 0; i1 < levels ; ++i1) {
   int i1 = levels - 1;
   int j1 = 0;
//#pragma omp parallel //shared(lValues)//private(map, contribs)
//  {
//#pragma omp  for schedule(auto) private(j1, tempVec)
   for (j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
    //tempVec = new double[n]();
    tempVec = (double *) calloc(n, sizeof(double));
    for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
     int i = partition[k1];
     int curCol = sup2col[i];
     int nxtCol = sup2col[i + 1];
     int supWdt = nxtCol - curCol;
     int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
     double *Ltrng = &Lx[Lp[curCol]];//first nnz of current supernode
     //lSolve_dense(nSupR,supWdt,Ltrng,&x[curCol]);
#ifdef SYM_BLAS
     dlsolve_blas_nonUnit(nSupR, supWdt, Ltrng, &x[curCol]);
#else
#ifdef CBLAS
     cblas_dtrsm(CblasColMajor, CblasLeft, CblasLower, CblasNoTrans, CblasNonUnit, supWdt, ione, 1.0,
                 Ltrng, nSupR, &x[curCol], n);
#else
     SYM_DTRSM("L", "L", "N", "N", &supWdt, &ione, one, Ltrng,
               &nSupR, &x[curCol], &n);
#endif

#endif
     Ltrng = &Lx[Lp[curCol] + supWdt];//first nnz of below diagonal
     //matVector(nSupR,nSupR-supWdt,supWdt,Ltrng,&x[curCol],tempVec);
#ifdef SYM_BLAS
     dmatvec_blas(nSupR, nSupR - supWdt, supWdt, Ltrng, &x[curCol],
                  tempVec);
#else
     int tmpRow = nSupR - supWdt;
#ifdef CBLAS
     cblas_dgemv(CblasColMajor,CblasNoTrans,tmpRow, supWdt, one[0], Ltrng,
                 nSupR, &x[curCol], ione, zero[0], tempVec, ione);
#else
     SYM_DGEMV("N", &tmpRow, &supWdt, one, Ltrng, &nSupR, &x[curCol], &ione,
               zero, tempVec, &ione);
#endif
#endif
     // #pragma omp critical
     for (int l = Li_ptr[curCol] + supWdt, k = 0;
          l < Li_ptr[nxtCol]; ++l, ++k) {
//#pragma omp atomic
      x[Li[l]] -= tempVec[k];
      tempVec[k] = 0;
     }
    }
    free(tempVec);
   }
//  }

   //}
   return (1);
  }
 }
}
//
// Created by kazem on 11/25/17.
//

#ifndef CHOLOPENMP_PARALLEL_PB_CHOLESKY_05_H
#define CHOLOPENMP_PARALLEL_PB_CHOLESKY_05_H
#include <chrono>
#ifdef ENABLE_OPENMP
 #include <omp.h>
#endif
#include <common/Sym_BLAS.h>
#include <cassert>
#include <common/Reach.h>

#define TIMING
//#undef TIMING
#define TIMING1
#undef TIMING1
#define BLASTIMING
#undef BLASTIMING
#undef PRUNE
#ifdef MKL
#include "mkl.h"
#endif

#ifdef OPENBLAS
#include "openblas/cblas.h"
#include "openblas/lapack.h"
#define MKL_INT int
#endif

#ifdef APPLEBLAS
#include "cblas.h"
#include "clapack.h"

#endif

#ifdef USE_TBB
#include <tbb/parallel_for.h>
#include "tbb/task_scheduler_init.h"
#endif
#include <iostream>

namespace sym_lib {
 namespace parsy {

bool cholesky_left_par_05(int n, int* c, int* r, double* values,
                          size_t *lC, int* lR, size_t* Li_ptr, double* lValues,
                          int *blockSet, int supNo, double *timing,
#ifndef PRUNE
                          int *aTree, int *cT, int *rT, int *col2Sup,
#else
                          int *prunePtr, int *pruneSet,
#endif

                          int nLevels, int *levelPtr, int *levelSet,
                          int nPar, int *parPtr, int *partition,
                          int chunk, int threads, int super_max,
                          int col_max, double *nodCost=NULL) {
 /*
  * For timing using BLAS
  */
 int top = 0;
 double *blasTimePerThread = timing + 4;
 int info=0;
 int thth = 0;
 double one[2], zero[2];
 one[0] = 1.0;    /* ALPHA for *syrk, *herk, *gemm, and *trsm */
 one[1] = 0.;
 zero[0] = 0.;     /* BETA for *syrk, *herk, and *gemm */
 zero[1] = 0.;
 std::chrono::time_point<std::chrono::system_clock> start, end, startin, endin;
 std::chrono::duration<double> elapsed_seconds;
 double duration4 = 0, duration3 = 0, duration2 = 0, duration1 = 0;
 // MKL_Domain_Set_Num_Threads(1,MKL_DOMAIN_BLAS);
 //omp_set_num_threads(6);
 //   std::cout<<"MAx threads are: " <<omp_get_max_threads()<<"\n";
 //omp_set_nested(1);
 int **map_list = new int*[threads]();
 double **contribs_list = new double*[threads]();
 int **xi_list = new int*[threads]();
 for (int i = 0; i < threads; ++i) {
  map_list[i] = new int[n]();
  contribs_list[i] = new double[super_max * col_max]();
  xi_list[i] = new int[2 * supNo]();
 }
#ifdef TIMING
 start = std::chrono::system_clock::now();
#endif
 for (int i1 = 0; i1 < nLevels - 1; ++i1) {

#ifndef USE_TBB
#pragma omp parallel //default(none) //shared(lValues)//private(map, contribs)
  {
#pragma omp  for schedule(static) private(i1, startin, endin, duration2)
   for (int j1 = levelPtr[i1]; j1 < levelPtr[i1 + 1]; ++j1) {
#ifdef ENABLE_OPENMP
    int worker_index = omp_get_thread_num();
#else
    int worker_index = 1;
#endif
#else
     tbb::task_scheduler_init TBBinit(threads);
    tbb::parallel_for(levelPtr[i1], levelPtr[i1+1], 1,[&](int j1) {
      int worker_index = tbb::task_arena::current_thread_index();
#endif
#ifdef BLASTIMING
     int threadID = omp_get_thread_num();
     std::chrono::time_point<std::chrono::system_clock> startBlas, endBlas;
#endif

     //std::cout<<worker_index<<"\n";
     int *map = map_list[worker_index]; //new int[n]();
     double *contribs = contribs_list[worker_index]; //new double[super_max * col_max]();
     int *xi = xi_list[worker_index]; //new int[2 * supNo]();
     //int pls = levelSet[j1];
#ifdef TIMING1
     startin = std::chrono::system_clock::now();
#endif
//#pragma omp parallel for schedule(static,chunk)private(thth)
     for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
      int s = partition[k1] + 1;

      int curCol = s != 0 ? blockSet[s - 1] : 0;
      int nxtCol = blockSet[s];
      int supWdt = nxtCol - curCol;
      int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
      for (size_t i = Li_ptr[curCol], cnt = 0; i < Li_ptr[nxtCol]; ++i) {
       map[lR[i]] = cnt++;//mapping L rows position to actual row idx
      }
      //copy the columns from A to L
      for (int i = curCol; i < nxtCol; ++i) {//Copy A to L
       int pad = i - curCol;
       for (int j = c[i]; j < c[i + 1]; ++j) {
        // if(r[j]>=i)//does not need to save upper part.
        lValues[lC[i] + map[r[j]]] = values[j];
        //   else
        //      printf("dddd\n");
       }
      }
      double *src, *cur = &lValues[lC[curCol]];//pointing to first element of the current supernode
#ifndef PRUNE
      top = ereach_sn(supNo, cT, rT, curCol, nxtCol, col2Sup, aTree, xi, xi + supNo);
      assert(top >= 0);
      for (int i = top; i < supNo; ++i) {
       int lSN = xi[i];

#else
       for (int i = prunePtr[s - 1]; i < prunePtr[s]; ++i) {
        int lSN = pruneSet[i];
#endif
#if DEBUG
       if(xi[top++] != lSN)
                          printf("fail");
#endif
       int nSupRs = 0;
       int cSN = blockSet[lSN];//first col of current SN
       int cNSN = blockSet[lSN + 1];//first col of Next SN
       size_t Li_ptr_cNSN = Li_ptr[cNSN];
       size_t Li_ptr_cSN = Li_ptr[cSN];
       int nSNRCur = Li_ptr_cNSN - Li_ptr_cSN;
       int supWdts = cNSN - cSN;//The width of current src SN
       int lb = 0, ub = 0;
       bool sw = true;
       for (size_t j = Li_ptr_cSN; j < Li_ptr_cNSN; ++j) {
        //finding the overlap between curCol and curCol+supWdt in the src col
        if (lR[j] >= curCol && sw) {
         //src*transpose(row lR[j])
         lb = j - Li_ptr_cSN;
         sw = false;
        }
        if (lR[j] < curCol + supWdt && !sw) {
         ub = j - Li_ptr_cSN;
        }
        if (lR[j] >= curCol + supWdt)
         break;
       }
       nSupRs = Li_ptr_cNSN - Li_ptr_cSN - lb;
       int ndrow1 = ub - lb + 1;
       int ndrow3 = nSupRs - ndrow1;
       src = &lValues[lC[cSN] +
                      lb];//first element of src supernode starting from row lb
       double *srcL = &lValues[lC[cSN] + ub + 1];
#ifdef BLASTIMING
       startBlas = std::chrono::system_clock::now();
#endif
//#ifdef MKL
//       dsyrk("L", "N", &ndrow1, &supWdts, one, src, &nSNRCur, zero,
//             contribs, &nSupRs);
//#endif
#ifdef CBLAS
       cblas_dsyrk(CblasColMajor, CblasLower, CblasNoTrans,ndrow1,supWdts,one[0],src,
                   nSNRCur,zero[0],contribs, nSupRs);
#endif
#ifdef MYBLAS
       //TODO
#endif
       // MKL_Domain_Set_Num_Threads(5,MKL_DOMAIN_BLAS);
       if (ndrow3 > 0) {
//#ifdef MKL
//        dgemm("N", "C", &ndrow3, &ndrow1, &supWdts, one, srcL, &nSNRCur,
//              src, &nSNRCur, zero, &contribs[ndrow1], &nSupRs);
//#endif
#ifdef CBLAS
        cblas_dgemm(CblasColMajor,CblasNoTrans,CblasConjTrans,ndrow3,
                    ndrow1,supWdts,one[0],srcL,nSNRCur,
                    src,nSNRCur,zero[0],contribs+ndrow1,nSupRs );
#endif
#ifdef MYBLAS
        //TODO
#endif
#ifdef BLASTIMING
        endBlas = std::chrono::system_clock::now();
        elapsed_seconds = (endBlas-startBlas);
        blasTimePerThread[threadID]+=elapsed_seconds.count();
#endif
       }
       //copying contrib to L
       for (int i = 0; i < ndrow1; ++i) {//Copy contribs to L
        int col = map[lR[Li_ptr_cSN + i + lb]];//col in the SN
        for (int j = i; j < nSupRs; ++j) {
         int cRow = lR[Li_ptr_cSN + j + lb];//corresponding row in SN
         //lValues[lC[curCol+col]+ map[cRow]] -= contribs[i*nSupRs+j];
         cur[col * nSupR + map[cRow]] -= contribs[i * nSupRs + j];
        }
       }
      }
      //MKL_Domain_Set_Num_Threads(1,MKL_DOMAIN_BLAS);
#ifdef BLASTIMING
      startBlas = std::chrono::system_clock::now();
#endif
#ifdef MKL
      dpotrf("L", &supWdt, cur, &nSupR, &info);
#endif
      if (info != 0)
       break;
#if defined(OPENBLAS) || defined(APPLEBLAS)
      dpotrf_("L",&supWdt,cur,&nSupR,&info);
#endif
#ifdef MYBLAS
       Cholesky_col(nSupR,supWdt,cur);
#endif

      int rowNo = nSupR - supWdt;
//#ifdef MKL
//      //MKL_Domain_Set_Num_Threads(4,MKL_DOMAIN_BLAS);
//      dtrsm("R", "L", "C", "N", &rowNo, &supWdt, one,
//            cur, &nSupR, &cur[supWdt], &nSupR);
//#endif
#ifdef CBLAS
      cblas_dtrsm(CblasColMajor, CblasRight, CblasLower, CblasConjTrans, CblasNonUnit,
                  rowNo, supWdt,one[0],
                  cur,nSupR,&cur[supWdt],nSupR);
#endif
#ifdef MYBLAS
      for (int i = supWdt; i < nSupR; ++i) {
                      lSolve_dense_col(nSupR,supWdt,cur,&cur[i]);
                  }//TODO
#endif
#ifdef BLASTIMING
      endBlas = std::chrono::system_clock::now();
      elapsed_seconds = (endBlas-startBlas);
      blasTimePerThread[threadID]+=elapsed_seconds.count();
#endif

      //        }
      /*int thth3=omp_get_thread_num();
      std::cout<<"-"<<thth3<<"-";*/
     }
#ifdef TIMING1
     endin = std::chrono::system_clock::now();
     elapsed_seconds = endin-startin;
     duration1=elapsed_seconds.count();
     int thth2=omp_get_thread_num();
     std::cout<<"**"<<thth2<<" : "<<j1<<" "<<duration1<<"\n";
#endif
//     delete[]xi;
//     delete[]contribs;
//     delete[]map;
#ifndef USE_TBB
     }
    }
#else
    }
    );
#endif
  if (info != 0){
   assert(false);// matrix is not SPD
   return false;
  }
 }


#ifdef BLASTIMING
 int threadID = omp_get_thread_num();
 std::chrono::time_point<std::chrono::system_clock> startBlas, endBlas;
#endif
#ifdef TIMING
 end = std::chrono::system_clock::now();
 elapsed_seconds = end - start;
 duration2 = elapsed_seconds.count();
 // thth=omp_get_thread_num();
 //std::cout<<duration2<<"; ";
 timing[0] = duration2;
#endif

#if 1
//  std::cout << omp_get_thread_num() << "-----";
#ifdef MKL
 MKL_Domain_Set_Num_Threads(threads, MKL_DOMAIN_BLAS);
#endif
#ifdef TIMING
 start = std::chrono::system_clock::now();
#endif
 int *map = map_list[0];// new int[n]();
 double *contribs = contribs_list[0];// new double[super_max * col_max]();
 int *xi = xi_list[0];// new int[2 * supNo]();
 for (int j1 = levelPtr[nLevels - 1]; j1 < levelPtr[nLevels]; ++j1) {
  for (int k1 = parPtr[j1]; k1 < parPtr[j1 + 1]; ++k1) {
   int s = partition[k1] + 1;
   int curCol = s != 0 ? blockSet[s - 1] : 0;
   int nxtCol = blockSet[s];
   int supWdt = nxtCol - curCol;
   int nSupR = Li_ptr[nxtCol] - Li_ptr[curCol];//row size of supernode
   for (int i = Li_ptr[curCol], cnt = 0; i < Li_ptr[nxtCol]; ++i) {
    map[lR[i]] = cnt++;//mapping L rows position to actual row idx
   }
   //copy the columns from A to L
   for (int i = curCol; i < nxtCol; ++i) {//Copy A to L
    int pad = i - curCol;
    for (int j = c[i]; j < c[i + 1]; ++j) {
     // if(r[j]>=i)//does not need to save upper part.
     lValues[lC[i] + map[r[j]]] = values[j];
     //   else
     //      printf("dddd\n");
    }
   }

   double *src, *cur = &lValues[lC[curCol]];//pointing to first element of the current supernode
#ifndef PRUNE
   top = ereach_sn(supNo, cT, rT, curCol, nxtCol, col2Sup, aTree, xi, xi + supNo);
   for (int i = top; i < supNo; ++i) {
    int lSN = xi[i];

#else
    for (int i = prunePtr[s - 1]; i < prunePtr[s]; ++i) {
      int lSN = pruneSet[i];
#endif
    int nSupRs = 0;
    int cSN = blockSet[lSN];//first col of current SN
    int cNSN = blockSet[lSN + 1];//first col of Next SN
    int Li_ptr_cNSN = Li_ptr[cNSN];
    int Li_ptr_cSN = Li_ptr[cSN];
    int nSNRCur = Li_ptr_cNSN - Li_ptr_cSN;
    int supWdts = cNSN - cSN;//The width of current src SN
    int lb = 0, ub = 0;
    bool sw = true;
    for (int j = Li_ptr_cSN; j < Li_ptr_cNSN; ++j) {
     //finding the overlap between curCol and curCol+supWdt in the src col
     if (lR[j] >= curCol && sw) {
      //src*transpose(row lR[j])
      lb = j - Li_ptr_cSN;
      sw = false;
     }
     if (lR[j] < curCol + supWdt && !sw) {
      ub = j - Li_ptr_cSN;
     }
    }
    nSupRs = Li_ptr_cNSN - Li_ptr_cSN - lb;
    int ndrow1 = ub - lb + 1;
    int ndrow3 = nSupRs - ndrow1;
    src = &lValues[lC[cSN] + lb];//first element of src supernode starting from row lb
    double *srcL = &lValues[lC[cSN] + ub + 1];
#ifdef BLASTIMING
    startBlas = std::chrono::system_clock::now();
#endif
/*#ifdef MKL // fortran interface
    dsyrk("L", "N", &ndrow1, &supWdts, one, src, &nSNRCur, zero,
          contribs, &nSupRs);
#endif*/
#ifdef CBLAS
    cblas_dsyrk(CblasColMajor, CblasLower, CblasNoTrans,ndrow1,supWdts,one[0],src,
                nSNRCur,zero[0],contribs, nSupRs);
#endif
#ifdef MYBLAS
    //TODO
#endif
    if (ndrow3 > 0) {
/*#ifdef MKL
     dgemm("N", "C", &ndrow3, &ndrow1, &supWdts, one, srcL, &nSNRCur,
           src, &nSNRCur, zero, &contribs[ndrow1], &nSupRs);
#endif*/
#ifdef CBLAS
     cblas_dgemm(CblasColMajor,CblasNoTrans,CblasConjTrans,ndrow3,
                 ndrow1,supWdts,one[0],srcL,nSNRCur,
                 src,nSNRCur,zero[0],contribs+ndrow1,nSupRs );
#endif
#ifdef MYBLAS
     //TODO
#endif
#ifdef BLASTIMING
     endBlas = std::chrono::system_clock::now();
     elapsed_seconds = (endBlas-startBlas);
     blasTimePerThread[threadID]+=elapsed_seconds.count();
#endif

    }
    //copying contrib to L
    for (int i = 0; i < ndrow1; ++i) {//Copy contribs to L
     int col = map[lR[Li_ptr_cSN + i + lb]];//col in the SN
     for (int j = i; j < nSupRs; ++j) {
      int cRow = lR[Li_ptr_cSN + j + lb];//corresponding row in SN
      //lValues[lC[curCol+col]+ map[cRow]] -= contribs[i*nSupRs+j];
      cur[col * nSupR + map[cRow]] -= contribs[i * nSupRs + j];
     }
    }
   }
#ifdef BLASTIMING
   startBlas = std::chrono::system_clock::now();
#endif
#ifdef MKL
   dpotrf("L", &supWdt, cur, &nSupR, &info);
#endif
#if defined(OPENBLAS) || defined(APPLEBLAS)
   dpotrf_("L",&supWdt,cur,&nSupR,&info);
#endif
#ifdef MYBLAS
   Cholesky_col(nSupR,supWdt,cur);
#endif

   int rowNo = nSupR - supWdt;
/*#ifdef MKL
   dtrsm("R", "L", "C", "N", &rowNo, &supWdt, one,
         cur, &nSupR, &cur[supWdt], &nSupR);
#endif*/
#ifdef CBLAS
   cblas_dtrsm(CblasColMajor, CblasRight, CblasLower, CblasConjTrans, CblasNonUnit,
               rowNo, supWdt,one[0],
               cur,nSupR,&cur[supWdt],nSupR);
#endif
#ifdef MYBLAS
   for (int i = supWdt; i < nSupR; ++i) {
            lSolve_dense_col(nSupR,supWdt,cur,&cur[i]);
        }//TODO
#endif
#ifdef BLASTIMING
   endBlas = std::chrono::system_clock::now();
   elapsed_seconds = (endBlas-startBlas);
   blasTimePerThread[threadID]+=elapsed_seconds.count();
#endif

  }
 }
#ifdef TIMING
 end = std::chrono::system_clock::now();
 elapsed_seconds = end - start;
 duration2 = elapsed_seconds.count();
 //thth=omp_get_thread_num();
 //std::cout<<duration2<<"; ";
 timing[1] = duration2;
#endif
// delete[]xi;
// delete[]contribs;
// delete[]map;
#endif

 for (int i = 0; i < threads; ++i) {
  delete []map_list[i];
  delete []contribs_list[i];
  delete []xi_list[i];
 }
 delete []map_list;
 delete []contribs_list;
 delete []xi_list;

 return true;
}

 }
}
#endif //CHOLOPENMP_PARALLEL_PB_CHOLESKY_05_H

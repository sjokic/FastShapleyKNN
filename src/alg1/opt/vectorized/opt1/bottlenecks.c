//
// Created by lucasck on 25/05/22.
//

#include <stdio.h>
#include "mat.h"
#include <math.h>
#include <string.h>
#include "utils.h"
#include "tsc_x86.h"
#include <immintrin.h>
// #ifndef T 130
#define NUM_RUNS 30

float l2norm_opt(float a[], float b[], size_t len){
    float* vresult = malloc(8*sizeof(float));
    float res_scalar = 0.0;
    __m256 a_vec0, b_vec0, a_vec1, b_vec1, a_vec2, b_vec2, a_vec3, b_vec3;
    __m256 sub_vec0, res_vec0, sub_vec1, res_vec1, sub_vec2, res_vec2, sub_vec3, res_vec3;
    __m256 tmp_vec0, tmp_vec1, res_vec;

    res_vec0 = _mm256_setzero_ps();
    res_vec1 = _mm256_setzero_ps();
    res_vec2 = _mm256_setzero_ps();
    res_vec3 = _mm256_setzero_ps();

    for (size_t i = 0; i < len; i+=32) {
        // load data
        a_vec0 = _mm256_loadu_ps(a+i);
        b_vec0 = _mm256_loadu_ps(b+i);
        a_vec1 = _mm256_loadu_ps(a+i+8);
        b_vec1 = _mm256_loadu_ps(b+i+8);
        a_vec2 = _mm256_loadu_ps(a+i+16);
        b_vec2 = _mm256_loadu_ps(b+i+16);
        a_vec3 = _mm256_loadu_ps(a+i+24);
        b_vec3 = _mm256_loadu_ps(b+i+24);
        // computations
        sub_vec0 = _mm256_sub_ps(a_vec0, b_vec0);
        sub_vec1 = _mm256_sub_ps(a_vec1, b_vec1);
        sub_vec2 = _mm256_sub_ps(a_vec2, b_vec2);
        sub_vec3 = _mm256_sub_ps(a_vec3, b_vec3);
        res_vec0 = _mm256_fmadd_ps(sub_vec0, sub_vec0, res_vec0);
        res_vec1 = _mm256_fmadd_ps(sub_vec1, sub_vec1, res_vec1);
        res_vec2 = _mm256_fmadd_ps(sub_vec2, sub_vec2, res_vec2);
        res_vec3 = _mm256_fmadd_ps(sub_vec3, sub_vec3, res_vec3);
    }
    // add up the separate accumulators
    tmp_vec0 = _mm256_add_ps(res_vec0, res_vec1);
    tmp_vec1 = _mm256_add_ps(res_vec2, res_vec3);
    res_vec = _mm256_add_ps(tmp_vec0, tmp_vec1);
    // store to vresult
    _mm256_storeu_ps(vresult, res_vec);
    // sum up vector elements
    for (int k = 0; k < 8; k++) {
        res_scalar += vresult[k];
    }
    free(vresult);
    return res_scalar;
}

// Merge two subarrays L and M into arr
void merge(float arr[], int idx[], int p, int q, int r) {

    // Create L ← A[p..q] and M ← A[q+1..r]
    int n1 = q - p + 1;
    int n2 = r - q;

    float L[n1], M[n2];
    int L_idx[n1], M_idx[n2];

    for (int i = 0; i < n1; i++) {
        L[i] = arr[p + i];
        L_idx[i] = idx[p + i];
    }
    for (int j = 0; j < n2; j++) {
        M[j] = arr[q + 1 + j];
        M_idx[j] = idx[q + 1 + j];
    }
    // Maintain current index of sub-arrays and main array
    int i, j, k;
    i = 0;
    j = 0;
    k = p;

    // Until we reach either end of either L or M, pick larger among
    // elements L and M and place them in the correct position at A[p..r]
    while (i < n1 && j < n2) {
        if (L[i] <= M[j]) {
            arr[k] = L[i];
            idx[k] = L_idx[i];
            i++;
        } else {
            arr[k] = M[j];
            idx[k] = M_idx[j];
            j++;
        }
        k++;
    }

    // When we run out of elements in either L or M,
    // pick up the remaining elements and put in A[p..r]
    while (i < n1) {
        arr[k] = L[i];
        idx[k] = L_idx[i];
        i++;
        k++;
    }

    while (j < n2) {
        arr[k] = M[j];
        idx[k] = M_idx[j];
        j++;
        k++;
    }
}

// Divide the array into two subarrays, sort them and merge them
void mergeSort(float arr[], int idx[], int l, int r) {
    if (l < r) {

        // m is the point where the array is divided into two subarrays
        int m = l + (r - l) / 2;

        mergeSort(arr, idx, l, m);
        mergeSort(arr, idx, m + 1, r);

        // Merge the sorted subarrays
        merge(arr, idx, l, m, r);
    }
}


// returns array of sorted indices according to distance
void argsort(int idx[], float arr[], int len) {
    float tmp[len];
    memcpy(tmp, arr, len * sizeof(float));
    // initialize array of indices
//    for (int i = 0; i < len; i++) {
//        idx[i] = i;
//    }
    mergeSort(tmp, idx, 0, len - 1);
}

// get knn, returns mat of sorted data entries (knn alg)
void get_true_knn(mat *x_tst_knn_gt, mat *x_trn, mat *x_tst){
    int N = x_trn->n1;
    int N_tst = x_tst->n1;
    int d = x_tst->n2;
    float* data_trn = x_trn->data;
    float* data_tst = x_tst->data;
    myInt64 start_l2norm, start_argsort, start, cycles_l2norm, cycles_argsort, cycles;
    cycles_l2norm = cycles_argsort = cycles = 0;

    start = start_tsc();
    for (int i_tst = 0; i_tst < N_tst; i_tst++){
        float dist_gt[N];
        int idx_arr[N];
        for (int i_trn = 0; i_trn < N; i_trn++){
            float trn_row[d];
            float tst_row[d];
            for (int j = 0; j < d; j++) {
                trn_row[j] = data_trn[i_trn * d + j];
                tst_row[j] = data_tst[i_tst * d + j];
            }
            idx_arr[i_trn] = i_trn;
            start_l2norm = start_tsc();
            dist_gt[i_trn] = l2norm_opt(trn_row, tst_row, d);
            cycles_l2norm += stop_tsc(start_l2norm);
        }
        start_argsort = start_tsc();
        argsort(idx_arr, dist_gt, N);
        cycles_argsort += stop_tsc(start_argsort);

        for (int k = 0; k < N; k++) {
            x_tst_knn_gt->data[i_tst * N + k] = idx_arr[k];
        }
    }
    cycles = stop_tsc(start);
    cycles = cycles - cycles_argsort - cycles_l2norm;
    printf("%lld,", cycles);
    printf("%lld,", cycles_argsort);
    printf("%lld,", cycles_l2norm);
}

void compute_single_unweighted_knn_class_shapley(mat* sp_gt, const int* y_trn, const int* y_tst, mat* x_tst_knn_gt, int K) {
    int N = x_tst_knn_gt->n2;
    int N_tst = x_tst_knn_gt->n1;
    float tmp0, tmp1, tmp2, tmp3;
    int x_tst_knn_gt_j_i, x_tst_knn_gt_j_i_plus_1, x_tst_knn_gt_j_last_i;

    for (int j=0; j < N_tst;j++){
        x_tst_knn_gt_j_last_i = (int) mat_get(x_tst_knn_gt, j, N-1);
        tmp0 = (y_trn[x_tst_knn_gt_j_last_i] == y_tst[j]) ? 1.0/N : 0.0;
        mat_set(sp_gt, j, x_tst_knn_gt_j_last_i, tmp0);
        for (int i=N-2; i>-1; i--){
            x_tst_knn_gt_j_i = (int) mat_get(x_tst_knn_gt, j, i);
            x_tst_knn_gt_j_i_plus_1 = (int) mat_get(x_tst_knn_gt, j, \
             i+1);
            tmp0 = (y_trn[x_tst_knn_gt_j_i] == y_tst[j]) ? 1.0:0.0;
            tmp1 = (y_trn[x_tst_knn_gt_j_i_plus_1] == y_tst[j]) ? 1.0:0.0;
            tmp2 = (K > i+1) ? i+1 : K;
            tmp3 = mat_get(sp_gt, j, x_tst_knn_gt_j_i_plus_1) + \
                    (tmp0 - tmp1) / K * tmp2 / (i + 1);
            mat_set(sp_gt, j, x_tst_knn_gt_j_i, tmp3);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 5){

        printf("No/not enough arguments given, please input N M d K");
        return 0;
    }
    int N = atoi(argv[1]);
    int M = atoi(argv[2]);
    int d = atoi(argv[3]);
    int K = atoi(argv[4]);


    printf("remaining_runtime,argsort,l2-norm,compute_shapley\n");

    for(int iter = 0; iter < NUM_RUNS; ++iter ) {
        mat x_tst_knn_gt;
        mat sp_gt;
        mat x_trn;
        mat x_tst;
        int* y_trn = malloc(N*sizeof(int));
        int* y_tst = malloc(M*sizeof(int));

        build(&sp_gt, M, N);
        build(&x_trn, N, d);
        build(&x_tst, M, d);
        build(&x_tst_knn_gt, M, N);

        myInt64 start, cycles;
        initialize_rand_array(y_trn, N);
        initialize_rand_array(y_tst, M);
        initialize_rand_mat(&x_trn);
        initialize_rand_mat(&x_tst);

        initialize_mat(&sp_gt, 0.0);

        get_true_knn(&x_tst_knn_gt, &x_trn, &x_tst);
        start = start_tsc();
        compute_single_unweighted_knn_class_shapley(&sp_gt, y_trn, y_tst, &x_tst_knn_gt, K);
        cycles = stop_tsc(start);
        printf("%lld", cycles);
        printf("\n");

        destroy(&x_trn);
        destroy(&x_tst);
        destroy(&sp_gt);
        destroy(&x_tst_knn_gt);
        free(y_trn);
        free(y_tst);
    }

    return 0;
}

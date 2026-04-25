/*
 * benchmark_stages.c - Staged Performance Benchmarking
 *
 * This file benchmarks the recommendation pipeline at different optimization
 * stages, allowing you to see the time improvement introduced by each change.
 *
 * It contains BOTH the original (unoptimized) and optimized versions of
 * each function, and runs them side-by-side for direct comparison.
 *
 * Compile:
 *   gcc -o benchmark_stages.exe benchmark_stages.c matrix_normalization.c pearsons.c kmeans.c predictions.c sorting.c -lm -O2
 *
 * Usage:
 *   benchmark_stages.exe [user_id] [num_iterations]
 *   - user_id:        User ID to benchmark with (default: 1)
 *   - num_iterations: Number of iterations for averaging (default: 10)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "matrix_normalization.h"
#include "pearsons.h"
#include "kmeans.h"
#include "predictions.h"
#include "sorting.h"

#define No_of_movies 9125

/* ─────────────────────── High-Resolution Timer ─────────────────────── */

typedef struct {
    #ifdef _WIN32
    LARGE_INTEGER start, end, freq;
    #else
    struct timeval start, end;
    #endif
} Timer;

void timer_start(Timer *t) {
    #ifdef _WIN32
    QueryPerformanceFrequency(&t->freq);
    QueryPerformanceCounter(&t->start);
    #else
    gettimeofday(&t->start, NULL);
    #endif
}

double timer_stop(Timer *t) {
    #ifdef _WIN32
    QueryPerformanceCounter(&t->end);
    return (double)(t->end.QuadPart - t->start.QuadPart) / (double)t->freq.QuadPart;
    #else
    gettimeofday(&t->end, NULL);
    return (double)(t->end.tv_sec - t->start.tv_sec) +
           (double)(t->end.tv_usec - t->start.tv_usec) / 1000000.0;
    #endif
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ORIGINAL (UNOPTIMIZED) FUNCTIONS — kept here for comparison
 * ═══════════════════════════════════════════════════════════════════════ */

/* Original findusers: uses strtok to parse all columns */
static int orig_findusers(void) {
    char *line, *record;
    char tmp[1024];
    FILE *fstream = fopen("Dataset/ratings_learn.csv", "r");
    if (!fstream) { printf("Error: Cannot open file\n"); exit(1); }
    int j = 0, max = 0;
    while ((line = fgets(tmp, sizeof(tmp), fstream)) != NULL) {
        record = strtok(line, ",");
        while (record != NULL) {
            if (j == 0) {
                int t = atoi(record);
                if (t > max) max = t;
            }
            j++;
            record = strtok(NULL, ",");
        }
        j = 0;
    }
    fclose(fstream);
    return max;
}

/* Original get_utility_matrix: uses strtok, separate from findusers */
static void orig_get_utility_matrix(double *utility_matrix, char *s, int No_of_movies_p, int No_of_users, int uid) {
    char *line, *record;
    char tmp[1024];
    int i = 0, j = 0, k = 0;
    FILE *fstream = fopen(s, "r");
    if (!fstream) { printf("Error: Cannot open file\n"); exit(1); }
    while ((line = fgets(tmp, sizeof(tmp), fstream)) != NULL) {
        record = strtok(line, ",");
        while (record != NULL) {
            if (k == 0) i = atoi(record) - 1;
            else if (k == 1) j = atoi(record) - 1;
            else utility_matrix[i * No_of_movies_p + j] = atof(record);
            record = strtok(NULL, ",");
            k++;
        }
        k = 0;
    }
    fclose(fstream);
}

/* Original new_user_movies: re-reads the CSV file */
static void orig_new_user_movies(double *newuser, char *s, int uid) {
    char *line, *record;
    char tmp[1024];
    int i, j, k = 0;
    FILE *fstream = fopen(s, "r");
    if (!fstream) { printf("Error: Cannot open file\n"); exit(1); }
    while ((line = fgets(tmp, sizeof(tmp), fstream)) != NULL) {
        record = strtok(line, ",");
        while (record != NULL) {
            if (k == 0) i = atoi(record) - 1;
            else if (k == 1) j = atoi(record) - 1;
            else if (k == 2) {
                if (i + 1 == uid) newuser[j] = atof(record);
            }
            k++;
            record = strtok(NULL, ",");
        }
        k = 0;
    }
    fclose(fstream);
}

/* Original calc_similarity: malloc+copy per user */
static void orig_calc_similarity(double *normalizeduser, double *normalized_matrix, double *similarity, int No_of_users, int No_of_movies_p) {
    int i, j;
    for (i = 0; i < No_of_users; i++) {
        double *A = (double *)malloc(sizeof(double) * No_of_movies_p);
        for (j = 0; j < No_of_movies_p; j++) {
            A[j] = normalized_matrix[i * No_of_movies_p + j];
        }
        /* inline pearson correlation */
        double dot_p = 0, mag_a = 0, mag_b = 0;
        for (j = 0; j < No_of_movies_p; j++) {
            dot_p += normalizeduser[j] * A[j];
            mag_a += normalizeduser[j] * normalizeduser[j];
            mag_b += A[j] * A[j];
        }
        similarity[i] = dot_p / (sqrt(mag_a) * sqrt(mag_b));
        free(A);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  OPTIMIZED FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Optimized: merged findusers + get_utility_matrix with fast CSV parsing */
static int opt_get_utility_matrix(double **utility_matrix_out, char *s, int No_of_movies_p, int uid) {
    char tmp[1024];
    FILE *fstream = fopen(s, "r");
    if (!fstream) { printf("Error: Cannot open file\n"); exit(1); }

    /* First pass: find No_of_users using fast atoi (no strtok) */
    int No_of_users = 0;
    while (fgets(tmp, sizeof(tmp), fstream) != NULL) {
        int t = atoi(tmp);
        if (t > No_of_users) No_of_users = t;
    }

    /* Allocate matrix */
    *utility_matrix_out = (double *)calloc(No_of_users * No_of_movies_p, sizeof(double));
    if (!*utility_matrix_out) { printf("Error: Alloc failed\n"); exit(1); }

    /* Second pass: fill matrix using manual comma-scan (no strtok) */
    rewind(fstream);
    while (fgets(tmp, sizeof(tmp), fstream) != NULL) {
        char *p = tmp;
        int i = atoi(p) - 1;
        while (*p != ',' && *p != '\0') p++; if (*p == ',') p++;
        int j = atoi(p) - 1;
        while (*p != ',' && *p != '\0') p++; if (*p == ',') p++;
        (*utility_matrix_out)[i * No_of_movies_p + j] = atof(p);
    }
    fclose(fstream);
    return No_of_users;
}

/* Optimized new_user_movies: copy from utility matrix (no file I/O) */
static void opt_new_user_movies(double *newuser, double *utility_matrix, int uid, int No_of_movies_p) {
    int j;
    for (j = 0; j < No_of_movies_p; j++) {
        newuser[j] = utility_matrix[(uid - 1) * No_of_movies_p + j];
    }
}

/* Optimized calc_similarity: direct pointer to matrix row (no malloc/copy) */
static void opt_calc_similarity(double *normalizeduser, double *normalized_matrix, double *similarity, int No_of_users, int No_of_movies_p) {
    int i, j;
    for (i = 0; i < No_of_users; i++) {
        double *A = &normalized_matrix[i * No_of_movies_p];
        double dot_p = 0, mag_a = 0, mag_b = 0;
        for (j = 0; j < No_of_movies_p; j++) {
            dot_p += normalizeduser[j] * A[j];
            mag_a += normalizeduser[j] * normalizeduser[j];
            mag_b += A[j] * A[j];
        }
        similarity[i] = dot_p / (sqrt(mag_a) * sqrt(mag_b));
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  SHARED PIPELINE (clustering + prediction + sort)
 * ═══════════════════════════════════════════════════════════════════════ */

static double run_shared_pipeline(int No_of_users, int k, double *similarity,
                                    double *newuser, double *utility_matrix,
                                    char *movienames, char *moviegenres) {
    Timer t;
    int i;

    double *centroids = (double *)malloc(sizeof(double) * k);
    int *cluster_assignment = (int *)malloc(sizeof(int) * No_of_users);

    for (i = 0; i < k; i++) {
        int n = rand() % No_of_users;
        int m, flag = 0;
        for (m = 0; m < i; m++) {
            if (similarity[n] == centroids[m]) { flag = 1; break; }
        }
        if (flag) { i--; continue; }
        centroids[i] = similarity[n];
    }

    kmeans(1, similarity, No_of_users, k, centroids, cluster_assignment);

    int *similar_users = (int *)malloc(sizeof(int) * No_of_users);
    int no_of_susers = 0;
    double max = 0; int maxid = 0;
    for (i = 0; i < k; i++) {
        if (centroids[i] > max && centroids[i] < 0.3) { max = centroids[i]; maxid = i; }
    }
    for (i = 0; i < No_of_users; i++) {
        if (cluster_assignment[i] == maxid) { similar_users[no_of_susers++] = i; }
    }

    int *recommended_movies = (int *)malloc(sizeof(int) * No_of_movies);
    double *predicted_ratings = (double *)malloc(sizeof(double) * No_of_movies);
    int no_rec = make_prediction(newuser, similar_users, no_of_susers,
        similarity, utility_matrix, recommended_movies, predicted_ratings, No_of_movies);
    sort(recommended_movies, predicted_ratings, no_rec);

    free(centroids); free(cluster_assignment); free(similar_users);
    free(recommended_movies); free(predicted_ratings);

    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  STAGE RUNNERS
 * ═══════════════════════════════════════════════════════════════════════ */

/* Stage 0: Fully original (baseline) */
static double run_stage_original(int userid) {
    Timer t, total;
    timer_start(&total);

    int No_of_users = orig_findusers();
    int k = 16;

    double *utility_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    char *movienames = (char *)malloc(sizeof(char) * No_of_movies * 1024);
    char *moviegenres = (char *)malloc(sizeof(char) * No_of_movies * 1024);

    orig_get_utility_matrix(utility_matrix, "Dataset/ratings_learn.csv", No_of_movies, No_of_users, userid);

    /* We skip movie name/genre loading since it's identical in both versions */

    double *normalized_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    normalize_matrix(utility_matrix, normalized_matrix, No_of_users, No_of_movies);

    double *newuser = (double *)calloc(No_of_movies, sizeof(double));
    orig_new_user_movies(newuser, "Dataset/ratings_learn.csv", userid);

    double *normalizednewuser = (double *)calloc(No_of_movies, sizeof(double));
    normalize(newuser, normalizednewuser, No_of_movies);

    double *similarity = (double *)malloc(sizeof(double) * No_of_users);
    orig_calc_similarity(normalizednewuser, normalized_matrix, similarity, No_of_users, No_of_movies);

    run_shared_pipeline(No_of_users, k, similarity, newuser, utility_matrix, movienames, moviegenres);

    double elapsed = timer_stop(&total);

    free(utility_matrix); free(movienames); free(moviegenres);
    free(normalized_matrix); free(newuser); free(normalizednewuser); free(similarity);

    return elapsed;
}

/* Stage 1: Optimized calc_similarity only */
static double run_stage_1(int userid) {
    Timer total;
    timer_start(&total);

    int No_of_users = orig_findusers();
    int k = 16;

    double *utility_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    char *movienames = (char *)malloc(sizeof(char) * No_of_movies * 1024);
    char *moviegenres = (char *)malloc(sizeof(char) * No_of_movies * 1024);

    orig_get_utility_matrix(utility_matrix, "Dataset/ratings_learn.csv", No_of_movies, No_of_users, userid);

    double *normalized_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    normalize_matrix(utility_matrix, normalized_matrix, No_of_users, No_of_movies);

    double *newuser = (double *)calloc(No_of_movies, sizeof(double));
    orig_new_user_movies(newuser, "Dataset/ratings_learn.csv", userid);

    double *normalizednewuser = (double *)calloc(No_of_movies, sizeof(double));
    normalize(newuser, normalizednewuser, No_of_movies);

    double *similarity = (double *)malloc(sizeof(double) * No_of_users);
    /* OPTIMIZED: direct pointer instead of malloc+copy */
    opt_calc_similarity(normalizednewuser, normalized_matrix, similarity, No_of_users, No_of_movies);

    run_shared_pipeline(No_of_users, k, similarity, newuser, utility_matrix, movienames, moviegenres);

    double elapsed = timer_stop(&total);
    free(utility_matrix); free(movienames); free(moviegenres);
    free(normalized_matrix); free(newuser); free(normalizednewuser); free(similarity);
    return elapsed;
}

/* Stage 2: + Optimized new_user_movies (memory copy instead of file re-read) */
static double run_stage_2(int userid) {
    Timer total;
    timer_start(&total);

    int No_of_users = orig_findusers();
    int k = 16;

    double *utility_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    char *movienames = (char *)malloc(sizeof(char) * No_of_movies * 1024);
    char *moviegenres = (char *)malloc(sizeof(char) * No_of_movies * 1024);

    orig_get_utility_matrix(utility_matrix, "Dataset/ratings_learn.csv", No_of_movies, No_of_users, userid);

    double *normalized_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    normalize_matrix(utility_matrix, normalized_matrix, No_of_users, No_of_movies);

    double *newuser = (double *)calloc(No_of_movies, sizeof(double));
    /* OPTIMIZED: copy from matrix instead of re-reading file */
    opt_new_user_movies(newuser, utility_matrix, userid, No_of_movies);

    double *normalizednewuser = (double *)calloc(No_of_movies, sizeof(double));
    normalize(newuser, normalizednewuser, No_of_movies);

    double *similarity = (double *)malloc(sizeof(double) * No_of_users);
    opt_calc_similarity(normalizednewuser, normalized_matrix, similarity, No_of_users, No_of_movies);

    run_shared_pipeline(No_of_users, k, similarity, newuser, utility_matrix, movienames, moviegenres);

    double elapsed = timer_stop(&total);
    free(utility_matrix); free(movienames); free(moviegenres);
    free(normalized_matrix); free(newuser); free(normalizednewuser); free(similarity);
    return elapsed;
}

/* Stage 3: + Merged findusers into get_utility_matrix with fast CSV parsing */
static double run_stage_3(int userid) {
    Timer total;
    timer_start(&total);

    int k = 16;

    /* OPTIMIZED: single function does findusers + matrix build + fast parsing */
    double *utility_matrix;
    int No_of_users = opt_get_utility_matrix(&utility_matrix, "Dataset/ratings_learn.csv", No_of_movies, userid);

    char *movienames = (char *)malloc(sizeof(char) * No_of_movies * 1024);
    char *moviegenres = (char *)malloc(sizeof(char) * No_of_movies * 1024);

    double *normalized_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    normalize_matrix(utility_matrix, normalized_matrix, No_of_users, No_of_movies);

    double *newuser = (double *)calloc(No_of_movies, sizeof(double));
    opt_new_user_movies(newuser, utility_matrix, userid, No_of_movies);

    double *normalizednewuser = (double *)calloc(No_of_movies, sizeof(double));
    normalize(newuser, normalizednewuser, No_of_movies);

    double *similarity = (double *)malloc(sizeof(double) * No_of_users);
    opt_calc_similarity(normalizednewuser, normalized_matrix, similarity, No_of_users, No_of_movies);

    run_shared_pipeline(No_of_users, k, similarity, newuser, utility_matrix, movienames, moviegenres);

    double elapsed = timer_stop(&total);
    free(utility_matrix); free(movienames); free(moviegenres);
    free(normalized_matrix); free(newuser); free(normalizednewuser); free(similarity);
    return elapsed;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  MAIN — Runs all stages and prints comparison table
 * ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    int userid = 1;
    int iterations = 10;
    int i, s;

    if (argc >= 2) userid = atoi(argv[1]);
    if (argc >= 3) iterations = atoi(argv[2]);

    if (userid < 1) { printf("Error: user_id must be >= 1\n"); return 1; }
    if (iterations < 1) { printf("Error: iterations must be >= 1\n"); return 1; }

    srand((unsigned int)time(NULL));

    const char *stage_names[] = {
        "Stage 0: Original (baseline)",
        "Stage 1: + Optimized calc_similarity",
        "Stage 2: + Memory-based new_user_movies",
        "Stage 3: + Merged findusers + fast CSV parsing"
    };

    typedef double (*stage_func)(int);
    stage_func stages[] = {
        run_stage_original,
        run_stage_1,
        run_stage_2,
        run_stage_3
    };

    int num_stages = 4;
    double avg_times[4] = {0};

    printf("========================================================================\n");
    printf("  STAGED PERFORMANCE BENCHMARK\n");
    printf("  User ID: %d | Iterations: %d\n", userid, iterations);
    printf("========================================================================\n\n");

    for (s = 0; s < num_stages; s++) {
        printf("  Running %s ...\n", stage_names[s]);
        double total = 0;
        for (i = 0; i < iterations; i++) {
            total += stages[s](userid);
        }
        avg_times[s] = total / iterations;
        printf("    Avg: %.6f s\n\n", avg_times[s]);
    }

    /* ─── Print comparison table ─── */
    printf("\n========================================================================\n");
    printf("  OPTIMIZATION PROGRESSION — RESULTS\n");
    printf("========================================================================\n\n");

    printf("+----+---------------------------------------------+------------+----------+-----------+\n");
    printf("| #  | Stage                                       | Avg Time   | Speedup  | Reduction |\n");
    printf("+----+---------------------------------------------+------------+----------+-----------+\n");

    for (s = 0; s < num_stages; s++) {
        double speedup = (s == 0) ? 1.0 : avg_times[0] / avg_times[s];
        double reduction = (s == 0) ? 0.0 : (1.0 - avg_times[s] / avg_times[0]) * 100.0;
        printf("| %2d | %-43s | %8.6f s | %6.2fx  | %6.1f %%  |\n",
               s, stage_names[s], avg_times[s], speedup, reduction);
    }

    printf("+----+---------------------------------------------+------------+----------+-----------+\n");

    printf("\n  Baseline time:  %.6f s\n", avg_times[0]);
    printf("  Final time:     %.6f s\n", avg_times[num_stages - 1]);
    printf("  Total speedup:  %.2fx\n", avg_times[0] / avg_times[num_stages - 1]);
    printf("  Total reduction: %.1f%%\n\n", (1.0 - avg_times[num_stages - 1] / avg_times[0]) * 100.0);

    return 0;
}

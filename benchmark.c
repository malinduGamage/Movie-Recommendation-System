/*
 * benchmark.c - Performance benchmarking for the Movie Recommendation System
 *
 * This file runs the recommendation pipeline multiple times (without any user
 * input) and measures the average execution time of each major function.
 * It uses Windows high-resolution performance counters for accurate timing.
 *
 * Compile (MSVC):
 *   cl benchmark.c recommender.c utility_matrix.c matrix_normalization.c pearsons.c kmeans.c predictions.c sorting.c /Fe:benchmark.exe
 *
 * Compile (GCC / MinGW):
 *   gcc -o benchmark.exe benchmark.c recommender.c utility_matrix.c matrix_normalization.c pearsons.c kmeans.c predictions.c sorting.c -lm
 *
 * Usage:
 *   benchmark.exe [user_id] [num_iterations]
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

#include "utility_matrix.h"
#include "matrix_normalization.h"
#include "pearsons.h"
#include "kmeans.h"
#include "predictions.h"
#include "sorting.h"

#define No_of_movies 9125

/* ─────────────────────── High-Resolution Timer ─────────────────────── */

typedef struct {
    #ifdef _WIN32
    LARGE_INTEGER start;
    LARGE_INTEGER end;
    LARGE_INTEGER freq;
    #else
    struct timeval start;
    struct timeval end;
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

/* ────────────────────── Timing Result Struct ────────────────────────── */

typedef struct {
    double get_utility_matrix_time;  /* includes finding No_of_users + building matrix */
    double get_movie_names_time;
    double get_movie_genres_time;
    double normalize_matrix_time;
    double new_user_movies_time;
    double normalize_user_time;
    double calc_similarity_time;
    double centroid_init_time;
    double kmeans_time;
    double cluster_selection_time;
    double make_prediction_time;
    double sort_time;
    double total_time;
} BenchmarkResult;

/* ──────────────── Single iteration of the recommendation pipeline ──── */

BenchmarkResult run_benchmark_iteration(int userid) {
    BenchmarkResult result;
    memset(&result, 0, sizeof(result));
    Timer t;
    int i;

    Timer total_timer;
    timer_start(&total_timer);

    int k = 16;  /* number of clusters */

    /* 1. get_utility_matrix (includes finding No_of_users + allocating + filling) */
    double *utility_matrix;
    timer_start(&t);
    int No_of_users = get_utility_matrix(&utility_matrix, "Dataset/ratings_learn.csv", No_of_movies, userid);
    result.get_utility_matrix_time = timer_stop(&t);

    char *movienames = (char *)malloc(sizeof(char) * No_of_movies * 1024);
    char *moviegenres = (char *)malloc(sizeof(char) * No_of_movies * 1024);

    if (!utility_matrix || !movienames || !moviegenres) {
        printf("Error: Memory allocation failed.\n");
        exit(1);
    }

    /* 2. get_movie_names */
    timer_start(&t);
    get_movie_names(movienames, "Dataset/movies.csv");
    result.get_movie_names_time = timer_stop(&t);

    /* 4. get_movie_genres */
    timer_start(&t);
    get_movie_genres(moviegenres, "Dataset/movies_genres.csv");
    result.get_movie_genres_time = timer_stop(&t);

    /* 5. normalize_matrix */
    double *normalized_matrix = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
    timer_start(&t);
    normalize_matrix(utility_matrix, normalized_matrix, No_of_users, No_of_movies);
    result.normalize_matrix_time = timer_stop(&t);

    /* 6. new_user_movies */
    double *newuser = (double *)calloc(No_of_movies, sizeof(double));
    timer_start(&t);
    new_user_movies(newuser, utility_matrix, userid, No_of_movies);
    result.new_user_movies_time = timer_stop(&t);

    /* 7. normalize (new user) */
    double *normalizednewuser = (double *)calloc(No_of_movies, sizeof(double));
    timer_start(&t);
    normalize(newuser, normalizednewuser, No_of_movies);
    result.normalize_user_time = timer_stop(&t);

    /* 8. calc_similarity */
    double *similarity = (double *)malloc(sizeof(double) * No_of_users);
    timer_start(&t);
    calc_similarity(normalizednewuser, normalized_matrix, similarity, No_of_users, No_of_movies);
    result.calc_similarity_time = timer_stop(&t);

    /* 9. Centroid initialization */
    double *centroids = (double *)malloc(sizeof(double) * k);
    int *cluster_assignment = (int *)malloc(sizeof(int) * No_of_users);
    timer_start(&t);
    for (i = 0; i < k; i++) {
        int n = rand() % No_of_users;
        int m = 0, flag = 0;
        for (m = 0; m < i; m++) {
            if (similarity[n] == centroids[m]) {
                flag = 1;
                break;
            }
        }
        if (flag == 1) { i--; continue; }
        centroids[i] = similarity[n];
    }
    result.centroid_init_time = timer_stop(&t);

    /* 10. kmeans clustering */
    timer_start(&t);
    kmeans(1, similarity, No_of_users, k, centroids, cluster_assignment);
    result.kmeans_time = timer_stop(&t);

    /* 11. Cluster selection (finding most similar users) */
    int *similar_users = (int *)malloc(sizeof(int) * No_of_users);
    int no_of_susers = 0;
    timer_start(&t);
    double max = 0;
    int maxid = 0;
    for (i = 0; i < k; i++) {
        if (centroids[i] > max && centroids[i] < 0.3) {
            max = centroids[i];
            maxid = i;
        }
    }
    for (i = 0; i < No_of_users; i++) {
        if (cluster_assignment[i] == maxid) {
            similar_users[no_of_susers] = i;
            no_of_susers++;
        }
    }
    result.cluster_selection_time = timer_stop(&t);

    /* 12. make_prediction */
    int *recommended_movies = (int *)malloc(sizeof(int) * No_of_movies);
    double *predicted_ratings = (double *)malloc(sizeof(double) * No_of_movies);
    int no_of_recommended_movies = 0;
    timer_start(&t);
    no_of_recommended_movies = make_prediction(newuser, similar_users, no_of_susers,
        similarity, utility_matrix, recommended_movies, predicted_ratings, No_of_movies);
    result.make_prediction_time = timer_stop(&t);

    /* 13. sort */
    timer_start(&t);
    sort(recommended_movies, predicted_ratings, no_of_recommended_movies);
    result.sort_time = timer_stop(&t);

    /* Total pipeline time */
    result.total_time = timer_stop(&total_timer);

    /* Free all memory */
    free(utility_matrix);
    free(movienames);
    free(moviegenres);
    free(normalized_matrix);
    free(newuser);
    free(normalizednewuser);
    free(similarity);
    free(centroids);
    free(cluster_assignment);
    free(similar_users);
    free(recommended_movies);
    free(predicted_ratings);

    return result;
}

/* ──────────────────────────── Helper: add results ───────────────────── */

void accumulate_result(BenchmarkResult *acc, const BenchmarkResult *r) {
    acc->get_utility_matrix_time += r->get_utility_matrix_time;
    acc->get_movie_names_time   += r->get_movie_names_time;
    acc->get_movie_genres_time  += r->get_movie_genres_time;
    acc->normalize_matrix_time  += r->normalize_matrix_time;
    acc->new_user_movies_time   += r->new_user_movies_time;
    acc->normalize_user_time    += r->normalize_user_time;
    acc->calc_similarity_time   += r->calc_similarity_time;
    acc->centroid_init_time     += r->centroid_init_time;
    acc->kmeans_time            += r->kmeans_time;
    acc->cluster_selection_time += r->cluster_selection_time;
    acc->make_prediction_time   += r->make_prediction_time;
    acc->sort_time              += r->sort_time;
    acc->total_time             += r->total_time;
}

void scale_result(BenchmarkResult *r, double factor) {
    r->get_utility_matrix_time *= factor;
    r->get_movie_names_time   *= factor;
    r->get_movie_genres_time  *= factor;
    r->normalize_matrix_time  *= factor;
    r->new_user_movies_time   *= factor;
    r->normalize_user_time    *= factor;
    r->calc_similarity_time   *= factor;
    r->centroid_init_time     *= factor;
    r->kmeans_time            *= factor;
    r->cluster_selection_time *= factor;
    r->make_prediction_time   *= factor;
    r->sort_time              *= factor;
    r->total_time             *= factor;
}

/* ──────────────────────────── Print Report ──────────────────────────── */

void print_separator(void) {
    printf("+--------------------------------------+----------------+----------------+\n");
}

void print_row(const char *label, double avg_sec, double total_sec) {
    double pct = (total_sec > 0.0) ? (avg_sec / total_sec) * 100.0 : 0.0;
    printf("| %-36s | %11.6f s  | %11.2f %%  |\n", label, avg_sec, pct);
}

void print_report(const BenchmarkResult *avg, int iterations, int userid) {
    printf("\n");
    printf("========================================================================\n");
    printf("       MOVIE RECOMMENDATION SYSTEM - PERFORMANCE BENCHMARK REPORT       \n");
    printf("========================================================================\n");
    if (iterations > 1) {
        printf("  User IDs         : %d to %d\n", userid, userid + iterations - 1);
    } else {
        printf("  User ID          : %d\n", userid);
    }
    printf("  Iterations       : %d\n", iterations);
    printf("  Avg Total Time   : %.6f seconds\n", avg->total_time);
    printf("========================================================================\n\n");

    print_separator();
    printf("| %-36s | %-14s | %-14s |\n", "Function / Stage", "Avg Time", "% of Total");
    print_separator();

    /* Data Loading */
    printf("| %-36s |                |                |\n", "--- DATA LOADING ---");
    print_row("get_utility_matrix()",     avg->get_utility_matrix_time, avg->total_time);
    print_row("get_movie_names()",        avg->get_movie_names_time,    avg->total_time);
    print_row("get_movie_genres()",       avg->get_movie_genres_time,   avg->total_time);
    print_row("new_user_movies()",        avg->new_user_movies_time,    avg->total_time);
    print_separator();

    /* Normalization */
    printf("| %-36s |                |                |\n", "--- NORMALIZATION ---");
    print_row("normalize_matrix()",       avg->normalize_matrix_time,   avg->total_time);
    print_row("normalize() [user]",       avg->normalize_user_time,     avg->total_time);
    print_separator();

    /* Similarity & Clustering */
    printf("| %-36s |                |                |\n", "--- SIMILARITY & CLUSTERING ---");
    print_row("calc_similarity()",        avg->calc_similarity_time,    avg->total_time);
    print_row("Centroid initialization",  avg->centroid_init_time,      avg->total_time);
    print_row("kmeans()",                 avg->kmeans_time,             avg->total_time);
    print_row("Cluster selection",        avg->cluster_selection_time,  avg->total_time);
    print_separator();

    /* Prediction & Output */
    printf("| %-36s |                |                |\n", "--- PREDICTION & OUTPUT ---");
    print_row("make_prediction()",        avg->make_prediction_time,    avg->total_time);
    print_row("sort()",                   avg->sort_time,               avg->total_time);
    print_separator();

    /* Summary */
    double data_loading = avg->get_utility_matrix_time +
                          avg->get_movie_names_time + avg->get_movie_genres_time +
                          avg->new_user_movies_time;
    double normalization = avg->normalize_matrix_time + avg->normalize_user_time;
    double similarity    = avg->calc_similarity_time + avg->centroid_init_time +
                           avg->kmeans_time + avg->cluster_selection_time;
    double prediction    = avg->make_prediction_time + avg->sort_time;

    printf("\n");
    print_separator();
    printf("| %-36s | %-14s | %-14s |\n", "Category Summary", "Avg Time", "% of Total");
    print_separator();
    print_row("Data Loading (total)",     data_loading,    avg->total_time);
    print_row("Normalization (total)",    normalization,   avg->total_time);
    print_row("Similarity+Clustering",   similarity,      avg->total_time);
    print_row("Prediction+Sorting",      prediction,      avg->total_time);
    print_separator();
    print_row("TOTAL PIPELINE",           avg->total_time, avg->total_time);
    print_separator();

    printf("\n========================================================================\n");
    printf("  BOTTLENECK: ");

    /* Identify the biggest bottleneck */
    const char *bottleneck = "Unknown";
    double max_time = 0;

    struct { const char *name; double time; } stages[] = {
        {"get_utility_matrix()",  avg->get_utility_matrix_time},
        {"get_movie_names()",     avg->get_movie_names_time},
        {"get_movie_genres()",    avg->get_movie_genres_time},
        {"new_user_movies()",     avg->new_user_movies_time},
        {"normalize_matrix()",    avg->normalize_matrix_time},
        {"normalize() [user]",    avg->normalize_user_time},
        {"calc_similarity()",     avg->calc_similarity_time},
        {"Centroid init",         avg->centroid_init_time},
        {"kmeans()",              avg->kmeans_time},
        {"Cluster selection",     avg->cluster_selection_time},
        {"make_prediction()",     avg->make_prediction_time},
        {"sort()",                avg->sort_time},
    };

    int num_stages = sizeof(stages) / sizeof(stages[0]);
    int i;
    for (i = 0; i < num_stages; i++) {
        if (stages[i].time > max_time) {
            max_time = stages[i].time;
            bottleneck = stages[i].name;
        }
    }
    printf("%s (%.6f s, %.1f%% of total)\n", bottleneck, max_time,
           (avg->total_time > 0) ? (max_time / avg->total_time) * 100.0 : 0.0);
    printf("========================================================================\n\n");
}

/* ────────────────────────────── main ────────────────────────────────── */

int main(int argc, char *argv[]) {
    int userid = 1;             /* default user ID */
    int iterations = 10;        /* default number of iterations */
    int i;

    if (argc >= 2) userid = atoi(argv[1]);
    if (argc >= 3) iterations = atoi(argv[2]);

    if (userid < 1) {
        printf("Error: user_id must be >= 1\n");
        return 1;
    }
    if (iterations < 1) {
        printf("Error: num_iterations must be >= 1\n");
        return 1;
    }

    srand((unsigned int)time(NULL));

    printf("========================================================================\n");
    printf("  Starting benchmark: user_id=%d, iterations=%d\n", userid, iterations);
    printf("========================================================================\n");

    BenchmarkResult accumulator;
    memset(&accumulator, 0, sizeof(accumulator));

    /* Store individual results for min/max analysis */
    BenchmarkResult *all_results = (BenchmarkResult *)malloc(sizeof(BenchmarkResult) * iterations);

    for (i = 0; i < iterations; i++) {
        int current_userid = userid + i;
        printf("  Running iteration %d/%d (user_id=%d)...\n", i + 1, iterations, current_userid);
        all_results[i] = run_benchmark_iteration(current_userid);
        accumulate_result(&accumulator, &all_results[i]);
    }

    /* Compute averages */
    BenchmarkResult avg = accumulator;
    scale_result(&avg, 1.0 / iterations);

    /* Print detailed report */
    print_report(&avg, iterations, userid);

    /* Print per-iteration total times for consistency check */
    printf("  Per-iteration total times:\n");
    printf("  +-------+----------------+\n");
    printf("  | Iter  |    Time (s)    |\n");
    printf("  +-------+----------------+\n");
    for (i = 0; i < iterations; i++) {
        printf("  | %5d | %12.6f s |\n", i + 1, all_results[i].total_time);
    }
    printf("  +-------+----------------+\n");

    /* Compute standard deviation of total time */
    double sum_sq_diff = 0.0;
    for (i = 0; i < iterations; i++) {
        double diff = all_results[i].total_time - avg.total_time;
        sum_sq_diff += diff * diff;
    }
    double std_dev = sqrt(sum_sq_diff / iterations);
    printf("  Standard Deviation: %.6f s\n", std_dev);
    printf("  Coefficient of Variation: %.2f%%\n\n",
           (avg.total_time > 0) ? (std_dev / avg.total_time) * 100.0 : 0.0);

    free(all_results);

    return 0;
}

# Computational Optimization of a Recommender System
**In 21-S8-CS4553 - Scientific Computing**  
**210176F - Gamage M.S.**

## 1. Execution Environment
- **Operating System:** Microsoft Windows 10 Home (Build 19045), x64
- **Processor:** 11th Gen Intel Core i7-1165G7 @ 2.80GHz
- **RAM:** 7,919 MB DDR4 (8GB)
- **Compiler:** GCC 13.2.0 (MinGW-W64 x86_64-ucrt-posix-seh)
- **Compiler Flags:** `-O2 -lm`
- **Dataset:** Provided dataset

## 2. Profiling Methodology and Tools
- A dedicated benchmarking tool (`benchmark.c`) was developed to measure execution time for each major function in the recommendation pipeline.
- Uses Windows `QueryPerformanceCounter` API for microsecond-level accuracy.
- Each measurement is averaged over multiple iterations (10 runs).
- Each stage of the pipeline is individually timed.

## 3. Bottleneck Analysis (Baseline Profile)

### 3.1 Baseline Function-Level Timing

| Function | Time (s) | % of Total | Category |
|---|---|---|---|
| `get_utility_matrix()` | 0.02660 | 29.59% | Data Loading |
| `normalize_matrix()` | 0.01784 | 19.84% | Normalization |
| `new_user_movies()` | 0.01338 | 14.88% | Data Loading |
| `findusers()` | 0.01241 | 13.81% | Data Loading |
| `calc_similarity()` | 0.01144 | 12.72% | Similarity |
| `get_movie_names()` | 0.00419 | 4.66% | Data Loading |
| `get_movie_genres()` | 0.00339 | 3.76% | Data Loading |
| `kmeans()` | 0.00046 | 0.52% | Clustering |
| `make_prediction()` | 0.00008 | 0.09% | Prediction |
| `sort()` | 0.00001 | 0.01% | Output |
| **Total** | **0.08991** | **100%** | |

### 3.2 Category Summary

| Category | Time (s) | % of Total |
|---|---|---|
| Data Loading | 0.05997 | 66.70% |
| Normalization | 0.01785 | 19.85% |
| Similarity + Clustering | 0.01190 | 13.24% |
| Prediction + Sorting | 0.00008 | 0.09% |

### 3.3 Key Observations
- Data loading accounts for 67% of total time — primarily due to CSV parsing with `strtok` + `atoi` + `atof` and redundant file reads.
- `new_user_movies()` re-reads the same file that `get_utility_matrix()` already loaded.
- `findusers()` also reads the same file just to find the maximum user ID.
- `calc_similarity()` allocates and frees memory per user in a loop.

## 4. Optimization Strategies

### 4.1 Optimization 1: Eliminate Memory Allocation in `calc_similarity()`
- **Problem**: The function allocates a temporary array (`malloc`), copies an entire row from the normalized matrix into it, computes similarity, then frees it. It's repeated for every user.
- **Solution**: Since matrix rows are stored contiguously in memory, we can point directly to the row using pointer arithmetic instead of copying.

```c
// BEFORE: malloc + copy + free per user
double *A = (double *)malloc(sizeof(double) * No_of_movies);
for(j = 0; j < No_of_movies; j++)
   A[j] = normalized_matrix[i * No_of_movies + j];
similarity[i] = pearson_correlation(normalizeduser, A, No_of_movies);
free(A);

// AFTER: direct pointer — zero allocation, zero copy
double *A = &normalized_matrix[i * No_of_movies];
similarity[i] = pearson_correlation(normalizeduser, A, No_of_movies);
```
**File modified:** `pearsons.c`

### 4.2 Optimization 2: Replace File I/O with Memory Copy in `new_user_movies()`
- **Problem**: `new_user_movies()` opens and parses the entire `ratings_learn.csv` file to extract one user's ratings. However, this exact data was already loaded into the utility matrix by `get_utility_matrix()` moments earlier.
- **Solution**: Copy directly from the utility matrix (already in RAM) instead of re-reading from disk.

```c
// BEFORE: re-read entire CSV file, parse all lines
FILE *fstream = fopen(s, "r");
while(fgets(tmp, sizeof(tmp), fstream) != NULL) {
   // strtok + atoi + atof parsing for every line...
   if(userId == uid) newuser[movieId] = rating;
}

// AFTER: simple memory copy from utility matrix
void new_user_movies(double *newuser, double *utility_matrix, int uid, int No_of_movies) {
   for(int j = 0; j < No_of_movies; j++)
       newuser[j] = utility_matrix[(uid-1) * No_of_movies + j];
}
```
**Files modified:** `utility_matrix.c`, `utility_matrix.h`, `recommender.c`, `benchmark.c`

### 4.3 Optimization 3: Merge `findusers()` into `get_utility_matrix()`
- **Problem**: Two separate functions read the same `ratings_learn.csv` file: `findusers()` (scans for max user ID) and `get_utility_matrix()` (parses all ratings). The file is opened, read, and closed twice.
- **Solution**: Merge both operations into a single function using a two-pass approach with `rewind()`.

```c
int get_utility_matrix(double **matrix_out, char *s, int No_of_movies, int uid) {
   FILE *f = fopen(s, "r");
   // Pass 1: find max userId (determines No_of_users)
   int No_of_users = 0;
   while(fgets(tmp, sizeof(tmp), f) != NULL) {
       int t = atoi(tmp);  // atoi stops at the comma
       if(t > No_of_users) No_of_users = t;
   }
   // Allocate matrix
   *matrix_out = calloc(No_of_users * No_of_movies, sizeof(double));
   // Pass 2: fill the matrix
   rewind(f);  // go back to start — no need to reopen
   while(fgets(tmp, sizeof(tmp), f) != NULL) {
       // ... fill matrix ...
   }
   fclose(f);
   return No_of_users;
}
```
**Files modified:** `utility_matrix.c`, `utility_matrix.h`, `recommender.c`, `benchmark.c`

### 4.4 Optimization 4: Memory-Buffered I/O
- **Problem**: Reading files line-by-line with `fgets()` introduces significant disk I/O overhead and buffering bottlenecks.
- **Solution**: Load the entire dataset into a single massive memory buffer using `fread()`, bypassing line-by-line disk operations completely. The buffer is then parsed linearly using manual zero-copy pointer arithmetic.

```c
// AFTER: Memory Buffered Block I/O
fseek(fstream, 0, SEEK_END);
long size = ftell(fstream);
fseek(fstream, 0, SEEK_SET);
char *buffer = (char *)malloc(size + 1);
fread(buffer, 1, size, fstream);
```

### 4.5 Optimization 5: OpenMP Parallelization
- **Problem**: Operations like matrix normalization and centered cosine similarity process massive independent row datasets iteratively on a single core.
- **Solution**: Distribute matrix loops across threads using OpenMP (`#pragma omp parallel for`).

### 4.6 Bug Fixes
During analysis, undefined behavior issues were discovered where pointers to stack memory were being passed to `free()`. These were fixed in `findusers()`, `get_utility_matrix()`, and `new_user_movies()`.

## 5. Results
### 5.1 Staged Benchmark Results

| Stage | Description | Avg Time (s) | Speedup | Reduction |
|---|---|---|---|---|
| 0 | Original (baseline) | 0.0776 | 1.00× | 0.0% |
| 1 | + Optimized calc_similarity | 0.0783 | 0.99× | ~0% |
| 2 | + Memory-based new_user_movies | 0.0640 | 1.21× | 17.5% |
| 3 | + Memory-Buffered I/O Data Loading | 0.0377 | 2.06× | 51.5% |
| 4 | + OpenMP Parallelization | 0.0250 | 3.10× | 67.8% |

### 5.2 Final Optimized Profile (Using Parallelism and Memory I/O)

| Function | Time (s) | % of Total |
|---|---|---|
| `get_utility_matrix()` (buffered) | 0.014941 | 40.08% |
| `normalize_matrix()` (parallel) | 0.013619 | 36.53% |
| `calc_similarity()` (parallel) | 0.002760 | 7.40% |
| `get_movie_names()` (buffered) | 0.002585 | 6.93% |
| `get_movie_genres()` (buffered) | 0.002182 | 5.85% |
| `kmeans()` | 0.000776 | 2.08% |
| `new_user_movies()` | 0.000010 | 0.03% |
| Others | 0.000330 | 0.88% |
| **Total** | **0.037281** | **100%** |

### 5.3 Summary
- **Total pipeline time:** Reduced from 0.0776 s to 0.0250 s (67.8% faster).
- **Speedup factor:** 3.10×

## 6. How to Run

> [!TIP]
> The `-fopenmp` compiler flag is strictly required to activate Stage 4 parallelization.

```bash
# 1. Per-function profiling (current optimized code)
gcc -fopenmp -o benchmark.exe benchmark.c utility_matrix.c matrix_normalization.c \
   pearsons.c kmeans.c predictions.c sorting.c -lm -O2
./benchmark.exe 1 100

# 2. Staged comparison (original vs each optimization level)
gcc -fopenmp -o benchmark_stages.exe benchmark_stages.c matrix_normalization.c \
   pearsons.c kmeans.c predictions.c sorting.c -lm -O2
./benchmark_stages.exe 1 100
```

---

# Original Readme:

# Movie-Recommendation-System
Implementation of movie recommendation system using Collaborative Filtering Technique using C language.

# Algorithm:

1. Creation of utility matrix of ratings between users and movies.
2. Finding similarity of new user (or concerned user) with other users using Centered Cosine Similarity (Pearson's Correlation).
3. Clustering of users based on similarity using k means clustering.
4. Prediction of movies using Collaborative Filtering technique (Low Rank Matrix Factorization) based on clusters obtained in step 3.
5. Top 10 movies printed on command line.

Instructions to run this system:

1. Open recommender.c and correct all the paths. (on line 20,55,56,57 and 63).
2. Open ui.c and correct all the paths. (on line 15, 41, 64, 67, 68).
3. Open command line and locate the directory where source code is stored and type: `gcc -fopenmp ui.c kmeans.c matrix_normalization.c pearsons.c predictions.c recommender.c sorting.c utility_matrix.c`
4. Type `a` on command line to execute the program.

# Output:
https://imgur.com/a/dk3IY

RMS Error: 0.865843

# Authors and Maintainers

1. Shubham Bhatnagar (https://github.com/shubham-bhatnagar)
2. Udhav Sharma (https://github.com/UdhavSharma)

If you encounter any problems, please contact directly or post it in issues.

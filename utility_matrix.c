//creates utility matrix
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

void get_movie_names(char *movienames, char *s){
	char *line, *record;
	char tmp[1024];
	int i=0,j=0;
	FILE *fstream = fopen(s,"r");
	if (!fstream) {
		printf("Error: Could not open file %s\n", s);
		exit(1);
	}
	while((line=fgets(tmp,sizeof(tmp),fstream))!=NULL){ //traverse till end of file while storing each line
		record = strtok(line,","); //break line into multiple strings separated by comma
		while(record!=NULL){
			if(j==1){ //second string(i.e. moviename in the csv file)
				strcpy(&movienames[i*1024],record);
			}
			j++;
			record = strtok(NULL,","); //iterate
		}
		i++;j=0;
	}
	fclose(fstream);
	/* OPTIMIZATION 5.5: Bug Fix — removed free(line) and free(record)
	 * ORIGINAL CODE:
	 * free(line);
	 * free(record);
	 * These were freeing pointers into the stack buffer tmp[1024],
	 * which is undefined behaviour that could cause memory corruption. */
}

void get_movie_genres(char *moviegenres, char *s){
	char *line, *record;
	char tmp[1024];
	int i=0,j=0;
	FILE *fstream = fopen(s,"r");
	if (!fstream) {
		printf("Error: Could not open file %s\n", s);
		exit(1);
	}
	while((line=fgets(tmp,sizeof(tmp),fstream))!=NULL){
		record = strtok(line,",");
		while(record!=NULL){
			if(j==1){
				strcpy(&moviegenres[i*1024],record);
			}
			j++;
			record = strtok(NULL,",");
		}
		i++;j=0;
	}
	fclose(fstream);
	/* OPTIMIZATION 5.5: Bug Fix — removed free(line) and free(record)
	 * Same issue as get_movie_names above. */
}

/* ══════════════════════════════════════════════════════════════════════════
 * OPTIMIZATION 5.3: Merge findusers() into get_utility_matrix()
 *
 * BEFORE: Two separate functions read the same ratings_learn.csv file:
 *   - findusers() scanned for the maximum user ID
 *   - get_utility_matrix() parsed all ratings into the matrix
 * This meant the file was opened, read line-by-line, and closed TWICE.
 *
 * AFTER: Single function using a two-pass approach with rewind():
 *   Pass 1: Find max userId (No_of_users) using fast atoi
 *   Pass 2: Fill the matrix with ratings
 * The function now allocates the matrix internally and returns No_of_users.
 *
 * OPTIMIZATION 5.4: Fast CSV Parsing
 * Within both passes, strtok+column-counter is replaced with manual
 * comma-scanning using pointer arithmetic. atoi/atof naturally stop
 * at non-numeric characters (commas), eliminating strtok overhead.
 * ══════════════════════════════════════════════════════════════════════════ */

/* ORIGINAL findusers() — now eliminated (merged into get_utility_matrix):
 *
 * int findusers(){
 *     char *line, *record;
 *     char tmp[1024];
 *     FILE *fstream = fopen("Dataset/ratings_learn.csv","r");
 *     int j=0;
 *     int max = 0;
 *     while((line=fgets(tmp,sizeof(tmp),fstream))!=NULL){
 *         record = strtok(line,",");
 *         while(record!=NULL){
 *             if(j==0){
 *                 int t = atoi(record);
 *                 if(t > max) max = t;
 *             }
 *             j++;
 *             record = strtok(NULL,",");
 *         }
 *         j=0;
 *     }
 *     fclose(fstream);
 *     free(line);    // Bug: freeing stack pointer (Optimization 5.5)
 *     free(record);  // Bug: freeing stack pointer (Optimization 5.5)
 *     return max;
 * }
 */

/* ORIGINAL get_utility_matrix():
 *
 * void get_utility_matrix(double *utility_matrix, char *s, int No_of_movies, int No_of_users, int uid){
 *     char *line, *record;
 *     char tmp[1024];
 *     int i=0, j=0, k=0;
 *     FILE *fstream = fopen(s,"r");
 *     if (!fstream) {
 *         printf("Error: Could not open file %s\n", s);
 *         exit(1);
 *     }
 *     while((line=fgets(tmp,sizeof(tmp),fstream))!=NULL){
 *         record = strtok(line,",");
 *         while(record!=NULL){
 *             if(k==0){
 *                 i = atoi(record)-1;
 *             }else if(k==1){
 *                 j = atoi(record)-1;
 *             }else {
 *                 utility_matrix[i*No_of_movies + j] = atof(record);
 *             }
 *             record = strtok(NULL,",");
 *             k++;
 *         }
 *         k=0;
 *     }
 *     fclose(fstream);
 *     free(line);    // Bug: freeing stack pointer (Optimization 5.5)
 *     free(record);  // Bug: freeing stack pointer (Optimization 5.5)
 * }
 */

// OPTIMIZED: merged findusers + get_utility_matrix + fast CSV parsing
int get_utility_matrix(double **utility_matrix_out, char *s, int No_of_movies, int uid){
	char tmp[1024];
	FILE *fstream = fopen(s,"r");
	if (!fstream) {
		printf("Error: Could not open file %s\n", s);
		exit(1);
	}

	// Optimization 5.3: First pass — find max userId to determine No_of_users
	// Optimization 5.4: atoi(tmp) parses the integer at the start of the line
	// and stops at the first non-digit character (the comma), so we get
	// the userId without needing strtok to split the line
	int No_of_users = 0;
	while(fgets(tmp, sizeof(tmp), fstream) != NULL){
		int t = atoi(tmp);
		if(t > No_of_users) No_of_users = t;
	}

	// Optimization 5.3: Allocate the utility matrix internally
	// (No_of_users rows x No_of_movies columns)
	*utility_matrix_out = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
	if(!*utility_matrix_out){
		printf("Error: Memory allocation failed for utility matrix\n");
		exit(1);
	}

	// Optimization 5.3: Second pass — fill the matrix with ratings
	// rewind() repositions to the start without reopening the file
	rewind(fstream);
	while(fgets(tmp, sizeof(tmp), fstream) != NULL){
		// Optimization 5.4: Manual comma-scan instead of strtok
		// Parse userId (first field) — atoi stops at the comma
		char *p = tmp;
		int i = atoi(p) - 1;

		// Skip to movieId (second field) — advance past first comma
		while(*p != ',' && *p != '\0') p++;
		if(*p == ',') p++;
		int j = atoi(p) - 1;

		// Skip to rating (third field) — advance past second comma
		while(*p != ',' && *p != '\0') p++;
		if(*p == ',') p++;
		(*utility_matrix_out)[i * No_of_movies + j] = atof(p);
	}

	fclose(fstream);
	return No_of_users; // Optimization 5.3: return No_of_users instead of separate findusers()
}

/* ══════════════════════════════════════════════════════════════════════════
 * OPTIMIZATION 5.2: Replace File I/O with Memory Copy in new_user_movies()
 *
 * BEFORE: Opened and parsed the entire ratings_learn.csv file to extract
 * one user's ratings. But this data was already loaded into the utility
 * matrix by get_utility_matrix() moments earlier — a completely
 * redundant file I/O operation (~14ms wasted).
 *
 * AFTER: Copy directly from the utility matrix (already in RAM).
 * The memory copy (9,125 doubles) takes microseconds vs ~14ms for file I/O.
 * ══════════════════════════════════════════════════════════════════════════ */

/* ORIGINAL new_user_movies():
 *
 * void new_user_movies(double *newuser, char *s, int uid){
 *     char *line, *record;
 *     char tmp[1024];
 *     int i,j,k=0;
 *     FILE *fstream = fopen(s,"r");
 *     if (!fstream) {
 *         printf("Error: Could not open file %s\n", s);
 *         exit(1);
 *     }
 *     while((line=fgets(tmp,sizeof(tmp),fstream))!=NULL){
 *         record = strtok(line,",");
 *         while(record!=NULL){
 *             if(k==0){
 *                 i = atoi(record) - 1;
 *             }
 *             if(k==1){
 *                 j = atoi(record) - 1;
 *             }
 *             if(k==2){
 *                 if(i+1==uid){
 *                     newuser[j] = atof(record);
 *                 }
 *             }
 *             k++;
 *             record = strtok(NULL,",");
 *         }
 *         k=0;
 *     }
 *     fclose(fstream);
 *     free(line);    // Bug: freeing stack pointer (Optimization 5.5)
 *     free(record);  // Bug: freeing stack pointer (Optimization 5.5)
 * }
 */

// OPTIMIZED: copy from utility matrix instead of re-reading the CSV file
void new_user_movies(double *newuser, double *utility_matrix, int uid, int No_of_movies){
    // utility_matrix[(uid-1) * No_of_movies + j] contains exactly the same
    // value that was parsed from the CSV — same file, same field, same conversion
    for(int j = 0; j < No_of_movies; j++){
        newuser[j] = utility_matrix[(uid-1) * No_of_movies + j];
    }
}

//creates utility matrix
#include<stdio.h>
#include<stdlib.h>
#include<string.h>


void get_movie_names(char *movienames, char *s){
	FILE *fstream = fopen(s,"rb");
	if (!fstream) {
		printf("Error: Could not open file %s\n", s);
		exit(1);
	}
	fseek(fstream, 0, SEEK_END);
	long size = ftell(fstream);
	fseek(fstream, 0, SEEK_SET);
	char *buf = (char *)malloc(size + 1);
	fread(buf, 1, size, fstream);
	fclose(fstream);
	buf[size] = '\0';

	char *p = buf;
	int i = 0;
	while (*p) {
		while (*p != ',' && *p != '\0') p++;
		if (*p == ',') p++;
		
		char *start = p;
		while (*p != ',' && *p != '\n' && *p != '\0') p++;
		int len = p - start;
		if (len > 1023) len = 1023;
		strncpy(&movienames[i * 1024], start, len);
		movienames[i * 1024 + len] = '\0';

		while (*p != '\n' && *p != '\0') p++;
		if (*p == '\n') p++;
		i++;
	}
	free(buf);
}

void get_movie_genres(char *moviegenres, char *s){
	FILE *fstream = fopen(s,"rb");
	if (!fstream) {
		printf("Error: Could not open file %s\n", s);
		exit(1);
	}
	fseek(fstream, 0, SEEK_END);
	long size = ftell(fstream);
	fseek(fstream, 0, SEEK_SET);
	char *buf = (char *)malloc(size + 1);
	fread(buf, 1, size, fstream);
	fclose(fstream);
	buf[size] = '\0';

	char *p = buf;
	int i = 0;
	while (*p) {
		while (*p != ',' && *p != '\0') p++;
		if (*p == ',') p++;
		
		char *start = p;
		while (*p != ',' && *p != '\n' && *p != '\0') p++;
		int len = p - start;
		if (len > 1023) len = 1023;
		strncpy(&moviegenres[i * 1024], start, len);
		moviegenres[i * 1024 + len] = '\0';

		while (*p != '\n' && *p != '\0') p++;
		if (*p == '\n') p++;
		i++;
	}
	free(buf);
}

/* ══════════════════════════════════════════════════════════════════════════
 * OPTIMIZATION 3: Merge findusers() into get_utility_matrix()
 *
 * BEFORE: Two separate functions read the same ratings_learn.csv file:
 *   - findusers() scanned for the maximum user ID
 *   - get_utility_matrix() parsed all ratings into the matrix
 * This meant the file was opened, read line-by-line, and closed TWICE.
 *
 * AFTER: Single function using a two-pass approach.
 *   Pass 1: Find max userId (No_of_users)
 *   Pass 2: Fill the matrix with ratings
 * The function now allocates the matrix internally and returns No_of_users.
 *
 * OPTIMIZATION 4: Memory-Buffered I/O
 * Traditional fgets() file parsing introduces significant disk I/O overhead.
 * Instead, we load the entire dataset into a single massive memory buffer using
 * fread(), and parse it linearly using zero-copy pointer arithmetic.
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
 *     free(line);    // Bug: freeing stack pointer (Bug fixes)
 *     free(record);  // Bug: freeing stack pointer (Bug fixes)
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
 *     free(line);    // Bug: freeing stack pointer (Bug fixes)
 *     free(record);  // Bug: freeing stack pointer (Bug fixes)
 * }
 */

// OPTIMIZED: merged findusers + get_utility_matrix + memory buffered I/O
int get_utility_matrix(double **utility_matrix_out, char *s, int No_of_movies, int uid){
	FILE *fstream = fopen(s,"rb");
	if (!fstream) {
		printf("Error: Could not open file %s\n", s);
		exit(1);
	}

	fseek(fstream, 0, SEEK_END);
	long fileSize = ftell(fstream);
	fseek(fstream, 0, SEEK_SET);

	char *buffer = (char *)malloc(fileSize + 1);
	if (!buffer) {
		printf("Error: Memory allocation failed for file buffer\n");
		exit(1);
	}
	fread(buffer, 1, fileSize, fstream);
	fclose(fstream);
	buffer[fileSize] = '\0';

	int No_of_users = 0;
	char *p = buffer;
	while (*p) {
		int t = atoi(p);
		if (t > No_of_users) No_of_users = t;
		while (*p && *p != '\n') p++;
		if (*p == '\n') p++;
	}

	*utility_matrix_out = (double *)calloc(No_of_users * No_of_movies, sizeof(double));
	if (!*utility_matrix_out) {
		printf("Error: Memory allocation failed for utility matrix\n");
		exit(1);
	}

	p = buffer;
	while (*p) {
		int i = atoi(p) - 1;

		while (*p != ',' && *p != '\0') p++;
		if (*p == ',') p++;
		int j = atoi(p) - 1;

		while (*p != ',' && *p != '\0') p++;
		if (*p == ',') p++;
		(*utility_matrix_out)[i * No_of_movies + j] = atof(p);

		while (*p != '\n' && *p != '\0') p++;
		if (*p == '\n') p++;
	}

	free(buffer);
	return No_of_users;
}

/* ══════════════════════════════════════════════════════════════════════════
 * OPTIMIZATION 2: Replace File I/O with Memory Copy in new_user_movies()
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
 *     free(line);    // Bug: freeing stack pointer (Bug fixes)
 *     free(record);  // Bug: freeing stack pointer (Bug fixes)
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

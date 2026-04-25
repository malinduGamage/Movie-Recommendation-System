//header for utility_matrix.h
#ifndef UTILITY_MATRIX_H
#define UTILITY_MATRIX_H

void get_movie_names(
					char *movienames, // defined as char *movienames = (char *)malloc(sizeof(char)* No_of_movies * 1024)); where movies will be stored

					char *s // source of dataset file
					);

void get_movie_genres(
					char *moviegenres, // defined as char *moviegenres = (char *)malloc(sizeof(char) * No_of_movies * 1024); where movie genres will be stored

					char *s // source of dataset file
					);

/* Optimization 5.3: Merged findusers() into get_utility_matrix()
 * ORIGINAL signature: void get_utility_matrix(double *utility_matrix, char *s, int No_of_movies, int No_of_users, int uid);
 * NEW signature: allocates the matrix internally via double**, returns No_of_users */
int get_utility_matrix(
						double **utility_matrix_out, // output: pointer to allocated matrix (No_of_users x No_of_movies)

						char *s, // source of dataset file

						int No_of_movies,

						int uid
						); // returns No_of_users

/* Optimization 5.2: Replaced file I/O with memory copy
 * ORIGINAL signature: void new_user_movies(double *newuser, char *s, int uid);
 * NEW signature: reads from utility_matrix instead of CSV file path */
void new_user_movies(
                     double *newuser, //defined as double *newuser = (double *)malloc(sizeof(double) * No_of_movies); where ratings of new user will be stored

                     double *utility_matrix, //utility matrix already loaded in memory

                     int uid,

                     int No_of_movies
                     );

#endif

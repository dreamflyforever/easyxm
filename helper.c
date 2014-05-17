#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#define MAXBUFFER 1024

/* Concatenate dirname and filename, and return an open
 * file pointer to file filename opened for writing in the
 * directory dirname (located in the current working directory)
 */
FILE *open_file_in_dir(char *filename, char *dirname) {
    char buffer[MAXBUFFER];
    strncpy(buffer, "./", MAXBUFFER);
    strncat(buffer, dirname, MAXBUFFER - strlen(buffer));

    // create the directory dirname. Fail silently if directory exists
    if(mkdir(buffer, 0700) == -1) {
        if(errno != EEXIST) {
            perror("mkdir");
            exit(1);
        }
    }
    strncat(buffer, "/", MAXBUFFER - strlen(buffer));
    strncat(buffer, filename, MAXBUFFER - strlen(buffer));

    return fopen(buffer, "w");
}


/* A simple test for open_file_in_dir */
/*
int main(int argc, char *argv[]) {
    char *dir = "filestore";

    FILE *fp;
 
    if((fp = open_file_in_dir(argv[1], dir)) == NULL) {
        perror("open file");
        exit(1);
    }
    fprintf(fp, "Trial\n");
    fclose(fp);

}
*/

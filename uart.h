#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#define FALSE  -1
#define TRUE   0

extern int speed_arr[];
extern int name_arr[];
int set_speed(int fd, int speed);
int set_parity(int fd, int databits, int stopbits, int parity);

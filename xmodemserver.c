#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>
#include "xmodemserver.h"
#include "crc16.h"
#include "uart.h"

#ifndef PORT
#define PORT 50179
#endif

#define MAXBUFFER 1024

#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

/* Variable to hold the state*/
int state;
/* A helper function for processing a client. This functions covers all states
   except the finished state. This function takes in the argument toplist (the
   top of the linked list, and cl, the current client node/struct. */
int processclient(struct client *cl)
{
	/* Variable used for writing ACK, NAK, and other exciting control codes */
	char acker;

	/* If client is in the initial state */
	if ( cl->state == initial ) {

		/* Variable to determine if network newline has been found */
		int readin = 0;

		/* Read in one byte at a time so that the read() call doesn't block, top when
		   network newline has been found and add a null terminator at the end of the
		   filename. */
		printf("%s:%d\n", __func__, __LINE__);

		if (read(cl->fd,  &cl->filename, 20) <= 0) {
			fprintf(stderr, "%s:%d\n", __func__, __LINE__);
			return 0;
		}
		printf("%s:%d--->filename: %s\n", __func__, __LINE__, cl->filename);
		readin = 1;

		/* If filename is completely read in, use helper.c to generate the file
		   pointer for it, reset the buffer index, and transition to pre_block state */
		if ( readin == 1) {
			cl->fp = open_file_in_dir(cl->filename, "storage");
			cl->inbuf = 0;
			cl->state = pre_block;
		}

		/* Write "C" to the client so it can move on with its transfer, if write
		   fails, drop client by transitioning to finished block */
		if ( write(cl->fd, "C", 1) < 1) {
			perror("Writing C failed");
			cl->state = finished;
		}	
	}

	/* If client is in the pre_block state */
	if ( cl->state == pre_block ) {

		/* Temporary buffer to hold in any incoming control codes. 2048 is 
		   overkill, yes, however it can't hurt to be safe. */
		char prebuf[2048];

		/* Read in one byte at a time from the client */
		if ( read(cl->fd, &prebuf[0], 1) == 1) {
			printf("%s:%d\n", __func__, __LINE__);

			/* If EOT is received, drop client by transitioning to finish state */
			if ( prebuf[cl->inbuf] == EOT ) {

				/* Close file pointer */
				fclose(cl->fp);

				/* Set acker to ACK control code and write it to the client */
				acker = ACK;
				printf("%s:%d\n", __func__, __LINE__);
				if (write(cl->fd, &acker, sizeof(char)) < 1) {
					perror("writing ACK for EOT in pre_block failed");
				}

				cl->state = finished;

			}

			/* If SOH block is received, set the expected blocksize on client to 128, 
			   and transition to get_block */
			if ( prebuf[0] == SOH ) {
				cl->blocksize = 128;
				cl->state = get_block;

			}

			/* If STX block is received, set the expected blocksize on client to 1024, 
			   and transition to get_block */
			if ( prebuf[0] == STX ) {
				cl->blocksize = 1024;
				cl->state = get_block;

			}
		}

		else{
			perror("pre_block read fail");

		} 

	}

	/* If client is in the get_block state */
	if ( cl->state == get_block ) {

		/* If server is expecting a SOH block */
		if (cl->blocksize == 128) {

			/* Read in one byte for current block number and store it in the client's
			   current_block attribute */
			if ( read(cl->fd, &cl->current_block, 1) < 1) {
				perror("reading in current_block failed in SOH, get_block");
			}

			/* Read in one byte for inverse block number and store it in the client's
			   inverse_block attribute */
			if ( read(cl->fd, &cl->inverse_block, 1) < 1) {
				perror("reading in inverse_block failed in SOH, get_block");
			}

			/* Read in the actual payload to the client's buf attribute, one byte at a
			   time */
			while ( cl->inbuf < 128 ) {
				if ( read(cl->fd, &cl->buf[cl->inbuf], 1) == 1) {
					cl->inbuf++;
				}
				else {
					perror("get_block_soh_read_failure\n");
				}

				/* Check if blocksize bytes have been read in, if so, caclculate CRC 
				   and store it in the client's CRC attributes. Transition to check_block. */
				if (cl->inbuf == cl->blocksize) {
					read(cl->fd, &cl->crca, 1);
					read(cl->fd, &cl->crcb, 1);
					cl->state = check_block;	
				}
			}

		}

		/* If server is expecting a STX block */
		if (cl->blocksize == 1024 ) {

			/* Read in one byte for current block number and store it in the client's
			   current_block attribute */
			if (read(cl->fd, &cl->current_block, 1) < 1) {
				perror("reading in current_block failed in STX, get_block");
			}

			/* Read in one byte for inverse block number and store it in the client's
			   inverse_block attribute */
			if (read(cl->fd, &cl->inverse_block, 1) < 1) {
				perror("reading in inverse_block failes in STX, get_block");
			}

			/* Read in the actual payload to the client's buf attribute, one byte at a
			   time */
			while ( cl->inbuf < 1024 ) {
				if ( read(cl->fd, &cl->buf[cl->inbuf], 1) == 1) {
					cl->inbuf++;
				}
				else {
					perror("get_block_stx_read_failure");
				}
			}

			/* Check if blocksize bytes have been read in, if so, caclculate CRC 
			   and store it in the client's CRC attributes. Transition to check_block. */
			if (cl->inbuf == cl->blocksize) {
				read(cl->fd, &cl->crca, 1);
				read(cl->fd, &cl->crcb, 1);
				cl->state = check_block;
			}

		}
	}

	if ( cl->state == check_block ) {

		/* Reset buffer index after previous block */
		cl->inbuf = 0;

		/* If inverse block and current block don't corresond, write ACK and drop client */
		if ( (255 - cl->inverse_block)  != cl->current_block ) {

			/* Set acker to the ACK control code and write it */
			acker = ACK;
			if (write(cl->fd, &acker, sizeof(char)) < 1) {
				perror("ACK failed to write in check_block for invalid inverse");
			}
			cl->state = finished;
		}

		/* If previous block and current block numbers are the same, ACK the second one */
		if (cl->previous_block == cl->current_block) {
			acker = ACK;
			if (write(cl->fd, &acker, sizeof(char)) < 1) {
				perror("ACK failed to write in check_block for duplicate blocks");
			}
		}

		/* If blocks are out of order, write ACK and drop client */
		if ( cl->current_block != cl->previous_block + 1) {
			if ( (cl->previous_block != 255) && (cl->current_block != 0) ) {

				/* Set acker to the ACK control code and write it */
				acker = ACK;
				if (write(cl->fd, &acker, sizeof(char)) < 1) {
					perror("ACK failed to write in check_block for invalid block order");
				}
				cl->state = finished;
			}
		}

		/* If CRC's don't match up, send NAK */
		if ( crc_message(XMODEM_KEY, cl->buf, cl->blocksize) != ((cl->crca << 8) + cl->crcb) ) {

			/* Set acker to NAK and write it */
			acker = NAK;
			if (write(cl->fd, &acker, sizeof(char)) < 1) {
				perror("NAK failed to write in check_block for invalid CRC");
			}
		}

		/* If none of the above error checks are triggered */
		else {

			/* Set previous block to current, and account for wrapping at block number 
			   255 */
			cl->previous_block = cl->current_block;
			if ( cl->current_block > 255 ) {
				cl->current_block = 0;
			}
			/* Write the current payload in the client's buf attribute to the file */ 
			if (fwrite(&cl->buf, sizeof(char), cl->blocksize, cl->fp) < cl->blocksize) {
				perror("Writing payload to file failed"); 
			}

			/* Set acker to ACK and write it */
			acker = ACK;
			if (write(cl->fd, &acker, sizeof(char)) < 1) {
				perror("ACK failed to write in check_block file write stage");
			}

			/* Transition back to the pre_block */
			cl->state = pre_block;
		}
	}

	if ( cl->state == finished ) {
		cl->state = initial;
	}

	return 1;
}


int main(void)
{
	struct client *cl = (struct client *)malloc(sizeof(struct client));
	cl->state = initial;
	cl->inbuf = 0;
	cl->fd =  open("/dev/ttyUSB0", O_RDWR);
	if (cl->fd < 0) {
		fprintf(stderr, "Can't open the uart\n");
		exit(1);
	}

	set_speed(cl->fd, 115200);
	if (set_parity(cl->fd, 8, 1, 'N') == FALSE) {
		fprintf(stderr, "Set Parity Error\n");
		exit(0);
	}
	printf("fd:%d\n", cl->fd);
	/* Infinite while loop -- server must be killed */
	while (1) {
		processclient(cl);
	}
	printf("-------------------\n");
	free(cl);
	close(cl->fd);
	return 1;
}

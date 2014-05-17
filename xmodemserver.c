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

#ifndef PORT
	#define PORT 50179
#endif

#define MAXBUFFER 1024

#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>


/* File descriptors */
int listenfd; 
int maxfd;

/* Socket descriptor sets */
fd_set fdlist, rset;

/* Variable to hold the state*/
int state;

/* Helper function for adding a new client to the linked list. 
   loosely based on muffinman.c */
void addclient(int fd, struct client *toplist) {
    
	/* New struct for the client being added */
	struct client *p = malloc(sizeof(struct client));
	if (!p) {
    	fprintf(stderr, "out of memory!\n");  
    	exit(1);
    }
    
	/* If the list is comprised solely of the top node, then simply
	   append the new node to the top of the list. */
    if ( !(toplist->next)) {
		p->fd = fd;
		p->state = initial;
		p->inbuf = 0;
		p->current_block = 0;
		toplist->next = p;
		p->next = NULL;
	}
	
	/* Otherwise, iterate through the list and insert the node where necessary */
    else {
		for (p = toplist; p; p = p->next ) {
            if ( p->next == NULL ) {
                p->fd = fd;
				p->state = initial;
				p->inbuf = 0;
				p->current_block = 0;
				p->next = (struct client *)(malloc(sizeof(struct client)));
				p->next->next = NULL;
            }
        }
	}
}

/* A helper function for processing a client. This functions covers all states
   except the finished state. This function takes in the argument toplist (the
   top of the linked list, and cl, the current client node/struct. */
void processclient(struct client *toplist, struct client *cl) {
	
	/* Variable used for writing ACK, NAK, and other exciting control codes */
	char acker;
	
	/* If client is in the initial state */
	if ( cl->state == initial ) {
		
		/* Variable to determine if network newline has been found */
		int readin = 0;
		
		/* Read in one byte at a time so that the read() call doesn't block, top when
 		   network newline has been found and add a null terminator at the end of the
		   filename. */
		while ( (cl->inbuf < 20) && (read(cl->fd, &cl->filename[cl->inbuf], 1) == 1) ) {
		    if ( (cl->inbuf > 0) && (cl->filename[cl->inbuf] == '\r') ) {
    			readin = 1;
				cl->filename[cl->inbuf] = '\0';
				break;
			}
			
			/* Increment the position in the buffer */
			cl->inbuf++;
			
		}
    	
		/* If network newline wasn't found, drop client by moving to finish state */
		if ( (readin == 0) && (cl->inbuf > 20) ) {
            cl->state = finished;
		}
		
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
			
			/* If EOT is received, drop client by transitioning to finish state */
			if ( prebuf[cl->inbuf] == EOT ) {
				
				/* Close file pointer */
				fclose(cl->fp);
				
				/* Set acker to ACK control code and write it to the client */
				acker = ACK;
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
}


int main(void) {
	
	/* Top of the linked list */
	struct client *top = malloc(sizeof(struct client));
	top->next = NULL;
	
	/* FD for any new clients connecting */
	int newclientfd;
	
	/* Necessary socket stuff */
	struct sockaddr_in r;
	socklen_t len;
	
	/* Create socket, and listen */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);
	
    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }

	/* Set up the FD sets */
	FD_ZERO(&fdlist);
    FD_SET(listenfd, &fdlist);
	
	/* Set the highest descriptor to the listenfd */
	maxfd = listenfd;

	/* Infinite while loop -- server must be killed */
	while (1) {
		
		/* Make a copy of the list of descriptors */
		rset = fdlist;
	
		/* Pointers to clients that we can use later one for linked list operations */
		struct client *node;
		struct client *previously;
		struct client *current;
		
		/* Iterate through the linked list and add any clients not in the descriptor set,
		   we can also adjust maxfd if any of the clients in the linked list have 
		   descriptor higher than the current maxfd. */
		node = top;
		while (node) {
			if ( !(FD_ISSET(node->fd, &rset)) ) {
				FD_SET(node->fd, &rset);
			}
			if ( node->fd > maxfd) {
				maxfd = node->fd;
			}
			node = node->next;
		}

		/* Call select to see which descriptors have data ready for us */
		if ( select(maxfd+1, &rset, NULL, NULL, NULL) > -1 ) {
			
			/* If the listenfd is in the set, that means we have a new client connecting */
			if ( FD_ISSET(listenfd, &rset) ) {
				len = sizeof(r);
				newclientfd = accept(listenfd, (struct sockaddr *)&r, &len);
				if (newclientfd < 0) {
					perror("accept failed");
				}
				
				/* Add the new client to the linked list and check if maxfd needs to be 
				   set */
				addclient(newclientfd, top);
				if ( newclientfd > maxfd ) {
					maxfd = newclientfd;
				}
			}
			
			/* Iterate through the linked list, and if the client is in the select set,
			   process it through the state machine */
			node = top;
			while (node) {
				
				/* If current client is in the select set, process it */
				if (FD_ISSET(node->fd, &rset)) {
					processclient(top, node);

					/* If that client is now finished, or needs to be dropped, handle it
					   accordingly */
					if (node->state == finished) {
						
						/* If that client had the highest descriptor, adjust maxfd */
						if ( node->fd == maxfd) {
							maxfd = maxfd - 1;
						}
						
						/* Remove client from descriptor set and from linked list */
						FD_CLR(node->fd, &fdlist);
						current = top;
						while ( current != NULL ) {
							if (current->fd == node->fd) {
								if (current == top) {
									top = current->next;
									free(current);
									break;
								}
								else {
									previously->next = current->next;
									free(current);
									break;
								}
							}
							else {
								previously = current;
								current = current->next;
							}
						}

						/* Close the descriptor for the dropped client */
						close(node->fd);
		
						/* If there is no next client, break from the loop */
						if ( !(node->next) ) {
                        	break;
                    	}
					}
				}
				
				else {
					
					/* If there is no next client, break from the loop */
					if ( !(node->next) ) {
						break;
					}
					else {
						node = node->next;
					}
				}
			}
			
		}
		else {
			perror("select failed");
		}
	}
	return 1;
}































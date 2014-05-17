#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "crc16.h"

//ASCII characters
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 21
#define CAN 24
#define SUB 26

enum sendstate {
	initial, // send filename
	handshake, // wait for C
	send_block, // read and send the block
	wait_reply, // wait for packet to be ack'd
	finish
}; // send eot


#define XMODEM_KEY 0x1021




int main(int argc, char* argv[]) {
	int soc, len, err;
	unsigned char buf[128]; // payloads
	char temp;
	struct addrinfo *info, hints;
	struct sockaddr_in peer;
	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if ( argc != 4) {
		fprintf(stderr, "Usage: %s <hostname> <port> <filename>\n", argv[0]);
		exit(1);
	}
	if ((err = getaddrinfo(argv[1], argv[2], &hints, &info))) {
		fprintf (stderr, "%s\n", gai_strerror(err));
		exit (1);
	}

	FILE *fp = fopen(argv[3], "rb");
	if (!fp) {
		perror("fopen");
		exit(2);
	}

	peer = *(struct sockaddr_in *)(info->ai_addr);

	if ((soc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror ("socket");
		exit (1);
	}

	if (connect(soc, (struct sockaddr *)&peer, sizeof(peer)) == -1) {
		perror ("connect");
		exit (1);
	}

	int state = initial;
	int current_block, readblock;

	while (1) {
		if (state == initial) {
			write(soc, argv[3], strlen(argv[3]));
			write(soc, "\r\n", 2);
			current_block = 1;
			readblock = 1;
			state = handshake;
			printf("Client sent filename and entering handshake.\n");
		}

		if (state == handshake) {
			while ((len = read(soc, &temp, 1)) > 0) {
				if (temp == 'C') {
					printf("Client found C; entering send_block\n");
					state = send_block;
					break;
				}
			}
			if (len <= 0) {
				fprintf(stderr, "Server dropped client\n");
				exit(1);
			}
		}
		if (state == send_block) {
			if (readblock) {
				readblock = 0;
				len = fread(buf, 1, 128, fp);
				if (len == 0) {
					printf("Client got 0 fread; moving to finish.\n");
					state = finish;
					continue;
				}
				while (len < 128)
					buf[len++] = SUB;
			}
			unsigned char char_block = current_block;
			temp = SOH;
			printf("Client sending the block+overhead\n");
			write(soc, &temp, 1);
			printf("Sending block %d and inverse %d\n", char_block, 255-char_block);
			write(soc, &char_block, 1);
			char_block = 255 - char_block;
			write(soc, &char_block, 1);
			write(soc, buf, 128);
			unsigned short crc = crc_message(XMODEM_KEY, buf, 128);
			printf("Client wants to send crc %x\n", crc);
			char_block = crc >> 8;
			printf("Client sends crc first byte %x\n", char_block);
			write(soc, &char_block, 1);
			char_block = crc;
			printf("Client sends crc second byte %x\n", char_block);
			write(soc, &char_block, 1);
			printf("Client moving to wait_reply\n");
			state = wait_reply;
		}

		if (state == wait_reply) {
			while ((len = read(soc, &temp, 1)) > 0) {
				if (temp == NAK) {
					printf("Client received nak; moving back to send_block\n");
					state = send_block;
					break;
				}

				if (temp == ACK) {
					printf("Client received ack; moving to send_block for next block\n");
					state = send_block;
					readblock = 1;
					current_block++;
					if (current_block > 255) 
						current_block = 0;
					break;
				}
			}
		}

		if (state == finish) {
			temp = EOT;
			printf("Client sent eot, waiting for ack\n");
			write(soc, &temp, 1);
			while ((len = read(soc, &temp, 1)) > 0) {
				if (temp == NAK) {
					temp = EOT;
					write(soc, &temp, 1);
					printf("Client received nak to its EOT! Sending new EOT\n");
					break;
				}

				if (temp == ACK) {
					printf("Done!\n");
					exit(0);
				}
			}
		}
	}

	return 0;
}

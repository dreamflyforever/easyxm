#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "uart.h"
#include "crc16.h"

//ASCII characters
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 21
#define CAN 24
#define SUB 'c'

enum sendstate {
	/*send filenameI*/
	initial,
	/*wait for C*/
	handshake,
	/*read and send the block*/
	send_block,
	/*wait for packet to be ack'd*/
	wait_reply,
	finish
};/*send eot*/

#define XMODEM_KEY 0x1021

/*argv[0] = uart, argv[1] = file_path*/
int main(int argc, char* argv[])
{
	int uart_fd, len;
	/*payloads*/
	unsigned char buf[128];
	char temp;

	if ( argc != 3) {
		fprintf(stderr, "Usage: %s <uart_device> <filename>\n", argv[0]);
		exit(1);
	}

	FILE *fp = fopen(argv[2], "rb");
	if (!fp) {
		perror("fopen");
		exit(2);
	}

	if ((uart_fd = open(argv[1], O_RDWR)) < 0) {
		perror("uart_fdket");
		exit (1);
	}

	set_speed(uart_fd, 115200);
	if (set_parity(uart_fd, 8, 1, 'N') == FALSE) {
		fprintf(stderr, "Set Parity Error\n");
		exit(0);
	}

	int state = initial;
	int current_block, readblock;

	while (1) {
		switch (state) {
		case initial:
			write(uart_fd, argv[2], strlen(argv[2]));
			current_block = 1;
			readblock = 1;
			state = handshake;
			printf("Client sent filename and entering handshake.\n");
			break;
		
		case handshake:
			while ((len = read(uart_fd, &temp, 1)) > 0) {
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
			break;

		case send_block:
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
			write(uart_fd, &temp, 1);
			printf("Sending block %d and inverse %d\n",
				char_block,
				255-char_block);
			write(uart_fd, &char_block, 1);
			char_block = 255 - char_block;
			write(uart_fd, &char_block, 1);
			write(uart_fd, buf, 128);
			unsigned short crc = crc_message(XMODEM_KEY, buf, 128);
			printf("Client wants to send crc %x\n", crc);
			char_block = crc >> 8;
			printf("Client sends crc first byte %x\n", char_block);
			write(uart_fd, &char_block, 1);
			char_block = crc;
			printf("Client sends crc second byte %x\n", char_block);
			write(uart_fd, &char_block, 1);
			printf("Client moving to wait_reply\n");
			state = wait_reply;
			break;

		case wait_reply:
			while ((len = read(uart_fd, &temp, 1)) > 0) {
				if (temp == NAK) {
					printf("Client received nak; moving back \
					to send_block\n");
					state = send_block;
					break;
				}

				if (temp == ACK) {
					printf("Client received ack; moving to \
					send_block for next block\n");
					state = send_block;
					readblock = 1;
					current_block++;
					if (current_block > 255) 
						current_block = 0;
					break;
				}
			}

			break;

		case finish:
			temp = EOT;
			printf("Client sent eot, waiting for ack\n");
			write(uart_fd, &temp, 1);
			while ((len = read(uart_fd, &temp, 1)) > 0) {
				if (temp == NAK) {
					temp = EOT;
					write(uart_fd, &temp, 1);
					printf("Client received nak to its EOT!\
					Sending new EOT\n");
					break;
				}

				if (temp == ACK) {
					printf("Done!\n");
					exit(0);
				}
			}
			break;
		default:
			printf("error\n");
			break;
		}
	}

	return 0;
}

#include "crc16.h"

/*Process one bit of the crc.
reg: register (modified by this function)
key: CRC16 key being used
next_bit: next bit of message
 */

void crc_bit(unsigned short *reg, unsigned int key, unsigned int next_bit) {
	unsigned short r = *reg;
	unsigned int top_bit = r & 0x8000;

	r = r << 1; // shift left to make room for ...
	r = r ^ next_bit; // the next data bit
	if (top_bit) 
		r = r ^ key;
	*reg = r;
}

/*Process one byte of the crc.
reg: register (modified by this function)
key: CRC16 key being used
next_byte: next byte of message
 */

void crc_byte(unsigned short *reg, unsigned int key, unsigned int next_byte) {
	unsigned int top_bit;
	int i;

	for (i = 0; i < 8; i++) {
		top_bit = (next_byte & 0x80) >> 7;
		crc_bit(reg, key, top_bit);
		next_byte = next_byte << 1;
	}
}

/*Process an entire message using CRC16.
key: CRC16 key being used
message: message to be crc'd.
 */
unsigned short crc_message(unsigned int key,  unsigned char *message, int num_bytes) {
	unsigned short reg = 0;
	int i;

	for (i = 0; i < num_bytes; i++ )
		crc_byte(&reg, key, message[i]);

	// push all databytes through the register
	for (i = 0; i < 2; i++) {
		crc_byte(&reg, key, 0);
	}
	return reg;
}

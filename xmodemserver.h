/* This header file should be included in your xmodemserver.c file.
 * You are welcome to add to it.
 */
FILE *open_file_in_dir(char *filename, char *dirname);
void crc_bit(unsigned short *reg, unsigned int key, unsigned int next_bit);
void crc_byte(unsigned short *reg, unsigned int key, unsigned int next_byte);
unsigned short crc_message(unsigned int key,  unsigned char *message, int num_bytes);

/* Enumeration constants are similar to constants created with #define
 * It is a nice way of being able to give a name to a state in this case.
 * For example, you can use the enum below as follows:
 *     //first declare a variable:
 *     enum recstate state;
 *     //then give it a value (note that the value is not a string)
 *     state = check_block;
 */
enum recstate {
	initial,
	pre_block,
	get_block,
	check_block,
	finished
};

/* The client struct contains the state that the server needs for each client.
 */
struct client {
	int fd;               // socket descriptor for this client
	unsigned char buf[2048];       // buffer to hold data being read from client
	int inbuf;            // index into buf
	char filename[20];    // name of the file being transferred
	FILE *fp;             // file pointer for where the file is written to
	enum recstate state;  // current state of data transfer for this client
	int blocksize;        // the size of the current block
	int current_block;    // the block number of the block currently being processed
	int previous_block;   // the block number of the previous block
	int inverse_block;    // the inverse block number 
	unsigned short crca;   // the first crc of the current block as provided by client	
	unsigned short crcb;   // the second crc of the current block as provided by client
};

#define XMODEM_KEY 0x1021


//ASCII characters
#define SOH 1
#define STX 2
#define EOT 4
#define ACK 6
#define NAK 21
#define CAN 24

/*Process an entire message using CRC16.
  key: CRC16 key being used
  message: message to be crc'd.
*/

unsigned short crc_message(unsigned int key,  unsigned char *message, int num_bytes);

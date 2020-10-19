### Xmodemserver - A file server written in C using the XMODEM protocol to transfer files. 

author `Eric Williams  
Assignment 4  
CSC209, Spring 2014`  

### OVERVIEW:

Clients send files and the server accepts them, and stores them on disk.   

### Test case
Run `make` will product two program, run the server first, `./xmodemserver`,  
then run the client, usage: ./client <uart_device> <filename>, and you will  
see the filename in storage directory.

The server is meant to be run until killed. Simply running the binary will
start the server until it is killed, or crashes (hopefully not!).  

### Modified by Shanjin Yang

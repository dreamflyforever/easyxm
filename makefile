PORT=50179
CFLAGS=-DPORT=\$(PORT) -g -Wall


all: xmodemserver client1

xmodemserver: xmodemserver.o crc16.o helper.o 
	gcc ${CFLAGS} -o $@ xmodemserver.o helper.o crc16.o

client1: client1.o crc16.o 
	gcc ${CFLAGS} -o $@ client1.o crc16.o

xmodemserver.o: xmodemserver.c xmodemserver.h 
	gcc ${CFLAGS} -c $<

crc16.o: crc16.c crc16.h 
	gcc ${CFLAGS} -c $<

%.o: %.c 
	gcc ${CFLAGS} -c $<

clean: rm *.o xmodemserver client1 crc16 helper

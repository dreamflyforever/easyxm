PORT=50179
CFLAGS=-DPORT=\$(PORT) -g -Wall


all: xmodemserver client1

xmodemserver: xmodemserver.o crc16.o helper.o uart.o
	gcc ${CFLAGS} -o $@ xmodemserver.o helper.o crc16.o uart.o

client1: client1.o crc16.o uart.c uart.o 
	gcc ${CFLAGS} -o $@ client1.o crc16.o uart.o

xmodemserver.o: xmodemserver.c xmodemserver.h uart.c uart.h
	gcc ${CFLAGS} -c $<

crc16.o: crc16.c crc16.h 
	gcc ${CFLAGS} -c $<

%.o: %.c 
	gcc ${CFLAGS} -c $<

clean:
	-rm *.o xmodemserver client1 crc16 helper

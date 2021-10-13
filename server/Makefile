#file: makefile for building executable file for native platform or with cross compilation specified at commandline(for eg. using aarch64-none-linux-gnu- toolchain)
#refernce : http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/sequencer_generic/Makefile
#author : Mehul Patel
#usage for crosscompilation: make CROSS_COMPILE=platform specificaion

ifeq ($(CC),)
	CC = ${CROSS_COMPILE}gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $^ -o $@ $(INCLUDES) $(LDFLAGS) $(CFLAGS)

clean:
	-rm -f *.o
	-rm -f aesdsocket
		

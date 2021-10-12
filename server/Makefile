#file: makefile for building executable file for native platform or with cross compilation specified at commandline(for eg. using aarch64-none-linux-gnu- toolchain)
#refernce : http://ecee.colorado.edu/~ecen5623/ecen/ex/Linux/sequencer_generic/Makefile
#author : Mehul Patel
#usage for crosscompilation: make CROSS_COMPILE=platform specificaion	
INCLUDE_DIRS = 
LIB_DIRS = 
CC=$(CROSS_COMPILE)gcc

ifeq ($(CC),)
	CC = $(CROSS_COMPILE)gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

HFILES= 
CFILES= aesdsocket.c

SRCS= ${HFILES} ${CFILES}
OBJS= ${CFILES:.c=.o}

all: aesdsocket  

default: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) $(INCLUDES) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	
clean:
	-rm -f *.o
	-rm -f aesdsocket

depend:

.c.o:
	$(CC) $(CFLAGS) -c $<
	

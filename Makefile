# simple makefile for lshwd
#
#CC = ncc -ncgcc -ncld -ncfabs
CFLAGS =-I. -Wall -O2 #-DDEBUG #-D_GNU_SOURCE

LIBS = -lpci -lusb 
OBJFILES = lshwd.c usb_names.c psaux.c pcmcia.c
#OBJFILES = lshwd.o usb_names.o psaux.o pcmcia.o

all: lshwd

lshwd: $(OBJFILES) $(LIBS)

clean:
	rm -f *.o *~ lshwd

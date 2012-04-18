SHELL = /bin/sh

# CC = arm-angstrom-linux-gnueabi-gcc -O2

CFLAGS = -Wall -DLINUX -g
INCLUDES = -I/usr/include -I/usr/include/SDL
LIBFLAGS = -lutil -lSDL-1.2 -lSDL_mixer-1.2 -lSDL_ttf-2.0 -lSDL_image

objects = xvt.o command.o screen.o ttyinit.o omshell.o SDL_terminal.o
     
all: $(objects)
	$(CC) $(CFLAGS) $(LIBFLAGS) -lpthread -o fatfingershell $(objects)
     
$(objects): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean: 
	rm -f *.o fatfingershell $(objects) core core.*


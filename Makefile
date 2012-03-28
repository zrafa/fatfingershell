SHELL = /bin/sh

# CC = arm-angstrom-linux-gnueabi-gcc -O2 -g 
# CC = arm-angstrom-linux-gnueabi-gcc -O2
# CDEBUG = -g
# CFLAGS = $(CDEBUG) -Wall -DLINUX
CFLAGS = -Wall -DLINUX -g
# INCLUDES = -I/usr/include -I/usr/include/SDL -I. -Iinclude/ -Iinclude/GL -Iinclude/tcl8.4
INCLUDES = -I/usr/include -I/usr/include/SDL
# INCLUDES = 
LIBFLAGS = -lutil -lSDL-1.2 -lSDL_mixer-1.2 -lSDL_ttf-2.0 -lX11

all:    fatfingershell

fatfingershell: command.h SDL_terminal.h token.h omshell.h screen.h SDL_terminal_private.h ttyinit.h xvt.h xvt.c command.c screen.c ttyinit.c SDL_terminal.c omshell.c
	$(CC) $(CFLAGS) $(INCLUDES) -c xvt.c
	$(CC) $(CFLAGS) $(INCLUDES) -c command.c
	$(CC) $(CFLAGS) $(INCLUDES) -c screen.c
	$(CC) $(CFLAGS) $(INCLUDES) -c ttyinit.c
	$(CC) $(CFLAGS) $(INCLUDES) -c SDL_terminal.c
	$(CC) $(CFLAGS) $(INCLUDES) -pthread -c omshell.c
	$(CC) $(CFLAGS) $(LIBFLAGS) -lpthread -o fatfingershell xvt.o command.o screen.o ttyinit.o omshell.o SDL_terminal.o
 
clean: 
	rm -f *.o fatfingershell SDL_terminal.o core core.* xvt.o command.o screen.o ttyinit.o omshell.o


SHELL = /bin/sh

# CC = arm-angstrom-linux-gnueabi-gcc -O2

CFLAGS = -Wall -DLINUX
INCLUDES = -I/usr/include/SDL
LIBFLAGS = -lutil -lSDL -lSDL_mixer -lSDL_ttf -lSDL_image

BINDIR = /usr/local/bin
DATADIR = /usr/local/share/fatfingershell

objects = xvt.o command.o screen.o ttyinit.o omshell.o SDL_terminal.o
     
all: $(objects)
	$(CC) $(CFLAGS) $(LIBFLAGS) -lpthread -o fatfingershell $(objects)
     
$(objects): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean: 
	rm -f *.o fatfingershell $(objects) core core.*

# install:
# 	@echo "Install .."
# 	if [ ! -d $(BINDIR) ] ; then mkdir $(BINDIR) ; fi
# 	install -m755 fatfingershell $(BINDIR)
# 	if [ ! -d $(DATADIR) ] ; then mkdir $(DATADIR) ; fi
# 	install -m644 ../share/* $(DATADIR)
# 	
# uninstall:
# 	@echo "Uninstall .."
# 	rm $(BINDIR)/fatfingershell
# 	rm $(DATADIR)/*
# 	rmdir $(DATADIR)
# 	

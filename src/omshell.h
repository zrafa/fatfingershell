#ifndef OMSHELL_H
#define OMSHELL_H

void omshell_quit(int error);

void omshell_terminalcursor(int x,int y,int width,int height);

void omshell_terminaldrawimagestring(int x,int y,unsigned char *str,int len, int b, int inverso, int extended);

void omshell_terminal_clear_area(int x,int y,int width,int height);

void omshell_terminal_copy_area(int src_x, int src_y, 
			unsigned int width, unsigned height, int dest_x, int dest_y);

int omshell_check_ts(void);

char *omshell_keyreleased();

void omshell_keypressed(int k);

void omshell_main(int argc, char * argv[]);

#endif /* OMSHELL_H */

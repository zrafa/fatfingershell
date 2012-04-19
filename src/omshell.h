#ifndef OMSHELL_H
#define OMSHELL_H

void omshell_quit(int error);

void TerminalBlit(int x1, int y1, int x2, int y2);

void TerminalCursor(int x,int y,int width,int height);

void TerminalDrawImageString(int x,int y,unsigned char *str,int len, int b, int inverso, int extended);

void Terminal_Clear_Area(int x,int y,int width,int height);

void Terminal_Copy_Area(int src_x, int src_y, 
			unsigned int width, unsigned height, int dest_x, int dest_y);

int check_ts(void);

char *keyreleased();

void keypressed(int k);

void omshell_main(int argc, char * argv[]);

#endif /* OMSHELL_H */

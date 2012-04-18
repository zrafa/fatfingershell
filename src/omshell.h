#ifdef __STDC__

#include <SDL/SDL.h>
#define	nkeys 41	/* for layout 1 */
#define	nkeys2 41	/* for layout 2 */


void quit_omshell(int error);

void Blitspecialkeycolors();

void TerminalBlit(int x1, int y1, int x2, int y2);

void Blitnormalkey(int k);

/* void mostrar_keyboard(); */

void TerminalCursor(int x,int y,int width,int height);

void TerminalDelCursor();

void TerminalRenderString(int x, int y, char *str, int len);

void TerminalFillRectangle (int x,int y);

void TerminalDrawImageString(int x,int y,unsigned char *str,int len, int b, int inverso, int extended);

void Terminal_Clear_Area(int x,int y,int width,int height);

void Terminal_Copy_Area(int src_x, int src_y, 
			unsigned int width, unsigned height, int dest_x, int dest_y);

void terminal_init();

void *vibration(void *arg);
void *playsound(void *arg);

void options(int *dark, int *v, int *s, int *fs, int argc, char * argv[]);

SDL_Surface *load_image(const char *f);

void load_images();

void load_sounds(); 

int check_ts();

char *keyreleased();

void keypressed(int k);

void terminal_update();

void load_kb_layout(int l, int n, const char *fn);	/* load key codes */


int main_omshell(int argc, char * argv[]);

#endif /* __STDC__ */

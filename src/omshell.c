/*
 * FatFingerShell - a virtual terminal for Openmoko
 *
 * Copyright (C) 2009-2012 Rafael Ignacio Zurita <rizurita@yahoo.com>
 *
 *   FatFingerShell is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Library General Public
 *   License as published by the Free Software Foundation;
 *   version 2 of the License.
 *
 *    You should have received a copy of the GNU Library General Public
 *    License along with this software (check COPYING); if not, write to the Free
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <termios.h>
#include <sys/select.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_ttf.h>

#include <linux/input.h>

#include "omshell.h"
#include "SDL_terminal.h"

#define	nkeys 41	/* for layouts */

/* #define DEBUG 1 */

int kb[2][nkeys][8]; /* x1, y1, x2, y2, key, shifted key, sound, status */

SDL_Surface *scr;	/* screen */

SDL_Surface *bg[6] ; 	/* bg = background: we have 2 different layous and 2
                      	 * colours for each layout = 4 
			 */
Mix_Chunk *ks[4][2];	/* key sounds, index 0 = pressed, index 1 = released 
			 * 4 key different sounds
			 */

int sound, vibracion;
int darkbackground;
int previouskey = 100; /* released */
int layout=0;	/* current layout */

SDL_Terminal *terminal;


int fd;
fd_set rset;
char buf[1024];

int cs[2][4]; 	/* colors: two set of colors, for "setcolor" and "foreground" 
		 * loaded from : colors.cfg
		 * format: r g b transparency
		 */


void quit_omshell(int error)
{
	int i;

	/* fd is the output of bash */
	close (fd);

	if (sound) {
		for (i=0; i<4; i++) {
			Mix_FreeChunk(ks[i][0]);
			Mix_FreeChunk(ks[i][1]);
		}
		Mix_CloseAudio(); 
	}

	for (i=0; i<4; i++)
		SDL_FreeSurface(bg[i]);

	SDL_Quit();
	exit(error);
}


/* 544 393 xmax ymax */
void getxy (int *x, int *y)
{
	(*x) = (*x) * 640 / 544;
	(*y) = (*y) * 458 / 393;
}


void Blitspecialkeycolors()
{
#ifdef DEBUG
	printf("blitspecialcolors\n");
#endif
	int i;
	int h, w;
	
	Uint32 color;

	for (i=0; i<nkeys; i++) {

		h = kb[layout][i][3] - kb[layout][i][1];
		w = kb[layout][i][2] - kb[layout][i][0];
	
		SDL_Rect dst = {kb[layout][i][0], kb[layout][i][1], w, h};

		if (kb[layout][i][7] == 1) {
			color = SDL_MapRGBA (scr->format, 0,255,0,0);
			SDL_BlitSurface(bg[layout*2+1], &dst, scr, &dst);
			TerminalBlit (kb[layout][i][0], kb[layout][i][1], w, h);
		}
	}

}


void TerminalBlit(int x1, int y1, int x2, int y2)
{
#ifdef DEBUG
	printf("terminalblit x1=%i,y1=%i   x2=%i,y2=%i\n",x1,y1,x2,y2);
#endif
	SDL_Rect dst = {x1, y1, x2, y2};
        SDL_SetAlpha (terminal->surface, SDL_SRCALPHA, 0);
	SDL_BlitSurface (terminal->surface, &dst, scr, &dst);
}

int cursorx=1000;
int cursory=1000;
int cursorw=1000;
int cursorh=1000;

void TerminalCursor(int x,int y,int width,int height)
{
#ifdef DEBUG
	printf("terminalcursor\n");
#endif

	int x1, y1;
	x1=x+2; y1=y;
	getxy(&x1,&y1);
	int w1, h1;
	w1=width; h1=height;
	getxy(&w1,&h1);
	Uint32 color = SDL_MapRGBA (scr->format, cs[1][0],cs[1][1],cs[1][2],255);
	
	SDL_Rect dst = {x1, y1, w1, h1};

	if ((cursorx!=1000) && ((cursorx!=x1) || (cursory!=y1))) {
		SDL_Rect dst2 = {cursorx, cursory, cursorw, cursorh};
		SDL_BlitSurface(bg[layout*2], &dst2, scr, &dst2);
		TerminalBlit (cursorx, cursory, cursorw, cursorh);
		SDL_UpdateRect(scr,cursorx,cursory,cursorw,cursorh);
	}

	cursorx=x1; cursory=y1; cursorw=w1; cursorh=h1;
	SDL_FillRect (scr, &dst, color);
	SDL_UpdateRect(scr,x1,y1,w1,h1);
}


/*
 * TerminalFillRectangle (imprime el cursor asi que el tamanio es fijo :( )
 */
void TerminalFillRectangle (int x,int y)
{
#ifdef DEBUG
	printf("fill rectangle\n");
#endif
	SDL_Color fg = terminal->fg_color;
	SDL_Color bgc = terminal->bg_color;
	terminal->fg_color = terminal->color;
	terminal->bg_color.unused = 255;

	int x1, y1;
	x1=x; y1=y;
	getxy(&x1,&y1);

	cursorx=x1;
	cursory=y1-2;
	terminal->fg_color = fg;
	terminal->bg_color = bgc;    

	SDL_Rect dst = {x1, y1,x1 + terminal->glyph_size.w, y1 + terminal->glyph_size.h};
	SDL_BlitSurface(bg[layout*2], &dst, scr, &dst);
	Blitspecialkeycolors();
	TerminalBlit (x1, y1,x1 + terminal->glyph_size.w, y1 + terminal->glyph_size.h);
	SDL_UpdateRect(scr, x1, y1, terminal->glyph_size.w, terminal->glyph_size.h);
}

/* see extend.png */
unsigned char extended_codes[32] = { 111, 72, 72, 70, 67, 76, 111, 43,
				     78, 86, 39, 46, 46, 39, 43, 45,
				     45, 45, 95, 95, 43, 43, 43, 43,
				     124, 60, 62, 109, 61, 69, 46, 32 };


void TerminalDrawImageString(int x,int y, unsigned char *str,int len, int b, int inverso, int extended)
{
#ifdef DEBUG
	printf("terminaldrawimagestring\n");
#endif
	SDL_Color fg = terminal->fg_color;
	SDL_Color bgc = terminal->bg_color;
	SDL_Color fg2;

	if (inverso) {
		fg2.r = 255 - fg.r;
		fg2.g = 255 - fg.g;
		fg2.b = 255 - fg.b;
		terminal->fg_color = fg2;
		terminal->bg_color = fg;
	}

	unsigned char linea[1024];
	int i;
	memcpy(linea, str, len);
	linea[len] = '\0';

	int x1, y1;
	x1=x; y1=y;
	getxy(&x1,&y1);

	if (b) 
		SDL_TerminalEnableBold (terminal);
		
	for (i=0; i<len; i++) {
		if ( linea[i] > 31) {
			if (extended && (linea[i]>95))
				linea[i] = extended_codes[(linea[i]-96)];
			SDL_TerminalRenderChar (terminal, 
				x1 + i * terminal->glyph_size.w,
				y1 - terminal->glyph_size.h +2,
				linea[i]);
		}
	}

	if (b) 
		SDL_TerminalDisableBold (terminal);

	SDL_Rect dst = {x1,y1 - terminal->glyph_size.h+2,x1 + len * terminal->glyph_size.w, y1};
	SDL_BlitSurface(bg[layout*2], &dst, scr, &dst);
	Blitspecialkeycolors();
	TerminalBlit (x1,y1 - terminal->glyph_size.h+2,x1 + len * terminal->glyph_size.w, y1);
	SDL_UpdateRect(scr,x1,y1 - terminal->glyph_size.h+2, len * terminal->glyph_size.w, terminal->glyph_size.h+2);

	if (inverso) {
		terminal->fg_color = fg;
		terminal->bg_color = bgc;
	}

}


/*
* Terminal_Clear_Area
*/
void Terminal_Clear_Area(int x,int y,int width,int height)
{
#ifdef DEBUG
	printf("terminalcleararea\n");
#endif
	int x1, y1;
	x1=x; y1=y;
	getxy(&x1,&y1);
	int w1, h1;
	w1=width; h1=height;
	getxy(&w1,&h1);
	Uint32 color = SDL_MapRGBA (terminal->surface->format, terminal->color.r, terminal->color.g, terminal->color.b, terminal->color.unused);

	SDL_SetAlpha (terminal->surface, 0, 0);
	SDL_Rect dst = {x1, y1, w1, h1};
	SDL_FillRect (terminal->surface, &dst, color);

	SDL_BlitSurface(bg[layout*2], &dst, scr, &dst);
	Blitspecialkeycolors();
	TerminalBlit (x1, y1, w1, h1);
	SDL_UpdateRect(scr, x1, y1, w1, h1);
}


/*
* Terminal_Copy_Area
*/
void Terminal_Copy_Area(int src_x, int src_y, 
		unsigned int width, unsigned height, int dest_x, int dest_y)
{
#ifdef DEBUG
	printf("terminalcopyarea\n");
#endif

	int x1, y1;
	x1=src_x; y1=src_y;
	getxy(&x1,&y1);
	int w1, h1;
	w1=width; h1=height;
	getxy(&w1,&h1);
	int x2, y2;
	x2=dest_x; y2=dest_y;
	getxy(&x2,&y2);

	SDL_Rect src = {x1, y1+2, w1, h1};
	SDL_Rect dst = {x2, y2+2, w1, h1};

	int error;

	SDL_SetAlpha (terminal->surface, 0, 0);

        error=SDL_BlitSurface (terminal->surface, &src, terminal->surface, &dst);
	if (error != 0)
		printf("ERROR SDL_BlitSurface!!!!\n");

	SDL_BlitSurface(bg[layout*2], &dst, scr, &dst);
	Blitspecialkeycolors();

	TerminalBlit (x2, y2+2, w1, h1);

	SDL_UpdateRect(scr, x2, y2+2, w1, h1);
}


void terminal_init()
{
	FILE *f;
	int i;

	f=fopen("colors.cfg","r");
	if (f == NULL) {
		printf("error fopen\n");
		exit(1);
	}

	for (i=0; i<2; i++) {
		fscanf (f, "%i %i %i %i", &cs[i][0], &cs[i][1], &cs[i][2], &cs[i][3]);
	}

	if (fclose(f) == EOF) {
		printf("error fclose\n");
		exit(1);
	};

	terminal =  SDL_CreateTerminal ();
	SDL_TerminalSetFont (terminal, "./VeraMono.ttf", 12);
	SDL_TerminalSetSize (terminal, 80, 24);
	SDL_TerminalSetPosition (terminal, 0, 0);


	SDL_TerminalSetColor (terminal, cs[0][0],cs[0][1],cs[0][2],cs[0][3]);
	SDL_TerminalSetBorderColor (terminal, 255,255,255,255);
	SDL_TerminalSetForeground (terminal, cs[1][0],cs[1][1],cs[1][2],cs[1][3]);
	SDL_TerminalSetBackground (terminal, 0,0,0,0); /* No background since alpha=0 */

	SDL_TerminalClear (terminal);
	SDL_TerminalPrint (terminal, "Terminal initialized\n");

}

int tecleando = 10;
/* int x1,y1; */
int kn=0, kx=0, ky=0;
int pulsado = 0;
int check_ts()
{
	int k=100; /* no key pressed */
	SDL_Event evento;
	int mb;
	int mx, my, i;
	int n=nkeys;

/* Para PC o tablets en general */

	while ( SDL_PollEvent(&evento) ) {
	 	   switch (evento.type){
			case SDL_MOUSEBUTTONDOWN:
				mb=SDL_GetMouseState(&mx, &my);
				printf("X=%d___Y=%d\n",mx,my);
				for (i=0; i<n; i++)
					if ((mx >= kb[layout][i][0]) && (mx <= kb[layout][i][2]) && (my >= kb[layout][i][1]) && (my <= kb[layout][i][3])) {
						k=i;
						break;
					};
				/* to guess where user wanted to press we modify a bit the position
 				 * because if the finger was the fat finger of the left hand then 
				 * it is very probable that the user wanted to press a bit more up
				 * and a bit more to the right. Check the fat finger and check where it
				 * touch the screen. Always the botton part of the fat finger touch
				 * first the screen. But we wanted to touch the screen with the part
				 * under the nail.
				 * I modify the behavior just in the middle up, where the fat finger
				 * must do an effort to press (and these are not in arrowhead position)
				 */
				break;
			case SDL_MOUSEBUTTONUP:
				k = 101;
				break;
			}
	}
/* FIN Para PC o tablets en general */

/* Para Openmoko Freerunner */
/*
	if ((tecleando<=4) && ((x1>=0) && (y1>=0))) {
		pulsado = 1;
		mx=x1; my=y1;
				if ((mx>250) && (mx<400) && (my<250)) {
				 	if (mx<320) { mx=mx+10; my=my-10; }
				 		else { mx=mx-10; my=my-10; } 
				}
				kx=kx+mx; ky=ky+my; kn++;
				mx=kx/kn; my=ky/kn;
				
				for (i=0; i<n; i++)
					if ((mx >= kb[layout][i][0]) && (mx <= kb[layout][i][2]) && (my >= kb[layout][i][1]) && (my <= kb[layout][i][3])) {
						k=i;
						break;
					};
	} else if ((tecleando>4) && pulsado) {
		pulsado = 0;
				k = 101;*/	/* key released */
/*
				kn=0; kx=0; ky=0;
		}
*/
/* FIN Para Openmoko Freerunner */
		     

	return k;
}

/* Para Openmoko Freerunner */
/*
void *teclear(void *arg)
{

        struct input_event ev0[64];

        int button=0, x=0, y=0, realx=0, realy=0, i, rd;


                event0_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
        while (1) {

                  button=0; x=0; y=0; realx=0; realy=0;

                  rd = read(event0_fd, ev0, sizeof(struct input_event) * 32);

                                if (rd != -1) { 
                  for (i = 0; i < (rd / sizeof(struct input_event) ); i++) {

                        if(ev0[i].type == 3 && ev0[i].code == 0) x = ev0[i].value;
                        else if (ev0[i].type == 3 && ev0[i].code == 1) y = ev0[i].value;
                        else if (ev0[i].type == 1 && ev0[i].code == 330) button = ev0[i].value << 2;
                        else if (ev0[i].type == 0 && ev0[i].code == 0 && ev0[i].value == 0) {
        
				
                                x1 = ((x-110) * 640) / (922-110);                     
                                y1 = ((y-105) * 480) / (928-105);



                       }
                }
				tecleando=0;
                } else {
			tecleando++;
		}

                usleep(10000);
        }
}
*/
/* FIN Para Openmoko Freerunner */



int vibrar = 0;

static void *vibration(void *arg) {

	char *p = "170\n"; /* power of vibr */
	char *p1 = "0\n"; /* power of vibr */
	int us = 600000; /* usleep in milliseconds */

	FILE *f;

	for(;;) {
		if (vibracion && vibrar) {

			f = fopen("/sys/class/leds/neo1973:vibrator/brightness", "w");
			fprintf(f, "%s", p);
			fclose(f);

			usleep(us);

			f = fopen("/sys/class/leds/neo1973:vibrator/brightness", "w");
			fprintf(f, "%s", p1);
			fclose(f);
			vibrar = 0;
		}

		SDL_Delay(20);
	}
	
	return NULL;
}

int play = 0;
int ksound, ktype;

static void *playsound(void *arg)
{
	/* phaserChannel */
	int c = -1;

	for(;;) {

		if (sound && play) {
			if (ksound <= nkeys)
				c = Mix_PlayChannel(-1, ks[((kb[layout][ksound][6])-1)][ktype], 0);
			play = 0;
		}

		SDL_Delay(20);
	}
}


static void options(int *dark, int *v, int *s, int argc, char * argv[])
{
	/* options */
	*dark=0;
	*v=0;
	*s=0;

	static const char *optstring = "dvsfh";
	int opt = getopt(argc, argv, optstring);
	
	while(opt != -1) {
		switch (opt) {
			case 'd':
				*dark = 1;  
				break;                                
			case 'v':
				*v = 1;  
				break;                                
			case 's':
				*s = 1;  
				break;                                
			case 'h':
				printf("usage: omshell [-v] [-s] [-f] \n\t -v : vibration \n\t -s : sound\n\t -d : dark background (black&white) (default: gray&black)\n");
				exit(0);                                
			default: break;
		}                
		opt = getopt(argc, argv, optstring);
	}

}

SDL_Surface *load_image(const char *f) {

	SDL_Surface *s, *st;

	printf("loading image..\n");

	st = IMG_Load(f);
	if (st == NULL)
		quit_omshell(1);

	s = SDL_DisplayFormat(st);
	if (s == NULL) {
		SDL_FreeSurface(st);
		quit_omshell(1);
	}

	SDL_FreeSurface(st);
	return s;

}


static void load_images(void) {

	if (darkbackground) {
		bg[0] = load_image("layout-k.bmp.png");	/* normal keyboard */
		bg[1] = load_image("layout-kp.bmp.png");	/* normal keyboard colour 1 */
		bg[2] = load_image("layout-np.bmp.png");	/* numeric pad */
		bg[3] = load_image("layout-npp.bmp.png");	/* numeric pad colour 1 */
		bg[4] = load_image("layout-kp2.bmp.png");	/* normal keyboard green */
		bg[5] = load_image("layout-npp2.bmp.png");	/* numeric pad green */
	} else {
		bg[1] = load_image("layout-k.bmp.png");	/* normal keyboard */
		bg[0] = load_image("layout-kp.bmp.png");	/* normal keyboard colour 1 */
		bg[3] = load_image("layout-np.bmp.png");	/* numeric pad */
		bg[2] = load_image("layout-npp.bmp.png");	/* numeric pad colour 1 */
		bg[4] = load_image("layout-kp2.bmp.png");	/* normal keyboard green */
		bg[5] = load_image("layout-npp2.bmp.png");	/* numeric pad green */
	}
}

static void load_sound(Mix_Chunk *mc, const char *t) {

	mc = Mix_LoadWAV(t);
	Mix_VolumeChunk(mc, 64);
}

static void load_sounds(void) {

	printf("loading sounds..");

	if (sound) {

		load_sound(ks[0][0], "k1.wav");	/* key 1 pressed */ 
		load_sound(ks[0][1], "k11.wav");	/* key 1 released */ 
		load_sound(ks[1][0], "k2.wav");	/* key 2 pressed */ 
		load_sound(ks[1][1], "k22.wav");	/* key 2 released */ 
		load_sound(ks[2][0], "k3.wav");	/* key 3 pressed */ 
		load_sound(ks[2][1], "k33.wav");	/* key 3 released */ 
		load_sound(ks[3][0], "k4.wav");	/* key 4 pressed */ 
		load_sound(ks[3][1], "k44.wav");	/* key 4 released */ 
	}
}


static void Blitnormalkey(int k) {

#ifdef DEBUG
	printf("blitnormalkey\n");
#endif
	int h, w;

	h = kb[layout][k][3] - kb[layout][k][1];
	w = kb[layout][k][2] - kb[layout][k][0];

	SDL_Rect orig = {kb[layout][k][0], kb[layout][k][1], w, h};
	SDL_Rect dest = {kb[layout][k][0], kb[layout][k][1], w, h};

	SDL_BlitSurface(bg[layout*2], &orig, scr, &dest);

	Blitspecialkeycolors();
	TerminalBlit(kb[layout][k][0], kb[layout][k][1], w, h);
	SDL_UpdateRect(scr, kb[layout][k][0], kb[layout][k][1], w, h);
}

char *keyreleased()
{
#ifdef DEBUG
	printf("keyreleased\n");
#endif
	char letra=0;
	char *s=malloc(2);
	int i,j;

	int k;

	if (previouskey >= 100) {
		letra='\0'; 
		sprintf(s,"%c", letra); 
		return s; 
	}

	k=previouskey;

	ksound = previouskey;
	ktype = 1;
	play = 1;
	vibrar = 1;

	previouskey = 100;

	SDL_Event backspace;
	backspace.type = SDL_KEYDOWN;
	backspace.key.keysym.sym = SDLK_BACKSPACE;

	if ((kb[layout][k][4] == 102) && (kb[layout][k][5] == 102)) { /* fn key , f=102 ascii*/
		letra='\0';
		layout++;
		if (layout == 2) layout = 0;
		kb[0][k][7]++;
		if (kb[0][k][7]==2) kb[0][k][7] = 0;
		SDL_BlitSurface(bg[layout*2], NULL, scr, NULL);
		Blitspecialkeycolors();
		SDL_TerminalBlit(terminal);
		SDL_Flip(scr);
	} else if ((kb[layout][k][4] == 119) && (kb[layout][k][5] == 119)) /* esc key , w=119 ascii*/
		letra=27;

	else if ((kb[layout][k][4] == 101) && (kb[layout][k][5] == 101)) { /* ENTER key , e=101 ascii*/
		letra='\n';
	} else if ((kb[layout][k][4] == 112) && (kb[layout][k][5] == 112)) { /* SPACE key , p=112 ascii*/
		letra=32;
	} else if ((kb[layout][k][4] == 116) && (kb[layout][k][5] == 116)) { /* TAB key , t=116 ascii*/
		letra='\t';
	} else if ((kb[layout][k][4] == 99) && (kb[layout][k][5] == 99)) { /* CTRL key , c=99 ascii*/
		letra='\0';
		kb[layout][k][7]++;
		if (kb[layout][k][7]==2) kb[layout][k][7] = 0;
	} else if ((kb[layout][k][4] == 115) && (kb[layout][k][5] == 115)) { /* SHIFT key , s=115 ascii*/
		letra='\0';
		kb[layout][k][7]++;
		if (kb[layout][k][7]==2) kb[layout][k][7] = 0;
	} else if ((kb[layout][k][4] == 98) && (kb[layout][k][5] == 98)) { /* BACKSPACE key , b=98 ascii*/
		letra='\b';
	} else {
		for (i=0;i<nkeys;i++) {
			if ((kb[layout][i][4] == 115) && (kb[layout][i][5] == 115)) { /* SHIFT key , s=115 ascii*/
				if (kb[layout][i][7])
                			letra=kb[layout][k][5];

				if (kb[layout][i][7] == 1) {
					kb[layout][i][7] = 0;
					Blitnormalkey(i);
				}
			} else if ((kb[layout][i][4] == 99) && (kb[layout][i][5] == 99)) { /* CTRL key , s=99 ascii*/ 
				if (kb[layout][i][7]) {
					if (kb[layout][k][4]==44) 	/* CTRL+',' = page up */
						letra=201;
					else if (kb[layout][k][4]==46) 	/* CTRL+',' = page down */
						letra=202;
					else {
						for (j=65;j<96;j++)	/* if 'A' (dec ascii 65), then ctrl+A (dec ascii 1), ... etc */
							if (kb[layout][k][5]==j) {
								letra=j-64;	/* 'A'==65, then ctrl+A==1, ... etc */
								break;
							}
					}

					kb[layout][i][7] = 0;
					Blitnormalkey(i);
				}
			}
					
				
		}
		if (!letra)
			if ((k>=0) && (k<=nkeys))
	                	letra=kb[layout][k][4];
        }

	Blitnormalkey(k);
	
	sprintf(s,"%c",letra);
	return s;
}

void keypressed(int k)
{
#ifdef DEBUG
	printf("keypressed\n");
#endif

	if (previouskey == 100) {
		ksound = k;
		ktype = 0;
		play = 1;
		vibrar = 1;
	}

	if (previouskey != k)
		Blitnormalkey(previouskey);
	
	previouskey = k;

}


/* load key codes */
static void load_kb_layout(int l, int n, const char *fn) {

	FILE *f;
	int i;

	f = fopen(fn,"r");
	if (f == NULL) {
		printf("error fopen\n");
		exit(1);
	}

	for (i=0; i<n; i++) {
		fscanf (f, "%i %i %i %i %c %c %i",
				 &kb[l][i][0], &kb[l][i][1],
				 &kb[l][i][2], &kb[l][i][3],
				 &kb[l][i][4], &kb[l][i][5],
				 &kb[l][i][6]);
		kb[l][i][7] = 0;
	}

	if (fclose(f) == EOF) {
		printf("error fclose\n");
		exit(1);
	};

}


void main_omshell(int argc, char * argv[]) {

	options(&darkbackground, &vibracion, &sound, argc, argv);

	load_kb_layout(0,nkeys,"keyboard.cfg");		/* load key codes */
	load_kb_layout(1,nkeys,"keyboard2.cfg");	/* load key codes */

	/* Init SDL: */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0 ) {
		fprintf(stderr, "Can't init: %s\n", SDL_GetError());
		exit(1);
	}

	/* Open scr: */
	scr = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE);
	if (scr == NULL){
		printf("No se pudo iniciar el modo grafico %s\n",SDL_GetError());
		exit(1);
	}


	/* Open audio */
	if (sound) {
		if (Mix_OpenAudio(8000, AUDIO_U8, 1, 1024) < 0) {
			fprintf(stderr, "Warning: Audio could not be setup \nReason: %s\n", SDL_GetError());
			/* exit(1); */
			sound = 0;
		}
	}

	if (sound)
		load_sounds();

	load_images();
	terminal_init();

	/* Iniciamos los threads para el sonido y la vibracion */

	pthread_t snd;  /* soud thread */
	pthread_t vib;  /* thread del vibrador */

	if (sound)
		pthread_create(&snd, NULL, playsound, NULL);

	if (vibracion)
		pthread_create(&vib, NULL, vibration, NULL);

	/* thread del teclado */
	/* Para Openmoko 
	pthread_t tec; 
	pthread_create(&tec,NULL,teclear,NULL);
	FIN prar Openmoko */

	SDL_BlitSurface(bg[layout*2], NULL, scr, NULL);
	Blitspecialkeycolors();
	SDL_TerminalBlit (terminal);
	SDL_Flip(scr);
}

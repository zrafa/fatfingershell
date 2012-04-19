/*  Copyright 1992, 1994 John Bovey, University of Kent at Canterbury.
 *
 *  Redistribution and use in source code and/or executable forms, with
 *  or without modification, are permitted provided that the following
 *  condition is met:
 *
 *  Any redistribution must retain the above copyright notice, this
 *  condition and the following disclaimer, either as part of the
 *  program source code included in the redistribution or in human-
 *  readable materials provided with the redistribution.
 *
 *  THIS SOFTWARE IS PROVIDED "AS IS".  Any express or implied
 *  warranties concerning this software are disclaimed by the copyright
 *  holder to the fullest extent permitted by applicable law.  In no
 *  event shall the copyright-holder be liable for any damages of any
 *  kind, however caused and on any theory of liability, arising in any
 *  way out of the use of, or inability to use, this software.
 *
 *  -------------------------------------------------------------------
 *
 *  In other words, do not misrepresent my work as your own work, and
 *  do not sue me if it causes problems.  Feel free to do anything else
 *  you wish with it.
 */

char xvt_screen_c_sccsid[] = "@(#)screen.c	1.6 11/1/94 (UKC)";

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "xvt.h"
#include "screen.h"
#include "command.h"
#include "ttyinit.h"

#include "omshell.h"

/*  Structure describing the current state of the screen.
 */
struct screenst {
	unsigned char **text;	/* backup copy of screen->text */
	unsigned char **rend;	/* character rendition styles etc. */
	int row;	/* cursor position */
	int col;	/* ditto */
	int tmargin;	/* top scroll margin */
	int bmargin;	/* bottom scroll margin */
	int decom;	/* origin mode flag */
	int wrap;	/* auto-wrap flag */
	int wrap_next;	/* wrap before the next printed character */
	int insert;	/* insert mode flag */
};

/*  structure describing a saved line
 */
struct slinest {
	unsigned char *sl_text;	/* the text of the line */
	unsigned char *sl_rend;	/* the rendition style */
	int sl_length;	/* length of the line */
};

/*  selection endpoint types.
 */
enum seltype {
	SCREEN,
	SAVED,
	NOSEL
};

/*  structure describing a selection endpoint.
 */
struct selst {
	enum seltype se_type;
	int se_index;	/* index into the sline or screen array */
	int se_col;	/* column of the character */
};

#ifdef __STDC__
static void change_offset(int);
static void paint_rval_text(unsigned char *,int,int,int,int);
static void paint_rvec_text(unsigned char *,unsigned char *,int,int,int);
static void refresh(int,int,int,int);
static void show_selection(int,int,int,int);
static void scroll(int,int,int);
static void scroll1(int);
static void home_screen(void);
static void cursor(void);
static void selend_to_rc(int *,int *,struct selst *);
static int selcmp(struct selst *,struct selst *);
static void check_selection(int,int);
#else /*  __STDC__ */
static void change_offset();
static void paint_rval_text();
static void paint_rvec_text();
static void refresh();
static void show_selection();
static void scroll();
static void scroll1();
static void home_screen();
static void cursor();
static void selend_to_rc();
static int selcmp();
static void check_selection();
#endif /*  __STDC__ */

#define MAX_WIDTH 250	/* max width of selected lines */
#define PROP_SIZE 1024	/* chunk size for retrieving the selection property */

#define SEL_KEY_DEL 2000 /* time delay in allowing keyboard input to be accepted
			    before a selection arrives. */

#define POUND	036	/* ascii value representing a pound in at least some fonts */

/*  Character classes used when selecting words with a double click.
 */
static int char_class[256] = {
	    32,   1,   1,   1,   1,   1,   1,   1,
	     1,  32,   1,   1,   1,   1,   1,   1,
	     1,   1,   1,   1,   1,   1,   1,   1,
	     1,   1,   1,   1,   1,   1,   1,   1,
	    32,  33,  34,  35,  36,  37,  38,  39,
	    40,  41,  42,  43,  44,  45,  46,  47,
	    48,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  58,  59,  60,  61,  62,  63,
	    64,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  48,  91,  92,  93,  94,  48,
	    96,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  48,  48,  48,  48,  48,  48,
	    48,  48,  48, 123, 124, 125, 126,   1,
	     1,   1,   1,   1,   1,   1,   1,   1,
	     1,   1,   1,   1,   1,   1,   1,   1,
	     1,   1,   1,   1,   1,   1,   1,   1,
	     1,   1,   1,   1,   1,   1,   1,   1,
	   160, 161, 162, 163, 164, 165, 166, 167,
	   168, 169, 170, 171, 172, 173, 174, 175,
	   176, 177, 178, 179, 180, 181, 182, 183,
	   184, 185, 186, 187, 188, 189, 190, 191,
	   192, 193, 194, 195, 196, 197, 198, 199,
	   200, 201, 202, 203, 204, 205, 206, 207,
	   208, 209, 210, 211, 212, 213, 214, 215,
	   216, 217, 218, 219, 220, 221, 222, 223,
	   224, 225, 226, 227, 228, 229, 230, 231,
	   232, 233, 234, 235, 236, 237, 238, 239,
	   240, 241, 242, 243, 244, 245, 246, 247,
	   248, 249, 250, 251, 252, 253, 254, 255
};

/*  External global variables that are initialised at startup.
 */
extern Display		*display;
extern Window		vt_win;
extern Window		main_win;
extern Colormap	colormap;
extern XFontStruct	*mainfont;	/* main font structure */
extern XFontStruct	*boldfont;	/* bold font structure */
extern GC 		txgc;		/* GC for drawing text */
extern GC		negc;		/* GC without graphics exposures */
extern GC		hlgc;		/* GC used for highlighting selection */
extern GC		cugc;		/* GC used for the text cursor */
extern unsigned long	foreground;	/* foreground pixel value */
extern unsigned long	background;	/* background pixel value */
extern int		reverse_wrap;	/* reverse wrap allowed */

static struct selst selend1, selend2;	/* the selection endpoints */

/*  Screen state variables that are the same for both screens.
 */
static int pwidth;	/* width in pixels */
static int pheight;	/* height in pixels */
static int cwidth = 0;	/* width in characters */
static int cheight = 0;	/* height in characters */
static int focus = 0;	/* window has the keyboard focus */
static int rstyle;	/* rendition style and current character flags */
static int gg[2];	/* character set Gn */
static int cset;	/* which character set is currently active */
static int save_rstyle;	/* when it needs to be saved */

/*  screen state variables
 */
static struct screenst screen1;	/* main screen */
static struct screenst screen2;	/* second screen */
static struct screenst *screen = NULL;	/* current of screen1 and screen2 */
static struct screenst save_screen;

static int sline_top;	/* high water mark of saved scrolled lines */
static int sline_max;	/* Max number of saved lines */
static struct slinest **sline;	/* main array of saved lines */
static int offset = 0;	/* current vertical offset for displaying saved lines */

static int fheight;	/* height of a character in the font */
static int fwidth;	/* width of a font character */

/*  Perform any initialisation on the screen data structures.  Called just once
 *  at startup.  saved_lines is the number of saved lines.
 */
void
scr_init(saved_lines)
int saved_lines;
{
	int i;

	/*  Initialise the array of lines that have scrolled of the top.
	 */
	sline_max = saved_lines;
	if (sline_max < MAX_SCROLL)
		sline_max = MAX_SCROLL;
	sline = (struct slinest **)cmalloc(sline_max * sizeof(struct slinest *));
	for (i = 0; i < sline_max; i++)
		sline[i] = NULL;
	sline_top = 0;

	screen1.text = NULL;
	screen1.rend = NULL;
	screen1.row = 0;
	screen1.col = 0;
	screen1.wrap = 1;
	screen1.decom = 0;
	screen1.insert = 0;
	screen2.text = NULL;
	screen2.rend = NULL;
	screen2.row = 0;
	screen2.col = 0;
	screen2.wrap = 1;
	screen2.decom = 0;
	screen2.insert = 0;
	save_screen.row = 0;
	save_screen.col = 0;
	screen = &screen1;
	rstyle = 0;
	gg[0] = CS_USASCII;
	gg[1] = CS_USASCII;
	cset = 0;

/* OJO CON ESTO QUE ES SOLO UN HACK DE PRUEBA */
	fwidth = 6;
	fheight = 13;
/* OJO CON ESTO QUE ES SOLO UN HACK DE PRUEBA */

	scr_reset();
}

/*  Reset the screen - called whenever the screen needs to be repaired completely.
 */
void
scr_reset()
{
	int x, y, n, i, j, onscreen;
	unsigned int width, height, border_width, depth;
	int cw, ch;
	unsigned char **r1, **r2, **s1, **s2;
	struct slinest *sl;

	/* I ran once XGetGeometry and I am harcoded (bad) values for x, y, etc..
	 * XGetGeometry(display,vt_win,&root,&x,&y,&width,&height,&border_width,&depth);
	 */
	x=0; y=0; width=484; height=316; border_width=0; depth=16;

	cw = (width - 2 * MARGIN) / fwidth;
	ch = (height - 2 * MARGIN) / fheight;

	if (screen->text == NULL || cw != cwidth || ch != cheight) {

		offset = 0;
		/*  Recreate the screen backup arrays.
		 *  The screen arrays are one byte wider than the screen and
		 *  the last byte is used as a flag which is non-zero of the
		 *  line wrapped automatically.
		 */
		s1 = (unsigned char **)cmalloc(ch * sizeof(unsigned char *));
		s2 = (unsigned char **)cmalloc(ch * sizeof(unsigned char *));
		r1 = (unsigned char **)cmalloc(ch * sizeof(unsigned char *));
		r2 = (unsigned char **)cmalloc(ch * sizeof(unsigned char *));
		for (y = 0; y < ch; y++) {
			s1[y] = (unsigned char *)cmalloc(cw + 1);
			s2[y] = (unsigned char *)cmalloc(cw + 1);
			r1[y] = (unsigned char *)cmalloc(cw + 1);
			r2[y] = (unsigned char *)cmalloc(cw + 1);
			memset(s1[y],0,cw + 1);
			memset(s2[y],0,cw + 1);
			memset(r1[y],0,cw + 1);
			memset(r2[y],0,cw + 1);
		}
		
		if (screen1.text != NULL) {

			/*  Now fill up the screen from the old screen and saved lines.
			 */
			if (screen1.row >= ch) {
				/* scroll up to save any lines that will be lost.
				 */
				scroll1(screen1.row - ch + 1);
				screen1.row = ch - 1;
			}
			/* calculate working no. of lines.
			 */
			i = sline_top + screen1.row + 1;
			j = i > ch ? ch - 1 : i - 1;
			i = screen1.row;
			screen1.row = j;
			onscreen = 1;
			for (; j >= 0; j--) {
				if (onscreen) {
					n = cw < cwidth ? cw : cwidth;
					memcpy(s1[j],screen1.text[i],n);
					memcpy(s2[j],screen2.text[i],n);
					memcpy(r1[j],screen1.rend[i],n);
					memcpy(r2[j],screen2.rend[i],n);
					s1[j][cw] = screen1.text[i][cwidth];
					s2[j][cw] = screen2.text[i][cwidth];
					r1[j][cw] = screen1.rend[i][cwidth];
					r2[j][cw] = screen2.rend[i][cwidth];
					i--;
					if (i < 0) {
						onscreen = 0;
						i = 0;
					}
				} else {
					if (i >= sline_top)
						break;
					sl = sline[i];
					n = cw < sl->sl_length ? cw : sl->sl_length;
					memcpy(s1[j],sl->sl_text,n);
					free(sl->sl_text);
					if (sl->sl_rend != NULL) {
						memcpy(r1[j],sl->sl_rend,n);
						r1[j][cw] = sl->sl_rend[sl->sl_length];
						free(sl->sl_rend);
					}
					free((void *)sl);
					i++;
				}
			}
			if (onscreen)
				abort();
			for (j = i; j < sline_top; j++)
				sline[j - i] = sline[j];
			for (j = sline_top - i; j < sline_top; j++)
				sline[j] = NULL;
			sline_top -= i;

			for (y = 0; y < cheight; y++) {
				free(screen1.text[y]);
				free(screen2.text[y]);
				free(screen1.rend[y]);
				free(screen2.rend[y]);
			}
			free((void *)screen1.text);
			free((void *)screen2.text);
			free((void *)screen1.rend);
			free((void *)screen2.rend);
		}
		screen1.text = s1;
		screen2.text = s2;
		screen1.rend = r1;
		screen2.rend = r2;

		cwidth = cw;
		cheight = ch;
		pwidth = width;
		pheight = height;
		screen1.tmargin = 0;
		screen1.bmargin = cheight - 1;
		screen1.decom = 0;
		screen1.wrap_next = 0;
		screen2.tmargin = 0;
		screen2.bmargin = cheight - 1;
		screen2.decom = 0;
		screen2.wrap_next = 0;
	}
	tty_set_size(cwidth,cheight);

	if (screen->col >= cwidth)
		screen->col = cwidth - 1;
	if (screen->row >= cheight)
		screen->row = cheight - 1;
	refresh(0,cheight - 1,0,cwidth - 1);
	cursor();
}

/*  Refresh the reagion of the screen delimited by the aruments.  Used to
 *  repair after minor exposure events.
 */
void
scr_refresh(x,y,width,height)
int x, y, width, height;
{
	int row1, row2, col1, col2;

	col1 = (x - MARGIN) / fwidth;
	col2 = (x + width - MARGIN + fwidth - 1) / fwidth - 1;
	if (col1 < 0)
		col1 = 0;
	if (col1 >= cwidth)
		col1 = cwidth - 1;
	if (col2 < 0)
		col2 = 0;
	if (col2 >= cwidth)
		col2 = cwidth - 1;
	row1 = (y - MARGIN) / fheight;
	row2 = (y + height - MARGIN + fheight - 1) / fheight - 1;
	if (row1 < 0)
		row1 = 0;
	if (row1 >= cheight)
		row1 = cheight - 1;
	if (row2 < 0)
		row2 = 0;
	if (row2 >= cheight)
		row2 = cheight - 1;
	refresh(row1,row2,col1,col2);
	if (screen->row >= row1 && screen->row <= row2 &&
	    screen->col >= col1 && screen->col <= col2)
		cursor();
}

/*  Parse the string as a sequence of character classes and use it to
 *  modify the char_class table.
 */
void
scr_char_class(s)
unsigned char *s;
{
	int first, last, value, i;

	while (isdigit(*s)) {
		first = last = value = 0;
		while (isdigit(*s)) {
			first = first * 10 + *s - '0';
			s++;
		}
		if (*s == '-') {
			s++;
			while (isdigit(*s)) {
				last = last * 10 + *s - '0';
				s++;
			}
		} else
			last = first;
		if (*s == ':')
			s++;
		while (isdigit(*s)) {
			value = value * 10 + *s - '0';
			s++;
		}
		if (*s == ',')
			s++;
		if (last > 255)
			last = 255;
		for (i = first; i <= last; i++)
			char_class[i] = value;
	}
}

/*  Handle a backspace
 */
void
scr_backspace()
{
	if (screen->col == 0 && 0 && screen->row > 0) {
		cursor();
		screen->row--;
		screen->col = cwidth - 1;
		cursor();
	} else if (screen->wrap_next && 0)
		screen->wrap_next = 0;
	else
		scr_move(-1,0,COL_RELATIVE|ROW_RELATIVE);
}

/*  Ring the bell
 */
void
scr_bell()
{
}

/*  Change between the alternate and the main screens
 */
void
scr_change_screen(direction)
int direction;
{
	home_screen();
	screen = (direction == HIGH) ? &screen2 : &screen1;
	selend2.se_type = NOSEL;
	refresh(0,cheight - 1,0,cwidth - 1);
	cursor();
}

/*  Change the rendition style.
 */
void
scr_change_rendition(style)
int style;
{
	if (style == 0)
		rstyle &= ~RS_STYLE;
	else
		rstyle |= style;
}

/*  Set character class set to style.
 */
void
scr_set_char_set(set,style)
int set, style;
{
	gg[set] = style;
	if (cset == set)
		rstyle = (rstyle & ~CS_STYLE) | style;
}

/*  Set which character set we are using.
 */
void
scr_shift(set)
int set;
{
	cset = set;
	rstyle = (rstyle & ~CS_STYLE) | gg[cset];
}

/*  Return the width and height of the screen.
 */
void
scr_get_size(width_p,height_p)
int *width_p, *height_p;
{
	*width_p = cwidth;
	*height_p = cheight;
}

/*  Indicate a change of keyboard focus.  type is 1 for entry events and 2 for
 *  focus events.
 */
void
scr_focus(type,is_in)
int type,is_in;
{
/*
	cursor();
	if (is_in)
		focus |= type;
	else
		focus &= ~type;
	cursor();
*/
}

/*  Display the string at the current position.  nlcount is the number of new lines
 *  in the string.
 */
void
scr_string(str,len,nlcount)
unsigned char *str;
int len, nlcount;
{
	int x, x2, y, n, i;
	unsigned int width;
	unsigned char *s, *r;

	home_screen();
	/* para openmoko descomento */
	cursor();
	if (nlcount > 0) {
		if (screen->row > screen->bmargin)
			nlcount = 0;
		else
			nlcount -= screen->bmargin - screen->row;
		if (nlcount < 0)
			nlcount = 0;
		else if (nlcount > screen->row - screen->tmargin)
			nlcount = screen->row - screen->tmargin;
		if (nlcount > MAX_SCROLL)
			nlcount = MAX_SCROLL;
		scroll(screen->tmargin,screen->bmargin,nlcount);
		screen->row -= nlcount;
	}
	while (len > 0) {
		if (*str == '\n') {
			if (screen->row == screen->bmargin)
				scroll(screen->tmargin,screen->bmargin,1);
			else if (screen->row < cheight - 1)
				screen->row++;
			check_selection(screen->row,screen->row);
			screen->wrap_next = 0;
			len--;
			str++;
			continue;
		}
		if (*str == '\r') {
			screen->col = 0;
			screen->wrap_next = 0;
			len--;
			str++;
			continue;
		}
		if (*str == '\t') {
			if (screen->col < cwidth - 1) {
				s = screen->text[screen->row];
				if (s[screen->col] == 0)
					s[screen->col] = '\t';
				screen->col++;
				while (screen->col % 8 != 0 && screen->col < cwidth - 1)
					screen->col++;
			}
			len--;
			str++;
			continue;
		}
		if (screen->wrap_next) {
			screen->text[screen->row][cwidth] = 1;
			if (screen->row == screen->bmargin)
				scroll(screen->tmargin,screen->bmargin,1);
			else if (screen->row < cheight - 1)
				screen->row++;
			screen->col = 0;
			screen->wrap_next = 0;
		}
		check_selection(screen->row,screen->row);
		x = MARGIN + fwidth * screen->col;
		y = MARGIN + fheight * screen->row;
		for (n = 0; str[n] >= ' '; n++)
			;
		n = n + screen->col > cwidth ? cwidth - screen->col : n;
		if (screen->insert) {
			s = screen->text[screen->row];
			r = screen->rend[screen->row];
			for (i = cwidth - 1; i >= screen->col + n; i--) {
				s[i] = s[i - n];
				r[i] = r[i - n];
			}
			width = (cwidth - screen->col - n) * fwidth;
			x2 = x + n * fwidth;
			if (width > 0) {
				omshell_Terminal_Copy_Area(x,y,width,fheight,x2,y);
			}
		}

		paint_rval_text(str,rstyle,n,x,y);
		memcpy(screen->text[screen->row] + screen->col,str,n);
		if (rstyle == 0)
			memset(screen->rend[screen->row] + screen->col,0,n);
		else {
			for (i = 0; i < n; i++)
				screen->rend[screen->row][screen->col + i] = rstyle;
			screen->rend[screen->row][cwidth] = 1;
		}
		len -= n;
		str += n;
		screen->col += n;
		if (len > 0 && screen->col == cwidth && *str >= ' ') {
			if (screen->wrap) {
				screen->text[screen->row][cwidth] = 1;
				if (screen->row == screen->bmargin)
					scroll(screen->tmargin,screen->bmargin,1);
				else
					screen->row++;
				screen->col = 0;
			} else {
				screen->col = cwidth - 1;
				cursor();
				return;
			}
		}
	}
	if (screen->col == cwidth) {
		screen->col = cwidth - 1;
		screen->wrap_next = screen->wrap;
	}
	/* para openmoko descomento */
	cursor();
}

/*  Return true if the character is one that can be handled by scr_string()
 */
int
is_string_char(c)
int c;
{
	c &= 0177;
	return(c >= ' ' || c == '\n' || c == '\r' || c == '\t');
}

/*  Move the cursor to a new position.  The relative argument is a pair of
 *  flags that specify relative rather than absolute motion.
 */
void
scr_move(x,y,relative)
int x, y, relative;
{
	home_screen();
	cursor();
	screen->col = (relative & COL_RELATIVE) ? screen->col + x : x;
	if (screen->col < 0)
		screen->col = 0;
	if (screen->col >= cwidth)
		screen->col = cwidth - 1;

	if (relative & ROW_RELATIVE) {
		if (y > 0) {
			if (screen->row <= screen->bmargin && screen->row + y > screen->bmargin)
				screen->row = screen->bmargin;
			else
				screen->row += y;
		} else if (y < 0) {
			if (screen->row >= screen->tmargin && screen->row + y < screen->tmargin)
				screen->row = screen->tmargin;
			else
				screen->row += y;
		}
	} else {
		if (screen->decom) {
			screen->row = y + screen->tmargin;
			if (screen->row > screen->bmargin)
				screen->row = screen->bmargin;
		} else
			screen->row = y;
	}
	if (screen->row < 0)
		screen->row = 0;
	if (screen->row >= cheight)
		screen->row = cheight - 1;

	screen->wrap_next = 0;
	check_selection(screen->row,screen->row);
	cursor();
}

/*  Move the cursor down one line and scroll if necessary.
 */
void
scr_index()
{
	home_screen();
	cursor();
	if (screen->row == screen->bmargin)
		scroll(screen->tmargin,screen->bmargin,1);
	else
		screen->row++;
	screen->wrap_next = 0;
	check_selection(screen->row,screen->row);
	cursor();
}

/*  Move the cursor up one line and scroll if necessary.
 */
void
scr_rindex()
{
	home_screen();
	cursor();
	if (screen->row == screen->tmargin)
		scroll(screen->tmargin,screen->bmargin,-1);
	else
		screen->row--;
	screen->wrap_next = 0;
	check_selection(screen->row,screen->row);
	cursor();
}

/*  Save the cursor position and rendition style.
 */
void
scr_save_cursor()
{
	save_screen.row = screen->row;
	save_screen.col = screen->col;
	save_rstyle = rstyle;
}

/*  Restore the cursor position and rendition style.
 */
void
scr_restore_cursor()
{
	cursor();
	screen->row = save_screen.row;
	if (screen->row >= cheight)
		screen->row = cheight - 1;
	screen->col = save_screen.col;
	if (screen->col >= cwidth)
		screen->col = cwidth - 1;
	scr_change_rendition(save_rstyle);
	cursor();
}

/*  erase part or the whole of a line
 */
void
scr_erase_line(mode)
int mode;
{
	int i, x, y, width, height;
	unsigned char *r, *s;

	home_screen();
	y = MARGIN + screen->row * fheight;
	height = fheight;
	s = screen->text[screen->row];
	r = screen->rend[screen->row];
	switch (mode) {
	    case START :
		x = MARGIN;
		width = (screen->col + 1) * fwidth;
		memset(s,0,screen->col + 1);
		memset(r,0,screen->col + 1);
		break;
	    case END :
		x = MARGIN + screen->col * fwidth;
		width = (cwidth - screen->col) * fwidth;
		memset(s + screen->col,0,cwidth - screen->col + 1);
		memset(r + screen->col,0,cwidth - screen->col);
		break;
	    case ENTIRE :
		x = MARGIN;
		width = cwidth * fwidth;
		memset(s,0,cwidth + 1);
		memset(r,0,cwidth);
		break;
	    default :
		return;
	}
	/*  patch in the final rendition flag if there is any non-zero
	 *  rendition.
	 */
	r[cwidth] = 0;
	for (i = 0; i < cwidth; i++)
		if (r[i] != 0) {
			r[cwidth] = 1;
			break;
		}
	cursor();
	check_selection(screen->row,screen->row);
	omshell_Terminal_Clear_Area(x,y,width,height);
	screen->wrap_next = 0;
	cursor();
}

/*  erase part or the whole of the screen
 */
void
scr_erase_screen(mode)
int mode;
{
	int x, y, width, height;
	int i;

	home_screen();
	screen->wrap_next = 0;
	x = MARGIN;
	width = fwidth * cwidth;
	switch (mode) {
	    case START :
		y = MARGIN;
		height = screen->row * fheight;
		for (i = 0; i < screen->row; i++) {
			memset(screen->text[i],0,cwidth + 1);
			memset(screen->rend[i],0,cwidth + 1);
		}
		check_selection(0,screen->row - 1);
		if (height > 0) {
			omshell_Terminal_Clear_Area(x, y, width, height);
		}
		scr_erase_line(mode);
		break;
	    case END :
		if (screen->row != 0 || screen->col != 0) {
			y = MARGIN + (screen->row + 1) * fheight;
			height = (cheight - screen->row - 1) * fheight;
			for (i = screen->row + 1; i < cheight; i++) {
				memset(screen->text[i],0,cwidth + 1);
				memset(screen->rend[i],0,cwidth + 1);
			}
			check_selection(screen->row + 1,cheight - 1);
			if (height > 0) {
				omshell_Terminal_Clear_Area(x, y, width, height);
			}
			scr_erase_line(mode);
			break;
		}
		/*  If we are positioned at the top left hand corner then
		 *  it is effectively a whole screen clear.
		 *  Drop through so that we do not need to duplicate
		 *  the scroll-up code.
		 */
	    case ENTIRE :
		y = MARGIN;
		height = cheight * fheight;
		if (screen == &screen1)
			scroll1(cheight);
		else
			for (i = 0; i < cheight; i++) {
				memset(screen->text[i],0,cwidth + 1);
				memset(screen->rend[i],0,cwidth + 1);
			}
		cursor();
		check_selection(0,cheight - 1);
		omshell_Terminal_Clear_Area(x, y, width, height);
		cursor();
		break;
	    default :
		return;
	}
}

/*  Delete count lines and scroll up the bottom of the screen to fill the gap
 */
void
scr_delete_lines(count)
int count;
{
	if (count > screen->bmargin - screen->row + 1)
		return;

	home_screen();
	cursor();
	while (count > MAX_SCROLL) {
		scroll(screen->row,screen->bmargin,MAX_SCROLL);
		count -= MAX_SCROLL;
	}
	scroll(screen->row,screen->bmargin,count);
	screen->wrap_next = 0;
	cursor();
}

/*  Insert count blank lines at the current position and scroll the lower lines
 *  down.
 */
void
scr_insert_lines(count)
int count;
{
	if (screen->row > screen->bmargin)
		return;
	if (count > screen->bmargin - screen->row + 1)
		count = screen->bmargin - screen->row + 1;

	home_screen();
	cursor();
	while (count > MAX_SCROLL) {
		scroll(screen->row,screen->bmargin,-MAX_SCROLL);
		count -= MAX_SCROLL;
	}
	scroll(screen->row,screen->bmargin,-count);
	screen->wrap_next = 0;
	cursor();
}

/*  Delete count characters from the current position.
 */
void
scr_delete_characters(count)
int count;
{
	int x1, x2, y, width, i;
	unsigned char *r, *s;

	if (count > cwidth - screen->col)
		count = cwidth - screen->col;
	if (count <= 0)
		return;

	home_screen();
	cursor();
	check_selection(screen->row,screen->row);
	s = screen->text[screen->row];
	r = screen->rend[screen->row];
	for (i = screen->col + count; i < cwidth; i++) {
		s[i - count] = s[i];
		r[i - count] = r[i];
	}
	memset(s + cwidth - count,0,count);
	memset(r + cwidth - count,0,count);
	y = MARGIN + screen->row * fheight;
	x2 = MARGIN + screen->col * fwidth;
	x1 = x2 + count * fwidth;
	width = (cwidth - count - screen->col) * fwidth;
	if (width > 0) {
		omshell_Terminal_Copy_Area(x1, y, width, fheight, x2, y);
	}
	x1 = x2 + width;
	width = count * fwidth;
	omshell_Terminal_Clear_Area(x1, y, width, fheight);
	screen->wrap_next = 0;
	cursor();
}

/*  Insert count spaces from the current position.
 */
void
scr_insert_characters(count)
int count;
{
	int x1, x2, y, width, i;
	unsigned char *r, *s;

	if (count > cwidth - screen->col)
		count = cwidth - screen->col;
	if (count <= 0)
		return;

	home_screen();
	cursor();
	check_selection(screen->row,screen->row);
	s = screen->text[screen->row];
	r = screen->rend[screen->row];
	for (i = cwidth - 1; i >= screen->col + count; i--) {
		s[i] = s[i - count];
		r[i] = r[i - count];
	}
	memset(s + screen->col,0,count);
	memset(r + screen->col,0,count);
	y = MARGIN + screen->row * fheight;
	x1 = MARGIN + screen->col * fwidth;
	x2 = x1 + count * fwidth;
	width = (cwidth - count - screen->col) * fwidth;
	if (width > 0) {
		omshell_Terminal_Copy_Area(x1, y, width, fheight, x2, y);
	}
	x1 = MARGIN + screen->col * fwidth;
	width = count * fwidth;
	omshell_Terminal_Clear_Area(x1, y, width, fheight);
	screen->wrap_next = 0;
	cursor();
}

/*  Tab to the next tab_stop.
 */
void
scr_tab()
{
	home_screen();
	if (screen->col == cwidth - 1)
		return;
	cursor();
	check_selection(screen->row,screen->row);
	if (screen->text[screen->row][screen->col] == 0)
		screen->text[screen->row][screen->col] = '\t';
	screen->col++;
	while (screen->col % 8 != 0 && screen->col < cwidth - 1)
		screen->col++;
	cursor();
}

/*  Attempt to set the top ans bottom scroll margins.
 */
void
scr_set_margins(top,bottom)
int top, bottom;
{
	if (top < 0)
		top = 0;
	if (bottom >= cheight)
		bottom = cheight - 1;
	if (top > bottom)
		return;

	screen->tmargin = top;
	screen->bmargin = bottom;
	scr_move(0,0,0);
}

/*  Set or unset automatic wrapping.
 */
void
scr_set_wrap(mode)
int mode;
{
	screen->wrap = mode == HIGH;
}

/*  Set or unset margin origin mode.
 */
void
scr_set_decom(mode)
int mode;
{
	screen->decom = mode == HIGH;
	scr_move(0,0,0);
}

/*  Set or unset automatic insert mode.
 */
void
scr_set_insert(mode)
int mode;
{
	screen->insert = mode == HIGH;
}

/*  Fill the screen with 'E's - useful for testing.
 */
void
scr_efill()
{
	int x, y;

	for (y = 0; y < cheight; y++)
		for (x = 0; x < cwidth; x++) {
			screen->text[y][x] = 'E';
			screen->rend[y][x] = 0;
		}
	home_screen();
	check_selection(0,cheight - 1);
	refresh(0,cheight - 1,0,cwidth - 1);
	cursor();
}

/*  Move the display so that line represented by scrollbar value y is at the top
 *  of the screen.
 */
void
scr_move_to(y)
int y;
{
	int n, lnum;

	y = pheight - 1 - y;
	lnum = y * (cheight + sline_top - 1) / (pheight - 1);
	n = lnum - cheight + 1;
	change_offset(n);
}

/*  Move the display by a distance represented by the value.
 */
void
scr_move_by(y)
int y;
{
	int n;

	if (y >= 0){
		y = (y - MARGIN) / fheight;
		n = offset - y;
	} else {
		y = (-y - MARGIN) / fheight;
		n = offset + y;
	}
	change_offset(n);
}

/*  Send the name of the current display to the command.
 */
void
scr_report_display()
{
	char *dname="0";
	struct utsname ut;

	(void)uname(&ut);

	if (strncmp(dname, "unix:", 5) == 0)
		cprintf("%s%s\r",ut.nodename,dname + 4);
	else if (dname[0] == ':')
		cprintf("%s%s\r",ut.nodename,dname);
	else
		cprintf("%s\r",dname);
}

/*  Report the current cursor position.
 */
void
scr_report_position()
{
	cprintf("\033[%d;%dR",screen->row + 1,screen->col + 1);
}


/*  Change the value of the scrolled screen offset and repaint the screen
 */
static void
change_offset(n)
int n;
{
	int y1, y2, height, d;

	if (n > sline_top)
		n = sline_top;
	if (n < 0)
		n = 0;
	if (n == offset)
		return;
	cursor();
	d = n - offset;
	offset = n;
	if (d > 0 && d < cheight) {
		/*  Text has moved down by less than a screen so raster
		 *  the lines that did not move off.
		 */
		y1 = MARGIN;
		y2 = y1 + d * fheight;
		height = (cheight - d) * fheight;
		omshell_Terminal_Copy_Area(0, y1, pwidth, height, 0, y2);
		refresh(0,d - 1,0,cwidth - 1);
	} else if (d < 0 && -d < cheight) {
		/*  Text has moved down by less than a screen.
		 */
		d = -d;
		y2 = MARGIN;
		y1 = y2 + d * fheight;
		height = (cheight - d) * fheight;
		omshell_Terminal_Copy_Area(0, y1, pwidth, height, 0, y2);
		refresh(cheight - d,cheight - 1,0,cwidth - 1);
	} else
		refresh(0,cheight - 1,0,cwidth - 1);
	cursor();
}

/*  Paint the text using the rendition value at the screen position.
 */
static void
paint_rval_text(str,rval,len,x,y)
unsigned char *str;
int rval;
int len, x, y;
{
	unsigned char *buf;
	int overstrike;
	int i;

	int b=0,inverso=0;	/* bold and inverso for omshell.c */

	if (rval & RS_RVID) {
		inverso=1;
	}
	overstrike = 0;
	if (rval & RS_BOLD) {
		b=1;	
		/* THe boldfont->ascent and maintfont->ascent are hardcoded BAD ! */
		if (0) {
			y +=  11;/* THe boldfont->ascent and maintfont->ascent are hardcoded BAD ! */
		} else {
			overstrike = 1;
			y +=  11;/* THe boldfont->ascent and maintfont->ascent are hardcoded BAD ! */
		}
	} else
		y +=  11;/* THe boldfont->ascent and maintfont->ascent are hardcoded BAD ! */

	if ((rval & CS_STYLE) == CS_USASCII) {
		omshell_TerminalDrawImageString(x,y,str,len, b, inverso, 0);
		if (overstrike) {
			omshell_TerminalDrawImageString(x + 1,y,str,len, b, inverso, 0);
		}
	} else {
		buf = (unsigned char *)cmalloc(len);
		if ((rval & CS_STYLE) == CS_UKASCII) {
			for (i = 0; i < len; i++)
				buf[i] = (str[i] == '#') ? POUND : str[i];
		} else if ((rval & CS_STYLE) == CS_SPECIAL) {
			for (i = 0; i < len; i++)
				if (str[i] >= 0140 && str[i] <= 0176)
					buf[i] = (str[i] & 037) + 1;
				else
					buf[i] = str[i];
		}

		/* because now we need to print extended chars, we use str and 1 for
 		   the last argument */
		omshell_TerminalDrawImageString(x,y,str,len, b, inverso, 1);
		if (overstrike) {
			omshell_TerminalDrawImageString(x + 1,y,str,len, b, inverso, 1);
		}
		free((void *)buf);
	}

	y++;
	if (rval & RS_ULINE) {
	}

	if (rval & RS_RVID) {
	}
	if (rval & RS_BOLD) {
	}
}

/* Display the string using the rendition vector at the screen coordinates
 */
static void
paint_rvec_text(str,rvec,len,x,y)
unsigned char *str, *rvec;
int len, x, y;
{
	int i;

	if (rvec == NULL) {
		paint_rval_text(str,0,len,x,y);
		return;
	}
	while (len > 0) {
		for (i = 0; i < len; i++)
			if (rvec[i] != rvec[0])
				break;
		paint_rval_text(str,rvec[0],i,x,y);
		str += i;
		rvec += i;
		len -= i;
		x += i * fwidth;
	}
}

/* Repaint the box delimited by row1 to row2 and col1 to col2 of the displayed
 * screen from the backup screen.
 */
static void
refresh(row1,row2,col1,col2)
int row1, row2, col1, col2;
{
	unsigned char *s, *str, *r;
	int x, y, x1, y1, x2, width, i, m;
	struct slinest *sl;

	str = (unsigned char *)cmalloc(cwidth + 1);
	y = row1;
	x1 = MARGIN + col1 * fwidth;
	y1 = MARGIN + row1 * fheight;

	/*  First do any 'scrolled off' lines that are visible.
	 */
	for (i = offset - 1 - row1; y <= row2 && i >= 0; y++, i--) {
		sl = sline[i];
		m = (col2 + 1) < sl->sl_length ? (col2 + 1) : sl->sl_length;
		s = sl->sl_text;
		m -= col1;
		for (x = 0; x < m; x++)
			str[x] = s[x + col1] < ' ' ? ' ' : s[x + col1];
		r = sl->sl_rend == NULL ? NULL : sl->sl_rend + col1;
		paint_rvec_text(str,r,m,x1,y1);
		x2 = x1 + m * fwidth;
		width = (col2 - col1 + 1 - m) * fwidth;
		if (width > 0) {
			omshell_Terminal_Clear_Area(x2, y1, width, fheight);
		}
		y1 += fheight;
	}


	/*  Now do the remainder from the current screen
	 */
	i = offset > row1 ? 0 : row1 - offset;
	for (; y <= row2; y++, i++) {
		s = screen->text[i];
		m = col1 - 1;
		for (x = col1; x <= col2; x++)
			if (s[x] < ' ')
				str[x - col1] = ' ';
			else {
				str[x - col1] = s[x];
				m = x;
			}
		m++;
		m -= col1;
		r = screen->rend[i][cwidth] == 0 ? NULL : screen->rend[i] + col1;
		paint_rvec_text(str,r,m,x1,y1);
		x2 = x1 + m * fwidth;
		width = (col2 - col1 + 1 - m) * fwidth;
		if (width > 0) {
			omshell_Terminal_Clear_Area(x2, y1, width, fheight);
		}
		y1 += fheight;
	}
	free(str);
	show_selection(row1,row2,col1,col2);
}

/*  Paint any part of the selection that is between rows row1 and row2 inclusive
 *  and between cols col1 and col2 inclusive.
 */
static void
show_selection(row1,row2,col1,col2)
int row1, row2, col1, col2;
{
	int r1, c1, r2, c2, sr, sc, er, ec;
	int x1, x2, y, row;

	if (selend1.se_type == NOSEL || selend2.se_type == NOSEL)
		return;
	if (selcmp(&selend1,&selend2) == 0)
		return;
	selend_to_rc(&r1,&c1,&selend1);
	selend_to_rc(&r2,&c2,&selend2);
	
	col2++;

	/*  Obtain initial and final endpoints for the selection.
	 */
	if (r1 < r2 || (r1 == r2 && c1 <= c2)) {
		sr = r1;
		sc = c1;
		er = r2;
		ec = c2;
	} else {
		sr = r2;
		sc = c2;
		er = r1;
		ec = c1;
	}
	if (sr < row1) {
		sr = row1;
		sc = col1;
	}
	if (sc < col1)
		sc = col1;
	if (er > row2) {
		er = row2;
		ec = col2;
	}
	if (ec > col2)
		ec = col2;

	if (sr > er)
		return;
	
	/*  Paint in the reverse video.
	 */
	for (row = sr; row <= er; row++) {
		y = MARGIN + row * fheight;
		x1 = MARGIN + (row == sr ? sc : col1) * fwidth;
		x2 = MARGIN + ((row == er) ? ec : col2) * fwidth;
		if (x2 > x1) {
		}
	}
}

/*  Scroll count lines from row1 to row2 inclusive.  row1 should be <= row1.
 *  scrolling is up for a +ve count and down for a -ve count.
 *  count is limited to a maximum of MAX_SCROLL lines.
 */
static void
scroll(row1,row2,count)
int row1, row2, count;
{
	int y1, y2, height, i, j;
	unsigned char *save[MAX_SCROLL], *rend[MAX_SCROLL];
	struct slinest *sl;
	unsigned char *r, *s;

	row2++;

	if (row1 == 0 && screen == &screen1 && count > 0) {

		/*  Save lines that scroll of the top of the screen.
		 */
		for (i = 1; i <= count; i++) {
			if ((sl = sline[sline_max - i]) != NULL) {
				free(sl->sl_text);
				if (sl->sl_rend != NULL)
					free(sl->sl_rend);
				free((void *)sl);
			}
			if (selend1.se_type == SAVED && selend1.se_index == sline_max-i) {
				show_selection(0,cheight - 1,0,cwidth - 1);
				selend1.se_type = NOSEL;
			}
			if (selend2.se_type == SAVED && selend2.se_index == sline_max-i) {
				show_selection(0,cheight - 1,0,cwidth - 1);
				selend2.se_type = NOSEL;
			}
		}
		for (i = sline_max - count - 1; i >= 0; i--) {
			sline[i + count] = sline[i];
			if (selend1.se_type == SAVED && selend1.se_index == i)
				selend1.se_index = i + count;
			if (selend2.se_type == SAVED && selend2.se_index == i)
				selend2.se_index = i + count;
		}
		for (i = 0; i < count; i++) {
			s = screen->text[i];
			r = screen->rend[i];
			for (j = cwidth - 1; j >= 0 && s[j] == 0; j--)
				;
			j++;
			sl = (struct slinest *)cmalloc(sizeof(struct slinest));
			sl->sl_text = (unsigned char *)cmalloc(j + 1);
			memcpy(sl->sl_text,s,j);
			sl->sl_text[j] = s[cwidth];
			if (r[cwidth] == 0)
				sl->sl_rend = NULL;
			else {
				sl->sl_rend = (unsigned char *)cmalloc(j + 1);
				memcpy(sl->sl_rend,r,j);
			}
			sl->sl_length = j;
			sline[count - i - 1] = sl;
			if (selend1.se_type == SCREEN && selend1.se_index == i) {
				selend1.se_type = SAVED;
				selend1.se_index = count - i - 1;
			}
			if (selend2.se_type == SCREEN && selend2.se_index == i) {
				selend2.se_type = SAVED;
				selend2.se_index = count - i - 1;
			}
		}
		sline_top += count;
		if (sline_top > sline_max)
			sline_top = sline_max;
	}

	if (count > 0) {
		j = row1;
		for (i = 0; i < count; i++, j++) {
			save[i] = screen->text[j];
			rend[i] = screen->rend[j];
			if (selend1.se_type == SCREEN && selend1.se_index == j) {
				show_selection(0,cheight - 1,0,cwidth - 1);
				selend1.se_type = NOSEL;
			}
			if (selend2.se_type == SCREEN && selend2.se_index == j) {
				show_selection(0,cheight - 1,0,cwidth - 1);
				selend2.se_type = NOSEL;
			}
		}
		for (; j < row2; j++) {
			screen->text[j - count] = screen->text[j];
			screen->rend[j - count] = screen->rend[j];
			if (selend1.se_type == SCREEN && selend1.se_index == j)
				selend1.se_index = j - count;
			if (selend2.se_type == SCREEN && selend2.se_index == j)
				selend2.se_index = j - count;
		}
		for (i = 0; i < count; i++) {
			memset(save[i],0,cwidth + 1);
			screen->text[row2 - i - 1] = save[i];
			memset(rend[i],0,cwidth + 1);
			screen->rend[row2 - i - 1] = rend[i];
		}
		if (count < row2 - row1) {
			y2 = MARGIN + row1 * fheight;
			y1 = y2 + count * fheight;
			height = (row2 - row1 - count) * fheight;
			omshell_Terminal_Copy_Area(0, y1, pwidth, height, 0, y2);
		}
		height = count * fheight;
		y1 = MARGIN + (row2 - count) * fheight;
		omshell_Terminal_Clear_Area(0, y1, pwidth, height);
	}
	if (count < 0) {
		count = -count;
		j = row2 - 1;
		for (i = 0; i < count; i++, j--) {
			save[i] = screen->text[j];
			rend[i] = screen->rend[j];
			if (selend1.se_type == SCREEN && selend1.se_index == j) {
				show_selection(0,cheight - 1,0,cwidth - 1);
				selend1.se_type = NOSEL;
			}
			if (selend2.se_type == SCREEN && selend2.se_index == j) {
				show_selection(0,cheight - 1,0,cwidth - 1);
				selend2.se_type = NOSEL;
			}
		}
		for (; j >= row1; j--) {
			screen->text[j + count] = screen->text[j];
			screen->rend[j + count] = screen->rend[j];
			if (selend1.se_type == SCREEN && selend1.se_index == j)
				selend1.se_index = j + count;
			if (selend2.se_type == SCREEN && selend2.se_index == j)
				selend2.se_index = j + count;
		}
		for (i = 0; i < count; i++) {
			memset(save[i],0,cwidth + 1);
			screen->text[row1 + i] = save[i];
			memset(rend[i],0,cwidth + 1);
			screen->rend[row1 + i] = rend[i];
		}
		if (count < row2 - row1) {
			y1 = MARGIN + row1 * fheight;
			y2 = y1 + count * fheight;
			height = (row2 - row1 - count) * fheight;
			omshell_Terminal_Copy_Area(0, y1, pwidth, height, 0, y2);
		}
		height = count * fheight;
		y1 = MARGIN + row1 * fheight;
		omshell_Terminal_Clear_Area(0, y1, pwidth, height);
	}
}

/*  Scroll screen1 up by count lines saving lines as needed.  This is used
 *  after the screen size is reduced.
 */
static void
scroll1(count)
int count;
{
	int i, j, n;
	unsigned char *save[MAX_SCROLL], *rend[MAX_SCROLL];
	struct slinest *sl;
	unsigned char *r, *s;

	while (count > 0) {

		/*  If count is greater than MAX_SCROLL then scroll in
		 *  installements.
		 */
		n = count > MAX_SCROLL ? MAX_SCROLL : count;
		count -= n;

		/*  Save lines that scroll of the top of the screen.
		 */
		for (i = 1; i <= n; i++) {
			if ((sl = sline[sline_max - i]) != NULL) {
				free(sl->sl_text);
				if (sl->sl_rend != NULL)
					free(sl->sl_rend);
				free((void *)sl);
			}
		}
		for (i = sline_max - n - 1; i >= 0; i--) {
			sline[i + n] = sline[i];
		}
		for (i = 0; i < n; i++) {
			s = screen1.text[i];
			r = screen1.rend[i];
			for (j = cwidth - 1; j >= 0 && s[j] == 0; j--)
				;
			j++;
			sl = (struct slinest *)cmalloc(sizeof(struct slinest));
			sl->sl_text = (unsigned char *)cmalloc(j + 1);
			memcpy(sl->sl_text,s,j);
			sl->sl_text[j] = s[cwidth];
			if (r[cwidth] == 0)
				sl->sl_rend = NULL;
			else {
				sl->sl_rend = (unsigned char *)cmalloc(j + 1);
				memcpy(sl->sl_rend,r,j);
			}
			sl->sl_length = j;
			sline[n - i - 1] = sl;
		}
		sline_top += n;
		if (sline_top > sline_max)
			sline_top = sline_max;

		j = 0;
		for (i = 0; i < n; i++, j++) {
			save[i] = screen1.text[j];
			rend[i] = screen1.rend[j];
		}
		for (; j < cheight; j++) {
			screen1.text[j - n] = screen1.text[j];
			screen1.rend[j - n] = screen1.rend[j];
		}
		for (i = 0; i < n; i++) {
			memset(save[i],0,cwidth + 1);
			screen1.text[cheight - i - 1] = save[i];
			memset(rend[i],0,cwidth + 1);
			screen1.rend[cheight - i - 1] = rend[i];
		}
	}
}

/*  Reposition the scrolled text so that the scrollbar is at the bottom.
 */
static void
home_screen()
{
	if (offset > 0) {
		offset = 0;
		refresh(0,cheight - 1,0,cwidth - 1);
		cursor();
	}
}

/*  Draw the cursor at the current position.
 */
static void
cursor()
{
	int x, y;

/* solo comento para hacer una prueba */
/*	if (offset > 0)
 *		return;
 */

	/* CORREGIR PARA QUE SE VEA EL CURSOR */
	x = MARGIN + fwidth * screen->col;
	y = MARGIN + fheight * screen->row;
	omshell_TerminalCursor(x,y,fwidth,fheight);
	/* fin de CORREGIR PARA QUE SE VEA EL CURSOR */

	if (focus == 0) {
	}
}



/*  Convert the selection into a row and column.
 */
static void
selend_to_rc(rowp,colp,se)
int *rowp, *colp;
struct selst *se;
{
	if (se->se_type == NOSEL)
		return;

	*colp = se->se_col;
	if (se->se_type == SCREEN)
		*rowp = se->se_index + offset;
	else
		*rowp = offset - se->se_index - 1;
}



/*  Compare the two selections and return -1, 0 or 1 depending on
 *  whether se2 is after, equal to or before se1.
 */
static int
selcmp(se1,se2)
struct selst *se1, *se2;
{
	if (se1->se_type == SAVED && se2->se_type == SAVED) {
		if (se1->se_index > se2->se_index)
			return(-1);
		if (se1->se_index < se2->se_index)
			return(1);
		if (se1->se_col < se2->se_col)
			return(-1);
		if (se2->se_col < se1->se_col)
			return(1);
		return(0);
	}
	if (se1->se_type == SCREEN && se2->se_type == SCREEN) {
		if (se1->se_index < se2->se_index)
			return(-1);
		if (se1->se_index > se2->se_index)
			return(1);
		if (se1->se_col < se2->se_col)
			return(-1);
		if (se2->se_col < se1->se_col)
			return(1);
		return(0);
	}
	if (se1->se_type == SAVED)
		return(-1);
	return(1);
}



/*  Determine if the current selection overlaps row1-row2 and if it does then
 *  remove it from the screen.
 */
static void
check_selection(row1,row2)
int row1,row2;
{
	int r1, r2, x;

	if (selend1.se_type == NOSEL || selend2.se_type == NOSEL)
		return;

	r1 = selend1.se_type == SCREEN ? selend1.se_index : -1;
	r2 = selend2.se_type == SCREEN ? selend2.se_index : -1;
	if (r1 > r2) {
		x = r1;
		r1 = r2;
		r2 = x;
	}
	if (row2 < r1 || row1 > r2)
		return;
	show_selection(0,cheight - 1,0,cwidth - 1);
	selend2.se_type = NOSEL;
}


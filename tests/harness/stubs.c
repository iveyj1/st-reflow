/* Headless X boundary. Clipboard strings have the same ownership as x.c. */
#include <stdlib.h>
#include <wchar.h>
#include "../../st.h"
#include "../../win.h"

char *utmp, *scroll;
char *stty_args = "", *vtiden = "\033[?6c", *termname = "st-256color";
wchar_t *worddelimiters = L" ";
int allowaltscreen = 1, allowwindowops = 1, reflowactive = 1;
unsigned int tabspaces = 8, defaultfg = 7, defaultbg = 0, defaultcs = 7;
const int boxdraw = 0, boxdraw_bold = 0, boxdraw_braille = 0;
char *lastselection;
int clipboardcopies;
void harnessfree(void *);

int
xconnectionfd(void)
{
	return -1;
}

void
xbell(void)
{
}

void
xclipcopy(void)
{
	clipboardcopies++;
}

void
xdrawcopycursor(int x, int y, int v)
{
	(void)x; (void)y; (void)v;
}

void
xdrawcopylabel(int x, int y, const char *text)
{
	(void)x; (void)y; (void)text;
}

void
xdrawcopynumber(int y, int d, int r, int c)
{
	(void)y; (void)d; (void)r; (void)c;
}

void
xdrawcursor(int x, int y, Glyph g, int ox, int oy, Glyph og)
{
	(void)x; (void)y; (void)g; (void)ox; (void)oy; (void)og;
}

void
xdrawline(Line l, int x, int y, int n)
{
	(void)l; (void)x; (void)y; (void)n;
}

void
xfinishdraw(void)
{
}

void
xloadcols(void)
{
}

int
xsetcolorname(int n, const char *s)
{
	(void)n; (void)s;
	return 0;
}

int
xgetcolor(int n, unsigned char *r, unsigned char *g, unsigned char *b)
{
	(void)n;
	*r = *g = *b = 0;
	return 0;
}

void
xseticontitle(char *s)
{
	(void)s;
}

void
xsettitle(char *s)
{
	(void)s;
}

int
xsetcursor(int n)
{
	(void)n;
	return 0;
}

void
xsetmode(int set, unsigned int flags)
{
	(void)set; (void)flags;
}

void
xsetpointermotion(int set)
{
	(void)set;
}

void
xsetsel(char *s)
{
	harnessfree(lastselection);
	lastselection = s;
}

int
xstartdraw(void)
{
	return 1;
}

void
xximspot(int x, int y)
{
	(void)x; (void)y;
}

int
isboxdraw(Rune u)
{
	(void)u;
	return 0;
}

ushort
boxdrawindex(const Glyph *g)
{
	(void)g;
	return 0;
}

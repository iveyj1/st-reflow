/* Internal read-only terminal geometry and controlled view/selection operations.
 * Not included by x.c. Include st.h first. */
#ifndef TERM_H
#define TERM_H

typedef struct {
	long long base, first, last, top;
	int rows, cols, alt, scrolled;
	CPos cursor;
} TView;
TView termview(void);
const Glyph *termline(long long);
int termlinelen(long long);
int termwrapped(long long);
void termshow(long long);
void termselstart(CPos, int);
void termselextend(CPos, int, int);
int termrange(CRange *);
void termdirty(void);
#endif

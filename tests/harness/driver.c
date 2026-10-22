/* Include the real terminal core; no X server or X libraries required. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>

typedef union {
	size_t size;
	long double alignment;
	void *pointer;
} Allocation;
static size_t allocated;

static void *
harnessmalloc(size_t n)
{
	Allocation *p = malloc(sizeof(*p) + n);
	assert(p);
	p->size = n;
	allocated += n;
	return p + 1;
}

void
harnessfree(void *ptr)
{
	Allocation *p;

	if (!ptr)
		return;
	p = (Allocation *)ptr - 1;
	allocated -= p->size;
	free(p);
}

static void *
harnessrealloc(void *ptr, size_t n)
{
	Allocation *p;
	size_t old;

	if (!ptr)
		return harnessmalloc(n);
	p = (Allocation *)ptr - 1;
	old = p->size;
	p = realloc(p, sizeof(*p) + n);
	assert(p);
	p->size = n;
	allocated = allocated - old + n;
	return p + 1;
}

#define malloc harnessmalloc
#define realloc harnessrealloc
#define free harnessfree
#include "../../st.c"
#undef malloc
#undef realloc
#undef free
#include "../../copymode.c"
#include <X11/keysym.h>
#include "copykeys.h"

#include "../../paste.h"
extern char *lastselection;
extern int clipboardcopies;

static void
feed(const char *s)
{
	assert(twrite(s, strlen(s), 0) == (int)strlen(s));
}

static void
rowtext(int viewrow, char *buf, size_t n)
{
	char encoded[UTF_SIZ];
	int x, len;
	size_t used = 0;
	Line line = TLINE(viewrow);

	assert(n);
	for (x = 0; x < tlinelen(line); x++) {
		if (line[x].mode & ATTR_WDUMMY)
			continue;
		len = utf8encode(line[x].u, encoded);
		if (used + len >= n)
			break;
		memcpy(buf + used, encoded, len);
		used += len;
	}
	buf[used] = '\0';
}

static void
resize(int col, int row)
{
	tresize(col, row);
}

static int
copyrow(void)
{
	return copyview();
}

static void
testcopy(void)
{
	Arg a = {.i = 3};
	int action, visual, scr, copies;
	char buf[128];

	feed("one\r\ntwo\r\nthree\r\nfour\r\nfive\r\nsix\r\nseven\r\neight\r\nnine\r\nten");
	kscrollup(&a);
	for (action = COPY_YANK; action <= COPY_YANK_CLEAN; action++) {
		for (visual = 0; visual <= 3; visual++) {
			scr = term.scr;
			copies = clipboardcopies;
			copymode(NULL);
			rowtext(copyrow(), buf, sizeof buf);
			assert(buf[0]);
			if (visual)
				copymodeaction(visual == 1 ? COPY_VISUAL :
				    visual == 2 ? COPY_VISUALLINE : COPY_VISUALRECT, 1);
			copymodeaction(action, 1);
			assert(!copymodeactive() && !copyvisual && sel.ob.x == -1);
			assert(clipboardcopies == copies + 1 && lastselection);
			assert(term.scr == scr);
			/* An invalidated visual selection yields no text. */
			copymode(NULL);
			copymodeaction(COPY_VISUAL, 1);
			selclear();
			copymodeaction(action, 1);
			assert(copymodeactive() && clipboardcopies == copies + 1);
			assert(term.scr == scr);
			copymodeaction(COPY_EXIT, 1);
		}
	}
}

static void
teststream(void)
{
	char before[128], after[128];
	int back, visual, i;
	long long position;
	Arg a = {.i = 3};

	for (back = 0; back < 2; back++) {
		for (visual = 0; visual < 2; visual++) {
			feed("\033[3J\033[2J\033[H");
			for (i = 0; i < 20; i++) {
				snprintf(before, sizeof before, "row %03d\r\n", i);
				feed(before);
			}
			if (back)
				kscrollup(&a);
			copymode(NULL);
			copymodeaction(COPY_UP, 2);
			copymodeaction(COPY_HOME, 1);
			rowtext(copyrow(), before, sizeof before);
			position = copyy;
			{
				int scr = term.scr;
				feed("\033[6n");
				assert(term.scr == scr);
				ttyinput("ignored paste", 13, 1);
				assert(term.scr == scr);
			}
			if (visual) {
				copymodeaction(COPY_VISUAL, 1);
				copymodeaction(COPY_END, 1);
			}
			feed("new 1\r\nnew 2\r\nnew 3\r\n");
			rowtext(copyrow(), after, sizeof after);
			assert(copyy == position && !strcmp(before, after));
			copymodeaction(COPY_YANK_CLEAN, 1);
			assert(!strncmp(lastselection, before, strlen(before)));
		}
	}
}

static void
testoverwrite(void)
{
	Arg a = {.i = 3};

	feed("\033[H");
	kscrollup(&a);
	assert(term.scr > 0);
	selstart(0, term.scr, 0);
	selextend(1, term.scr, SEL_REGULAR, 0);
	selextend(1, term.scr, SEL_REGULAR, 1);
	assert(selected(0, term.scr));
	feed("Z");
	assert(sel.ob.x == -1);
}

static void
testpaste(void)
{
	char *out;
	const unsigned char text[] = "a\r\nb\nc\033[201~d\033[200~e";
	size_t n;

	n = normalizepaste(text, sizeof text - 1, &out, 1);
	assert(n == 7 && !strcmp(out, "a\rb\rcde"));
	harnessfree(out);
	normalizepaste(text, sizeof text - 1, &out, 0);
	assert(strstr(out, "\033[201~"));
	harnessfree(out);
}

static void
testmouse(void)
{
	int i;

	feed("\033[3J\033[2J\033[Habcdef\r\nghijkl");
	copymode(NULL);
	copymodemouse(1, 0, 0);
	assert(copyx == 1 && copyview() == 0 && !copyvisual);
	copymodemouse(3, 0, 1);
	assert(copyvisual == COPY_CHAR);
	copymodeaction(COPY_YANK, 1);
	assert(!strcmp(lastselection, "bcd"));
	copymode(NULL);
	copymodemouse(0, 0, 0);
	copymodeaction(COPY_VISUAL, 1);
	for (i = 0; i < HISTSIZE + 20; i++)
		feed("more\r\n");
	assert(!copyvisual);
	copymodeaction(COPY_YANK, 1);
	assert(!copymodeactive());
}

static void
testreflow(void)
{
	char line[118];
	size_t baseline;
	int i, cycle;

	resize(120, 24);
	memset(line, 'x', 115);
	memcpy(line + 115, "\r\n", 3);
	for (cycle = 0; cycle < 4; cycle++) {
		for (i = 0; i < HISTSIZE + 50; i++)
			feed(line);
		baseline = allocated;
		resize(40, 24);
		resize(120, 24);
		assert(allocated <= baseline + 4096);
	}
	/* A very long active logical line exercises cursor-above-view cleanup. */
	for (i = 0; i < 30; i++)
		feed("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz");
	feed("\033[20A");
	baseline = allocated;
	resize(8, 4);
	resize(120, 24);
	assert(allocated <= baseline + 4096);
}

static void
keysequence(const char *s)
{
	Rune rune;
	size_t n;

	while (*s && (n = utf8decode(s, &rune, strlen(s)))) {
		copymodekey(rune, rune, 0, 0, copykeys, sizeof copykeys / sizeof *copykeys);
		s += n;
	}
}

static void
testflash(void)
{
	int alt, x, y, i;
	CPos target;
	char label[8];

	for (alt = 0; alt < 2; alt++) {
		if (alt)
			feed("\033[?1049h");
		kscrolldown(&(Arg){.i = HISTSIZE});
		feed("\033[3J\033[2J\033[Halpha apple ash\r\nbanana");
		copymode(NULL);
		copymodemouse(0, 0, 0);
		keysequence(" sa");
		assert(flashstate == 3 && flashcount > 1);
		assert(!strcmp(copyflashlabel(0, &x, &y), "a") && x == 4 && y == 0);
		assert(!copymodenumber(1));
		keysequence("a");
		assert(copyx == 4 && !flashstate && copyactive);
		copymodemouse(0, 0, 0);
		keysequence("v saa");
		{
			char *text = getsel();
			assert(text && !strcmp(text, "alpha"));
			harnessfree(text);
		}
		keysequence("v");
		keysequence("v sa");
		target = flashtargets[1].pos;
		strcpy(label, flashtargets[1].label);
		keysequence(label);
		assert(copyx == target.x && copyy == target.y && copyvisual);
		keysequence(" sa\033");
		assert(!flashstate && copyactive);
		keysequence(" sa");
		feed("!");
		assert(!flashstate && copyactive);
		keysequence(" sz");
		assert(!flashstate && copyactive); /* no match */
		keysequence("v se");
		assert(!flashstate && copyx == 10); /* single match */
		keysequence(" \033");
		assert(!flashstate && copyactive);
		copymodeaction(COPY_EXIT, 1);
		feed("\033[2J\033[H");
		for (i = 0; i < 8; i++)
			feed("a a a a a a a a a a\r\n");
		copymode(NULL);
		copymodemouse(0, 0, 0);
		keysequence(" sa");
		assert(flashcount > 10);
		target = flashtargets[10].pos;
		strcpy(label, flashtargets[10].label);
		assert(strlen(label) == 2);
		keysequence(label);
		assert(!flashstate && copyx == target.x && copyy == target.y);
		copymodeaction(COPY_EXIT, 1);
		feed("\033[2J\033[H界 x 界 x 界");
		copymode(NULL);
		copymodemouse(0, 0, 0);
		keysequence(" s界");
		assert(flashstate == 3 && flashcount == 2);
		keysequence("界");
		assert(!flashstate && copyx == 5);
		keysequence(" sx\033");
		assert(copyactive && !flashstate);
		keysequence(" sx");
		feed(alt ? "\033[?1049l" : "\033[?1049h");
		assert(!copyactive && !flashstate);
		if (!alt)
			feed("\033[?1049l");
	}
}

static void
testmotions(void)
{
	CPos from, saved;
	CMotion m;
	int action, scr, visual;
	char *before;

	feed("\033[3J\033[2J\033[Hone two three four\r\nfive six\r\nseven");
	copymode(NULL);
	copymodemouse(0, 0, 0);
	from = (CPos){copyy, copyx};
	copymodeaction(COPY_VISUAL, 1);
	before = getsel();
	scr = term.scr;
	visual = copyvisual;
	saved = (CPos){copyy, copyx};
	for (action = COPY_LEFT; action <= COPY_GOTO_LINE; action++) {
		m = copymotion(from, action, 2, 0);
		assert(m.ok && term.scr == scr && copyvisual == visual);
		assert(copyx == saved.x && copyy == saved.y);
	}
	{
		char *after = getsel();
		assert(!strcmp(before, after));
		harnessfree(before);
		harnessfree(after);
	}
	m = copymotion(from, COPY_WORD_FORWARD, 2, 0);
	assert(m.to.x == 8 && m.kind == M_EXCL);
	assert(copyoperator(COPY_YANK, (CRange){from, m.to, m.kind}));
	assert(!strcmp(lastselection, "one two "));
	copymodeaction(COPY_EXIT, 1);
}

static void
testparser(void)
{
	int i;
	long long first;

	feed("\033[3J\033[2J\033[H");
	for (i = 0; i < 20; i++)
		feed("one two three four five\r\n");
	copymode(NULL);
	first = termview().first;
	keysequence("gg5j"); assert(copyy == first + 5);
	keysequence("3w"); assert(copyx == 14);
	keysequence("10gg"); assert(copyy == first + 9 && copyx == 0);
	keysequence("gg"); assert(copyy == first);
	keysequence("G"); assert(copyy == termview().cursor.y && term.scr == 0);
	keysequence("5G"); assert(copyy == first + 4);
	keysequence("2$"); assert(copyy == first + 5 && copyx == 22);
	keysequence("3@j"); assert(copyy == first + 6);
	keysequence("5");
	copymodekey(XK_Shift_L, 0, 0, 1, copykeys, sizeof copykeys / sizeof *copykeys);
	keysequence("G"); assert(copyy == first + 4);
	keysequence("gg3");
	copymodekey(XK_Shift_L, 0, 0, 1, copykeys, sizeof copykeys / sizeof *copykeys);
	keysequence("W"); assert(copyx == 14);
	keysequence("9999999999"); assert(pendingcount == 1000000);
	keysequence("@"); assert(parserstate == CP_IDLE && pendingcount == 0);
	copymodeaction(COPY_EXIT, 1);
}

static void
copyfixture(const char *text, int width)
{
	if (copyactive)
		copymodeaction(COPY_EXIT, 1);
	resize(width, 6);
	feed("\033[2J\033[3J\033[H");
	feed(text);
	copymode(NULL);
	keysequence("gg");
}

static void
expectcopy(const char *text, int width, const char *keys, const char *expected)
{
	int copies = clipboardcopies;

	copyfixture(text, width);
	keysequence(keys);
	if (copyactive || strcmp(lastselection, expected))
		fprintf(stderr, "keys %s: expected <%s>, got <%s>, active %d\n",
		    keys, expected, lastselection, copyactive);
	assert(!copyactive && clipboardcopies == copies + 1);
	assert(!strcmp(lastselection, expected));
}

static void
controlkey(unsigned long key)
{
	copymodekey(key, 0, CM_CTRL, 0, copykeys, sizeof copykeys / sizeof *copykeys);
}

static void
testcommands(void)
{
	const char *words = "one two three four five six seven";
	int copies, numbers, i;
	long long first;

	expectcopy(words, 40, "3yw", "one two three ");
	expectcopy(words, 40, "y3w", "one two three ");
	expectcopy(words, 40, "2y3w", "one two three four five six ");
	expectcopy("one,two three", 40, "yw", "one");
	expectcopy("one,two three", 40, "yW", "one,two ");
	expectcopy("one\r\n  two", 40, "yw", "one");
	expectcopy("one", 40, "yw", "one");
	expectcopy("\r\n\r\n\r\n\r\n\r\none", 40, "5jyw", "one");
	expectcopy("\r\n\r\n\r\n\r\n\r\na b", 40, "5jyw", "a ");
	expectcopy("abc,def,ghi", 40, "yf,", "abc,");
	expectcopy("abc,def,ghi", 40, "yt,", "abc");
	expectcopy("abc,def,ghi", 40, "y2f,", "abc,def,");
	expectcopy("abc,def,ghi", 40, "$yF,", ",ghi");
	expectcopy("abc,def,ghi", 40, "$yT,", "ghi");
	expectcopy("abc,def,ghi", 40, "vt,;y", "abc,def");
	expectcopy("abc,def,ghi", 40, "vf,;y", "abc,def,");
	expectcopy("abc,def,ghi", 40, "$vF,y", ",ghi");
	expectcopy("abc,def,ghi", 40, "$vT,y", "ghi");
	expectcopy("ab界cd界ef", 40, "y2f界", "ab界cd界");
	expectcopy("abcdefghijklmnop\r\nnext", 8, "2lY", "cdefghijklmnop");
	expectcopy("abcdefghijklmnop\r\nnext", 8, "yy", "abcdefghijklmnop\n");
	expectcopy("abcdefghijklmnop\r\nnext", 8, "2Y", "abcdefghijklmnop\nnext");
	expectcopy("first\r\nsecond\r\nthird", 40, "2yy", "first\nsecond\n");
	expectcopy("first\r\nsecond\r\nthird", 40, "y2y", "first\nsecond\n");
	expectcopy("abc\r\n\r\nnext", 40, "2Y", "abc\n");
	expectcopy("abc  \r\n  def  ", 40, "v2$Y", "abc\n  def");
	expectcopy("abc  ", 40, "v$y", "abc  ");
	expectcopy("abc  ", 40, "v$999y", "abc  ");
	copyfixture("abc   def", 6);
	controlkey(XK_y);
	assert(!copyactive && !strcmp(lastselection, "abc   def"));
	copyfixture("abc  \r\n  def  ", 40);
	keysequence("2"); controlkey(XK_y);
	assert(!copyactive && !strcmp(lastselection, "abc\n  def"));
	copyfixture("abc  \r\n  def  ", 40);
	keysequence("2"); controlkey(XK_X);
	assert(!copyactive && !strcmp(lastselection, "abc\n  def"));
	copyfixture("abc  ", 40);
	keysequence("l"); controlkey(XK_C);
	assert(!copyactive && !strcmp(lastselection, "bc  "));
	copyfixture("abc  ", 40);
	keysequence("v$"); controlkey(XK_C);
	assert(!copyactive && !strcmp(lastselection, "abc  "));
	copyfixture("abc  ", 40);
	keysequence("v$"); controlkey(XK_y);
	assert(!copyactive && !strcmp(lastselection, "abc"));

	copyfixture("a,b,c,d", 40);
	keysequence("f,;"); assert(copyx == 3);
	keysequence(","); assert(copyx == 1);
	keysequence(";"); assert(copyx == 3); /* Reverse does not redefine find. */
	keysequence("2;"); assert(copyx == 3); /* Insufficient matches: no movement. */
	keysequence("0t,;"); assert(copyx == 2);
	keysequence(";"); assert(copyx == 4);
	keysequence(","); assert(copyx == 2);
	keysequence(";"); assert(copyx == 4);
	keysequence("$T,;"); assert(copyx == 4);
	keysequence(";"); assert(copyx == 2);
	keysequence(","); assert(copyx == 4);
	keysequence(";"); assert(copyx == 2);

	copyfixture("abc\r\nz", 40);
	copies = clipboardcopies;
	keysequence("yfz");
	assert(copyactive && copyx == 0 && clipboardcopies == copies);
	assert(strstr(copymodestatus(), "not found"));
	keysequence("y\033"); assert(copyactive && !pendingyank);
	keysequence("f\033"); assert(copyactive && parserstate == CP_IDLE);
	keysequence("y@"); assert(copyactive && !pendingyank);
	keysequence("999999y999999w"); assert(!copyactive); /* Saturated multiplication. */
	copyfixture("abcdefghijZ", 8);
	keysequence("fZ"); assert(copyx == 2 && copyy == termview().first + 1);

	copyfixture("one cat\r\ntwo cat\r\nthree cat", 40);
	first = termview().first;
	keysequence("/cat\r"); assert(copyy == first && copyx == 4);
	keysequence("2n"); assert(copyy == first + 2 && copyx == 6);
	keysequence("N"); assert(copyy == first + 1);
	keysequence("n"); assert(copyy == first + 2);
	keysequence("n"); assert(copyy == first && strstr(copymodestatus(), "wrapped"));
	keysequence("?cat\r"); assert(copyy == first + 2);
	keysequence("n"); assert(copyy == first + 1);
	keysequence("N"); assert(copyy == first + 2);
	keysequence("n"); assert(copyy == first + 1);
	keysequence("4n"); assert(copyy == first + 1 && strstr(copymodestatus(), "not found"));
	keysequence("/absent\033n"); assert(copyy == first); /* Cancel keeps previous search. */
	keysequence("/\r"); assert(copyy == first + 1); /* Empty Enter reuses pattern. */
	keysequence("/CAT\r"); assert(copyy == first + 1 && strstr(copymodestatus(), "not found"));
	keysequence("/junk"); controlkey(XK_u);
	keysequence("catx\b\r"); assert(copyy == first + 2);
	numbers = copynumbers; controlkey(XK_n); assert(copynumbers != numbers);
	controlkey(XK_n); assert(copynumbers == numbers);
	expectcopy("one cat\r\ntwo cat", 40, "v/cat\ry", "one c");
	expectcopy("ab界cd界ef", 40, "v/界\rny", "ab界cd界");
	copyfixture("012345abcdef\r\nghij", 8);
	first = termview().first;
	keysequence("/5abc\r"); assert(copyy == first && copyx == 5);
	keysequence("/fghi\r"); assert(copyy == first && copyx == 5);
	assert(strstr(copymodestatus(), "not found")); /* Cannot cross hard newline. */
	copymodeaction(COPY_VISUALRECT, 1);
	copymodeaction(COPY_RIGHT, 100);
	keysequence("/missing\r"); /* Rectangle padding must not make search loop forever. */
	assert(copyactive && strstr(copymodestatus(), "not found"));
	copyfixture("number 123 and a.b\r\nnumber 123", 40);
	keysequence("/123\r"); assert(copyx == 7);
	keysequence("/a.b\r"); assert(copyx == 15); /* Literal punctuation. */
	keysequence("/界x\b\b"); assert(inputlen == 0);
	for (i = 0; i < SEARCHLENGTH + 1; i++)
		keysequence("x");
	assert(inputlen == SEARCHLENGTH && strstr(copymodestatus(), "too long"));
	controlkey(XK_c); assert(copyactive && parserstate == CP_IDLE);
	copyfixture("retained marker\r\n", 40);
	for (i = 0; i < 12; i++)
		feed("live output\r\n");
	keysequence("G?retained\r");
	assert(copyy == termview().first && term.scr > 0);
	keysequence("/retained");
	for (i = 0; i < HISTSIZE + 10; i++)
		feed("replacement\r\n");
	keysequence("\r");
	assert(copyactive && strstr(copymodestatus(), "not found"));
	resize(8, 6);
	feed("\033[?1049h\033[2J\033[Halternate cat");
	copymode(NULL); keysequence("gg/5abc\r");
	assert(strstr(copymodestatus(), "not found"));
	keysequence("/cat\r"); assert(copyx == 2 && copyy == termview().first + 1);
	copymodeaction(COPY_EXIT, 1);
	feed("\033[?1049l");
	resize(40, 6);
}

static void
testexternalpipe(void)
{
	char *command[] = {"/bin/sh", "-c", "sleep 1", NULL};
	Arg a = {.v = command};
	struct timespec before, after;
	double elapsed;

	clock_gettime(CLOCK_MONOTONIC, &before);
	externalpipe(&a);
	clock_gettime(CLOCK_MONOTONIC, &after);
	elapsed = after.tv_sec - before.tv_sec +
	    (after.tv_nsec - before.tv_nsec) / 1e9;
	assert(elapsed < 0.5);
	while (waitpid(-1, NULL, 0) > 0)
		;
}

int
main(void)
{
	setlocale(LC_CTYPE, "");
	cmdfd = open("/dev/null", O_WRONLY);
	assert(cmdfd >= 0);
	tnew(120, 24);
	selinit();
	/* Empty rows below the cursor must be freed when replaced by reflow. */
	{
		size_t baseline = allocated;
		resize(40, 24);
		resize(120, 24);
		assert(allocated == baseline);
	}
	resize(40, 6);
	testcopy();
	teststream();
	testpaste();
	testoverwrite();
	testmouse();
	testmotions();
	testparser();
	testflash();
	testcommands();
	testreflow();
	testexternalpipe();
	puts("headless terminal tests passed");
	return 0;
}

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
	for (; *s; s++)
		copymodekey((unsigned char)*s, *s, 0, 0, copykeys,
		    sizeof copykeys / sizeof *copykeys);
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
	keysequence("3?j"); assert(copyy == first + 6);
	keysequence("5");
	copymodekey(XK_Shift_L, 0, 0, 1, copykeys, sizeof copykeys / sizeof *copykeys);
	keysequence("G"); assert(copyy == first + 4);
	keysequence("gg3");
	copymodekey(XK_Shift_L, 0, 0, 1, copykeys, sizeof copykeys / sizeof *copykeys);
	keysequence("W"); assert(copyx == 14);
	keysequence("9999999999"); assert(pendingcount == 1000000);
	keysequence("?"); assert(parserstate == CP_IDLE && pendingcount == 0);
	copymodeaction(COPY_EXIT, 1);
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
	testreflow();
	testexternalpipe();
	puts("headless terminal tests passed");
	return 0;
}

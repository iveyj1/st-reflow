/* Copy mode reads terminal rows through term.h; motions never mutate the view.
 * Deliberate Vim deviations: j/k use physical rows, % means last printable,
 * V selects logical/wrapped lines, G goes to the live cursor. Search is literal
 * and global; character-find is logical-line-local. Copies leave copy mode. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "st.h"
#include "term.h"
#include "win.h"

enum copyvisual_mode { COPY_NONE, COPY_CHAR, COPY_LINE, COPY_RECT };
static int copyactive, copyx, copynumbers = 1;
static long long copyy;
static enum copyvisual_mode copyvisual;
static enum copyparserstate parserstate;
static int pendingcount, pendingyank, operatorcount, commandcount;
static int finddirection, findtill, inputdirection, inputtill;
static Rune findchar;
#define SEARCHLENGTH 256
static Rune searchtext[SEARCHLENGTH], searchinput[SEARCHLENGTH];
static int searchlen, inputlen, searchdirection = 1;
static const char *copymessage = "";

/* Prefix-free labels: home-row singles, other letters start doubles. */
#define FLASHMAX 512
static struct { CPos pos; char label[8]; } flashtargets[FLASHMAX];
static int flashstate, flashcount, flashprefix;
static long long flashtop;
/* state: 1 = Space leader, 2 = character, 3 = label */

void
copyflashcancel(void)
{
	if (!flashstate)
		return;
	flashstate = flashcount = flashprefix = 0;
	copymessage = "";
	termdirty();
}

const char *
copyflashlabel(int index, int *x, int *y)
{
	if (flashstate != 3 || index >= flashcount)
		return NULL;
	*x = flashtargets[index].pos.x;
	*y = flashtargets[index].pos.y - termview().top;
	return flashtargets[index].label;
}

static CPos
clamp(CPos p, int rectangle)
{
	TView v = termview();
	int len;

	p.y = MAX(v.first, MIN(p.y, v.last));
	p.x = MAX(0, MIN(p.x, v.cols - 1));
	len = termlinelen(p.y);
	if (!rectangle)
		p.x = MIN(p.x, MAX(len - 1, 0));
	if (termline(p.y)[p.x].mode & ATTR_WDUMMY && p.x > 0)
		p.x--;
	return p;
}

static int
copyview(void)
{
	return copyy - termview().top;
}

static int
copyselectiontype(void)
{
	return copyvisual == COPY_RECT ? SEL_RECTANGULAR : SEL_REGULAR;
}

static void
copyextend(void)
{
	if (copyvisual)
		termselextend((CPos){copyy, copyx}, copyselectiontype(), 0);
}

static void
applymotion(CMotion m)
{
	CPos p;

	if (!m.ok)
		return;
	p = clamp(m.to, copyvisual == COPY_RECT);
	copyx = p.x;
	copyy = p.y;
	termshow(copyy);
	copyextend();
	termdirty();
}

void
copymodesync(void)
{
	if (copyactive)
		applymotion((CMotion){{copyy, copyx}, M_EXCL, 1});
	if (flashstate && termview().top != flashtop)
		copyflashcancel();
}

void
copymodelostselection(void)
{
	copyvisual = COPY_NONE;
}

static int
charclass(CPos p, int big)
{
	const Glyph *g = &termline(p.y)[p.x];

	if (!(g->mode & ATTR_SET) || !g->u || iswspace(g->u))
		return 0;
	return big || iswalnum(g->u) || g->u == L'_' ? 1 : 2;
}

/* Only modifies a local motion position. 2 means a hard-line boundary. */
static int
advance(CPos *p, int direction)
{
	TView v = termview();
	int hard = 0, len;

	do {
		len = termlinelen(p->y);
		if (direction > 0 && len > 0 && p->x < len - 1)
			p->x++;
		else if (direction < 0 && p->x > 0)
			p->x--;
		else {
			if (direction > 0)
				hard |= !termwrapped(p->y);
			if ((direction > 0 && p->y >= v.last) ||
			    (direction < 0 && p->y <= v.first))
				return 0;
			p->y += direction;
			p->x = direction > 0 ? 0 : MAX(termlinelen(p->y) - 1, 0);
			if (direction < 0)
				hard |= !termwrapped(p->y);
		}
	} while (termline(p->y)[p->x].mode & ATTR_WDUMMY);
	return hard ? 2 : 1;
}

static CPos
wordmotion(CPos p, int count, int direction, int big, int end, int *exhausted)
{
	int cls, moved;

	while (count-- > 0) {
		if (end) {
			if (!advance(&p, 1))
				break;
			while (!charclass(p, big))
				if (!advance(&p, 1))
					return p;
			cls = charclass(p, big);
			while ((moved = advance(&p, 1))) {
				if (moved == 2 || charclass(p, big) != cls) {
					advance(&p, -1);
					break;
				}
			}
		} else if (direction > 0) {
			cls = charclass(p, big);
			do {
				moved = advance(&p, 1);
				if (!moved) {
					if (exhausted)
						*exhausted = 1;
					return p;
				}
			} while (moved != 2 && cls && charclass(p, big) == cls);
			while (!charclass(p, big))
				if (!advance(&p, 1)) {
					if (exhausted)
						*exhausted = 1;
					return p;
				}
		} else {
			if (!advance(&p, -1))
				break;
			while (!charclass(p, big))
				if (!advance(&p, -1))
					return p;
			cls = charclass(p, big);
			while ((moved = advance(&p, -1))) {
				if (moved == 2 || charclass(p, big) != cls) {
					advance(&p, 1);
					break;
				}
			}
		}
	}
	return p;
}

CMotion
copymotion(CPos from, enum copymode_action action, int count, int rectangle)
{
	TView v = termview();
	CPos p = clamp(from, rectangle);
	CMotion m = {p, M_EXCL, 1};
	int i, len, step = 0, exhausted = 0;
	const Glyph *line;

	count = MAX(1, MIN(count, 1000000));
	switch (action) {
	case COPY_LEFT: p.x -= count; break;
	case COPY_RIGHT: p.x += count; break;
	case COPY_UP: p.y -= count; break;
	case COPY_DOWN: p.y += count; break;
	case COPY_WORD_FORWARD: p = wordmotion(p, count, 1, 0, 0, &exhausted); break;
	case COPY_WORD_FORWARD_BIG: p = wordmotion(p, count, 1, 1, 0, &exhausted); break;
	case COPY_WORD_BACK: p = wordmotion(p, count, -1, 0, 0, NULL); break;
	case COPY_WORD_BACK_BIG: p = wordmotion(p, count, -1, 1, 0, NULL); break;
	case COPY_WORD_END: p = wordmotion(p, count, 1, 0, 1, NULL); m.kind = M_INCL; break;
	case COPY_WORD_END_BIG: p = wordmotion(p, count, 1, 1, 1, NULL); m.kind = M_INCL; break;
	case COPY_HOME: p.x = 0; break;
	case COPY_END:
		p.y += count - 1;
		p.x = v.cols - 1;
		m.kind = M_INCL;
		break;
	case COPY_FIRSTPRINT:
	case COPY_LASTPRINT:
		if (action == COPY_LASTPRINT)
			p.y += count - 1;
		p = clamp(p, rectangle);
		line = termline(p.y);
		len = termlinelen(p.y);
		p.x = 0;
		for (i = action == COPY_LASTPRINT ? len - 1 : 0;
		    BETWEEN(i, 0, len - 1); i += action == COPY_LASTPRINT ? -1 : 1) {
			if ((line[i].mode & ATTR_SET) && !(line[i].mode & ATTR_WDUMMY) &&
			    line[i].u && iswgraph(line[i].u)) {
				p.x = i;
				break;
			}
		}
		m.kind = M_INCL;
		break;
	case COPY_HALFUP: step = -MAX(v.rows / 2, 1); break;
	case COPY_HALFDOWN: step = MAX(v.rows / 2, 1); break;
	case COPY_PAGEUP: step = -MAX(v.rows - 1, 1); break;
	case COPY_PAGEDOWN: step = MAX(v.rows - 1, 1); break;
	case COPY_TOP: p = (CPos){v.first, 0}; break;
	case COPY_BOTTOM: p = v.cursor; break;
	case COPY_GOTO_LINE: p = (CPos){v.first + count - 1, 0}; break;
	default: m.ok = 0; break;
	}
	/* Preserve intermediate short-row column clamping for counted pages. */
	while (step && count-- > 0) {
		CPos old = p;
		p.y += step;
		p = clamp(p, rectangle);
		if (p.y == old.y && p.x == old.x)
			break;
	}
	if (exhausted)
		m.kind = M_INCL;
	m.to = clamp(p, rectangle);
	return m;
}

int
copyoperator(enum copymode_action op, CRange range)
{
	char *s;
	CPos swap;
	CRange existing;
	int flash = !copyvisual && !termrange(&existing);
	int type = range.kind == M_RECT ? SEL_RECTANGULAR : SEL_REGULAR;

	if (op != COPY_YANK && op != COPY_YANK_CLEAN)
		return 0;
	if (range.start.y > range.end.y ||
	    (range.start.y == range.end.y && range.start.x > range.end.x)) {
		swap = range.start; range.start = range.end; range.end = swap;
	}
	if (range.kind == M_EXCL) {
		if (range.start.y == range.end.y && range.start.x == range.end.x)
			return 0;
		advance(&range.end, -1);
	}
	termselstart(range.start, range.kind == M_LINE ? SNAP_LINE : 0);
	termselextend(range.end, type, 0);
	termselextend(range.end, type, 1);
	if (!(s = getsel()))
		return 0;
	if (range.kind == M_LINE && (!s[0] || s[strlen(s) - 1] != '\n')) {
		size_t len = strlen(s);
		s = xrealloc(s, len + 2);
		s[len] = '\n';
		s[len + 1] = '\0';
	}
	if (range.kind == M_EOL && s[0] && s[strlen(s) - 1] == '\n')
		s[strlen(s) - 1] = '\0';
	if (op == COPY_YANK_CLEAN)
		seltrimtrailingws(s);
	xsetsel(s);
	xclipcopy();
	if (flash)
		yankflashstart();
	selclear();
	return 1;
}

/* End of the counted logical line, not the counted display row. */
static CPos
logicalend(CPos p, int count)
{
	TView v = termview();

	do {
		while (p.y < v.last && termwrapped(p.y))
			p.y++;
		if (--count <= 0 || p.y == v.last)
			break;
		p.y++;
	} while (1);
	p.x = MAX(termlinelen(p.y) - 1, 0);
	return clamp(p, 0);
}

void
copymodeaction(enum copymode_action action, int count)
{
	CRange range;
	CMotion motion;

	if (!copyactive)
		return;
	copymodesync();
	motion = copymotion((CPos){copyy, copyx}, action, count, copyvisual == COPY_RECT);
	if (motion.ok) {
		/* These commands historically reset the view before moving. */
		if (action == COPY_BOTTOM)
			termshow(termview().last);
		else if (action == COPY_TOP || action == COPY_GOTO_LINE)
			termshow(termview().first);
		applymotion(motion);
		return;
	}
	switch (action) {
	case COPY_NUMBERS: copynumbers = !copynumbers; break;
	case COPY_VISUAL:
	case COPY_VISUALLINE:
	case COPY_VISUALRECT:
		if (copyvisual) {
			copyvisual = COPY_NONE;
			selclear();
		} else {
			copyvisual = action == COPY_VISUALLINE ? COPY_LINE :
			    action == COPY_VISUALRECT ? COPY_RECT : COPY_CHAR;
			termselstart((CPos){copyy, copyx}, copyvisual == COPY_LINE ? SNAP_LINE : 0);
		}
		break;
	case COPY_YANK:
	case COPY_YANK_CLEAN:
	case COPY_YANK_END:
	case COPY_YANK_ALIAS:
	case COPY_YANK_LINE:
		if (copyvisual) {
			if (!termrange(&range))
				break;
			range.kind = copyvisual == COPY_LINE ? M_LINE :
			    copyvisual == COPY_RECT ? M_RECT : M_INCL;
			if (action == COPY_YANK_END)
				action = COPY_YANK_CLEAN;
		} else {
			range = (CRange){{copyy, copyx}, logicalend((CPos){copyy, copyx}, MAX(1, count)),
			    action == COPY_YANK || action == COPY_YANK_LINE ? M_LINE : M_EOL};
		}
		if (!copyoperator(action == COPY_YANK_CLEAN ? COPY_YANK_CLEAN : COPY_YANK, range))
			break;
		/* FALLTHROUGH */
	case COPY_EXIT:
		copyactive = 0;
		copyvisual = COPY_NONE;
		copyparserreset();
		selclear();
		break;
	default: break;
	}
	copyextend();
	termdirty();
}

void
copymode(const Arg *arg)
{
	TView v = termview();
	(void)arg;

	if (copyactive) {
		copymodeaction(COPY_EXIT, 1);
		return;
	}
	yankflashcancel();
	copyparserreset();
	copyactive = 1;
	copyvisual = COPY_NONE;
	applymotion((CMotion){v.scrolled ? (CPos){v.top + v.rows - 1, 0} : v.cursor,
	    M_EXCL, 1});
}

int
copymodeactive(void)
{
	return copyactive;
}

void
copymodemouse(int x, int y, int drag)
{
	TView v = termview();

	if (!copyactive)
		return;
	copyparserreset();
	if (drag && !copyvisual) {
		copyvisual = COPY_CHAR;
		termselstart((CPos){copyy, copyx}, 0);
	}
	applymotion((CMotion){{v.top + MAX(0, MIN(y, v.rows - 1)), x}, M_EXCL, 1});
}

int
copymodecursor(int *x, int *y)
{
	if (!copyactive)
		return 0;
	*x = copyx;
	*y = copyview();
	return 1;
}

int
copymodenumber(int row)
{
	return copyactive && !flashstate && copynumbers && row != copyview() ? abs(row - copyview()) : 0;
}

void
copyparserreset(void)
{
	copyflashcancel();
	parserstate = CP_IDLE;
	pendingcount = pendingyank = operatorcount = commandcount = inputlen = 0;
	copymessage = "";
}

static int
samepos(CPos a, CPos b)
{
	return a.y == b.y && a.x == b.x;
}

static CMotion
findmotion(int direction, int till, Rune target, int count, int repeat)
{
	CPos from = {copyy, copyx}, p = from, end;
	CMotion m = {from, M_INCL, 0};

	while (advance(&p, direction) == 1) {
		if (termline(p.y)[p.x].u != target)
			continue;
		end = p;
		if (till)
			advance(&end, -direction);
		/* Repeating t/T must not get stuck beside the previous target. */
		if (repeat && till && samepos(end, from))
			continue;
		if (--count == 0) {
			m.to = end;
			m.ok = 1;
			return m;
		}
	}
	copymessage = "Character not found";
	return m;
}

static int
searchmatch(CPos p)
{
	int i;

	for (i = 0; i < searchlen; i++) {
		if (p.x >= termlinelen(p.y) || termline(p.y)[p.x].u != searchtext[i])
			return 0;
		if (i + 1 < searchlen && advance(&p, 1) != 1)
			return 0;
	}
	return 1;
}

static CMotion
searchmotion(int direction, int count)
{
	TView v = termview();
	CPos from = clamp((CPos){copyy, copyx}, 0), p = from;
	CMotion m = {from, M_EXCL, 0};
	int wrapped = 0;

	if (!searchlen) {
		copymessage = "No previous search";
		return m;
	}
	do {
		if (!advance(&p, direction)) {
			p = direction > 0 ? (CPos){v.first, 0} :
			    clamp((CPos){v.last, v.cols - 1}, 0);
			wrapped = 1;
		}
		if (searchmatch(p) && --count == 0) {
			m.to = p;
			m.ok = 1;
			copymessage = wrapped ? "Search wrapped" : "";
			return m;
		}
	} while (!samepos(p, from));
	copymessage = "Pattern not found";
	return m;
}

static void
finishmotion(CMotion m)
{
	int yank = pendingyank;
	const char *message = copymessage;
	CPos from = {copyy, copyx};

	copyparserreset();
	copymessage = message;
	if (m.ok) {
		if (!yank)
			applymotion(m);
		else if (copyoperator(COPY_YANK, (CRange){from, m.to, m.kind}))
			copymodeaction(COPY_EXIT, 1);
	}
	termdirty();
}

const char *
copymodestatus(void)
{
	static char text[SEARCHLENGTH * 4 + 96];
	char *p = text;
	int i;

	if (!copyactive)
		return "";
	if (flashstate)
		return flashstate == 1 ? "Flash: s" : flashstate == 2 ?
		    "Flash: character" : flashprefix ? "Flash: second label letter" :
		    "Flash: label (Escape cancels)";
	if (parserstate == CP_SEARCH) {
		*p++ = inputdirection > 0 ? '/' : '?';
		for (i = 0; i < inputlen; i++)
			p += utf8encode(searchinput[i], p);
		snprintf(p, sizeof(text) - (p - text), "_ %s", copymessage);
		return text;
	}
	if (copymessage[0])
		return copymessage;
	if (parserstate == CP_FIND)
		return "Find character: _";
	return pendingyank ? "y: motion or y" : "";
}

static void
flashstart(Rune ch)
{
	static const char singles[] = "asdfghjkl";
	static const char first[] = "qwertyuiopzxcvbnm";
	static const char second[] = "asdfghjklqwertyuiopzxcvbnm";
	CPos after[FLASHMAX], before[FLASHMAX], p;
	char labels[FLASHMAX][8] = {{0}};
	TView v = termview();
	int na = 0, nb = 0, a = 0, b = 0, nl = 0, i, j, x, width, turn = 0;
	long long y;
	const Glyph *line;

	/* Scan outwards from the cursor, retaining the nearest visible matches. */
	for (y = MAX(copyy, v.top); y <= MIN(v.last, v.top + v.rows - 1); y++) {
		line = termline(y);
		for (x = y == copyy ? copyx + 1 : 0; x < v.cols && na < FLASHMAX; x++)
			if ((line[x].mode & ATTR_SET) && !(line[x].mode & ATTR_WDUMMY) && line[x].u == ch)
				after[na++] = (CPos){y, x};
	}
	for (y = MIN(copyy, v.top + v.rows - 1); y >= MAX(v.first, v.top); y--) {
		line = termline(y);
		for (x = y == copyy ? copyx - 1 : v.cols - 1; x >= 0 && nb < FLASHMAX; x--)
			if ((line[x].mode & ATTR_SET) && !(line[x].mode & ATTR_WDUMMY) && line[x].u == ch)
				before[nb++] = (CPos){y, x};
	}
	labels[nl][utf8encode(ch, labels[nl])] = '\0';
	nl++;
	for (i = 0; singles[i]; i++)
		if ((Rune)singles[i] != ch)
			labels[nl++][0] = singles[i];
	for (i = 0; first[i]; i++)
		if ((Rune)first[i] != ch)
			for (j = 0; second[j]; j++) {
				labels[nl][0] = first[i];
				labels[nl++][1] = second[j];
			}
	flashcount = 0;
	while ((a < na || b < nb) && flashcount < nl) {
		/* First forward, then alternate forward and backward like vigor. */
		if (a < na && (!turn || b >= nb))
			p = after[a++];
		else
			p = before[b++];
		turn = flashcount ? !turn : 0;
		width = flashcount == 0 ? MAX(1, wcwidth(ch)) : strlen(labels[flashcount]);
		if (p.x + width > v.cols)
			continue;
		line = termline(p.y);
		if (line[p.x].mode & ATTR_WDUMMY)
			continue;
		for (i = 1; i < width; i++)
			if ((line[p.x + i].mode & (ATTR_WIDE | ATTR_WDUMMY)) &&
			    !(i == 1 && (line[p.x].mode & ATTR_WIDE)))
				break;
		if (i < width)
			continue;
		for (i = 0; i < flashcount; i++)
			if (flashtargets[i].pos.y == p.y && abs(flashtargets[i].pos.x - p.x) <= 1)
				break;
		if (i < flashcount)
			continue;
		flashtargets[flashcount].pos = p;
		strcpy(flashtargets[flashcount].label, labels[flashcount]);
		flashcount++;
	}
	flashstate = 3;
	if (flashcount == 1) {
		p = flashtargets[0].pos;
		copyparserreset();
		applymotion((CMotion){p, M_INCL, 1});
	} else if (!flashcount) {
		copyparserreset();
		copymessage = "Flash: no targets";
	}
	termdirty();
}

static void
flashkey(int ch, unsigned int mods)
{
	char input[8] = {0};
	int i;
	CPos p;

	if (mods & (CM_CTRL | CM_ALT)) {
		copyparserreset();
		return;
	}
	if (flashstate == 1) {
		if (ch == 's')
			flashstate = 2;
		else
			copyparserreset();
	} else if (flashstate == 2) {
		if (ch >= 32 && iswprint(ch))
			flashstart(ch);
		else
			copyparserreset();
	} else if (ch > 0) {
		if (flashprefix) {
			input[0] = flashprefix;
			utf8encode(ch, input + 1);
		} else {
			utf8encode(ch, input);
		}
		for (i = 0; i < flashcount; i++) {
			if (!strcmp(input, flashtargets[i].label)) {
				p = flashtargets[i].pos;
				copyparserreset();
				applymotion((CMotion){p, M_INCL, 1});
				return;
			}
		}
		for (i = 0; i < flashcount; i++)
			if (!strncmp(input, flashtargets[i].label, strlen(input))) {
				flashprefix = ch;
				return;
			}
		copyparserreset();
	} else {
		copyparserreset();
	}
	termdirty();
}

void
copymodekey(unsigned long key, int ch, unsigned int mods, int modifier,
    const CopyKey *keys, size_t nkeys)
{
	size_t i;
	int count, escape = key == 0xff1b || ch == 27;
	enum copymode_action action;
	CMotion motion;

	if (modifier || !copyactive)
		return;
	copymodesync();
	if (escape && (flashstate || parserstate != CP_IDLE || pendingyank)) {
		copyparserreset();
		termdirty();
		return;
	}
	if (flashstate) {
		flashkey(ch, mods);
		return;
	}
	if (mods & CM_ALT)
		return;
	if (!(mods & CM_CTRL) && ch == ' ' &&
	    parserstate != CP_SEARCH && parserstate != CP_FIND) {
		copyparserreset();
		flashstate = 1;
		flashtop = termview().top;
		termdirty();
		return;
	}
	if (parserstate == CP_SEARCH) {
		copymessage = "";
		if (key == 0xff0d || ch == '\r') {
			if (inputlen) {
				memcpy(searchtext, searchinput, inputlen * sizeof(Rune));
				searchlen = inputlen;
			}
			searchdirection = inputdirection;
			finishmotion(searchmotion(searchdirection, commandcount));
		} else if ((mods & CM_CTRL) && (key == 'c' || ch == 3)) {
			copyparserreset();
		} else if ((mods & CM_CTRL) && (key == 'u' || ch == 21)) {
			inputlen = 0;
		} else if (key == 0xff08 || ch == 8 || ch == 127) {
			inputlen = MAX(0, inputlen - 1);
		} else if (!(mods & CM_CTRL) && ch >= 32 && iswprint(ch)) {
			if (inputlen < SEARCHLENGTH)
				searchinput[inputlen++] = ch;
			else
				copymessage = "Pattern too long";
		}
		termdirty();
		return;
	}
	copymessage = "";
	if (parserstate == CP_FIND) {
		if (!(mods & CM_CTRL) && ch >= 32 && iswprint(ch)) {
			findchar = ch;
			finddirection = inputdirection;
			findtill = inputtill;
			finishmotion(findmotion(finddirection, findtill, findchar, commandcount, 0));
		} else {
			copyparserreset();
			termdirty();
		}
		return;
	}
	if (!(mods & CM_CTRL) && ch >= '0' && ch <= '9' &&
	    (ch != '0' || pendingcount)) {
		pendingcount = MIN(1000000, pendingcount * 10 + ch - '0');
		parserstate = CP_COUNT;
		return;
	}
	count = pendingcount ? pendingcount : 1;
	if (pendingyank)
		count = MIN(1000000LL, (long long)count * operatorcount);
	for (i = 0; i < nkeys; i++) {
		if ((mods & CM_CTRL) != (keys[i].mods & CM_CTRL))
			continue;
		if ((keys[i].mods & CM_NOSHIFT) && (mods & CM_SHIFT))
			continue;
		if (keys[i].type == CK_KEY ? key != keys[i].key : ch != keys[i].ch)
			continue;
		action = keys[i].action;
		if (keys[i].type == CK_PREFIX && parserstate != CP_PREFIX) {
			parserstate = CP_PREFIX;
			return;
		}
		if (action == COPY_YANK && !copyvisual) {
			if (!pendingyank) {
				pendingyank = 1;
				operatorcount = count;
				pendingcount = 0;
				parserstate = CP_OPERATOR;
				termdirty();
				return;
			}
			action = COPY_YANK_LINE;
		}
		if (action >= COPY_FIND && action <= COPY_TILL_BACK) {
			inputdirection = action >= COPY_FIND_BACK ? -1 : 1;
			inputtill = action == COPY_TILL || action == COPY_TILL_BACK;
			commandcount = count;
			parserstate = CP_FIND;
			termdirty();
			return;
		}
		if (action == COPY_FIND_REPEAT || action == COPY_FIND_REVERSE) {
			if (findchar)
				finishmotion(findmotion(finddirection * (action == COPY_FIND_REPEAT ? 1 : -1),
				    findtill, findchar, count, 1));
			else {
				copyparserreset();
				copymessage = "No previous character find";
				termdirty();
			}
			return;
		}
		if (action == COPY_SEARCH || action == COPY_SEARCH_BACK) {
			inputdirection = action == COPY_SEARCH ? 1 : -1;
			inputlen = 0;
			commandcount = count;
			parserstate = CP_SEARCH;
			termdirty();
			return;
		}
		if (action == COPY_SEARCH_REPEAT || action == COPY_SEARCH_REVERSE) {
			finishmotion(searchmotion(searchdirection * (action == COPY_SEARCH_REPEAT ? 1 : -1), count));
			return;
		}
		if (pendingcount && (action == COPY_TOP || action == COPY_BOTTOM))
			action = COPY_GOTO_LINE;
		if (pendingyank && action != COPY_YANK_LINE) {
			motion = copymotion((CPos){copyy, copyx}, action, count, 0);
			/* Vim's yw/yW do not consume the next line's indentation. */
			if (action == COPY_WORD_FORWARD || action == COPY_WORD_FORWARD_BIG) {
				CPos before = count == 1 ? (CPos){copyy, copyx} :
				    copymotion((CPos){copyy, copyx}, action, count - 1, 0).to;
				CPos end = logicalend(before, 1);
				if (motion.to.y > end.y) {
					motion.to = end;
					motion.kind = M_EOL;
				}
			}
			finishmotion(motion);
			return;
		}
		copyparserreset();
		copymodeaction(action, count);
		return;
	}
	copyparserreset();
	termdirty();
}

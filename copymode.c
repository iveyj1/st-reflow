/* Copy mode reads terminal rows through term.h; motions never mutate the view.
 * Deliberate Vim deviations: j/k use physical rows, % means last printable,
 * V selects logical/wrapped lines, G goes to the live cursor, y/Y immediately
 * copy the visual range or current logical line. No operator-pending bindings. */
#include <stdlib.h>
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
static int pendingcount;

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
wordmotion(CPos p, int count, int direction, int big, int end)
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
				if (!moved)
					return p;
			} while (moved != 2 && cls && charclass(p, big) == cls);
			while (!charclass(p, big))
				if (!advance(&p, 1))
					return p;
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
	int i, len, step = 0;
	const Glyph *line;

	count = MAX(1, MIN(count, 1000000));
	switch (action) {
	case COPY_LEFT: p.x -= count; break;
	case COPY_RIGHT: p.x += count; break;
	case COPY_UP: p.y -= count; break;
	case COPY_DOWN: p.y += count; break;
	case COPY_WORD_FORWARD: p = wordmotion(p, count, 1, 0, 0); break;
	case COPY_WORD_FORWARD_BIG: p = wordmotion(p, count, 1, 1, 0); break;
	case COPY_WORD_BACK: p = wordmotion(p, count, -1, 0, 0); break;
	case COPY_WORD_BACK_BIG: p = wordmotion(p, count, -1, 1, 0); break;
	case COPY_WORD_END: p = wordmotion(p, count, 1, 0, 1); m.kind = M_INCL; break;
	case COPY_WORD_END_BIG: p = wordmotion(p, count, 1, 1, 1); m.kind = M_INCL; break;
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
	m.to = clamp(p, rectangle);
	return m;
}

int
copyoperator(enum copymode_action op, CRange range)
{
	char *s;
	CPos swap;
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
	if (op == COPY_YANK_CLEAN)
		seltrimtrailingws(s);
	xsetsel(s);
	xclipcopy();
	selclear();
	return 1;
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
		if (copyvisual) {
			if (!termrange(&range))
				break;
			range.kind = copyvisual == COPY_LINE ? M_LINE :
			    copyvisual == COPY_RECT ? M_RECT : M_INCL;
		} else {
			range = (CRange){{copyy, copyx}, {copyy, copyx}, M_LINE};
		}
		if (!copyoperator(action, range))
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
	return copyactive && copynumbers && row != copyview() ? abs(row - copyview()) : 0;
}

void
copyparserreset(void)
{
	parserstate = CP_IDLE;
	pendingcount = 0;
}

void
copymodekey(unsigned long key, int ch, unsigned int mods, int modifier,
    const CopyKey *keys, size_t nkeys)
{
	size_t i;
	int count;
	enum copymode_action action;

	if (modifier)
		return;
	if (!(mods & (CM_CTRL | CM_ALT)) && ch >= '0' && ch <= '9' &&
	    (ch != '0' || pendingcount)) {
		pendingcount = MIN(1000000, pendingcount * 10 + ch - '0');
		parserstate = CP_COUNT;
		return;
	}
	count = pendingcount ? pendingcount : 1;
	for (i = 0; i < nkeys; i++) {
		/* Character entries already encode Shift in their ASCII value. */
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
		if (pendingcount && (action == COPY_TOP || action == COPY_BOTTOM))
			action = COPY_GOTO_LINE;
		copyparserreset();
		copymodeaction(action, count);
		return;
	}
	copyparserreset();
}

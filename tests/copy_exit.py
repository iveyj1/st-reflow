#!/usr/bin/env python3
"""Run selection-exit control flow with mocked clipboard/X operations."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index('\n' + name + '(') + 1
    end = source.index('\n}', start) + 2
    return source[start:end]


class CopyExit(unittest.TestCase):
    def test_copy_exit(self):
        x = (ROOT / 'x.c').read_text()
        st = (ROOT / 'st.c').read_text()
        yank = st[st.index('\tcase COPY_YANK:'):st.index('\tcase COPY_EXIT:')]
        code = r'''
#include <assert.h>
#include <stddef.h>
typedef int Arg;
enum { COPY_YANK, COPY_YANK_CLEAN, COPY_EXIT, SNAP_LINE, SEL_REGULAR };
static int copyactive, copyvisual, copyx, copyy, highlight, copies, trims;
static int scroll = 42;
static char text[] = "selected text";
static char *selection;
static struct { char *primary, *clipboard; } xsel;
static void selclear(void) { highlight = 0; }
static int copymodeactive(void) { return copyactive; }
static void copymodeaction(int action, int count) {
    assert(action == COPY_EXIT && count == 1);
    copyactive = copyvisual = 0;
    selclear();
}
static void xclipcopy(void) { xsel.clipboard = xsel.primary; copies++; }
static char *getsel(void) { return selection; }
static void seltrimtrailingws(char *s) { assert(s); trims++; }
static void setsel(char *s, int time) { (void)time; xsel.primary = s; }
static void xsetsel(char *s) { xsel.primary = s; }
#define CurrentTime 0
static void selstart(int x, int y, int mode) { (void)x; (void)y; (void)mode; }
static void selextend(int x, int y, int type, int done) {
    (void)x; (void)y; (void)type; (void)done;
}
static int copyselectiontype(void) { return SEL_REGULAR; }
'''
        code += '\nstatic void\n' + function(x, 'clipcopy')
        code += '\nstatic void\n' + function(x, 'clipcopyclean')
        code += '\nstatic void yank(int action) { char *s; switch(action) {\n'
        code += yank + '\n} }\n'
        code += r'''
static void reset(int active) {
    copyactive = active;
    copyvisual = highlight = 1;
    copies = trims = 0;
    selection = xsel.primary = text;
    xsel.clipboard = NULL;
}
static void copied(void) {
    assert(!copyactive && !highlight);
    assert(xsel.primary == text && xsel.clipboard == text);
    assert(copies == 1 && scroll == 42);
}
int main(void) {
    for (int active = 0; active <= 1; active++) {
        reset(active); clipcopy(NULL); copied(); assert(!trims);
        reset(active); clipcopyclean(NULL); copied(); assert(trims == 1);
        reset(active); xsel.primary = NULL; clipcopy(NULL);
        assert(copyactive == active && highlight && !copies);
        reset(active); selection = NULL; clipcopyclean(NULL);
        assert(copyactive == active && highlight && !copies);
    }
    for (int action = COPY_YANK; action <= COPY_YANK_CLEAN; action++) {
        for (int visual = 0; visual <= 3; visual++) {
            reset(1); copyvisual = visual; yank(action); copied();
            assert(!copyvisual && trims == (action == COPY_YANK_CLEAN));
            reset(1); copyvisual = visual; selection = NULL; yank(action);
            assert(copyactive && highlight && !copies);
            assert(copyvisual == visual);
        }
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'copy.c'
            binary = Path(directory) / 'copy'
            source.write_text(code)
            subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall',
                            '-Wextra', '-Wno-unused-parameter', '-o', str(binary),
                            str(source)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()

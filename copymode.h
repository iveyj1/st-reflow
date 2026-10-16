#ifndef COPYMODE_H
#define COPYMODE_H

typedef struct { long long y; int x; } CPos;
enum motionkind { M_EXCL, M_INCL, M_LINE, M_RECT };
typedef struct { CPos to; enum motionkind kind; int ok; } CMotion;
typedef struct { CPos start, end; enum motionkind kind; } CRange;
enum copyparserstate { CP_IDLE, CP_COUNT, CP_PREFIX, CP_OPERATOR, CP_REGISTER };
enum { CK_KEY, CK_CHAR, CK_PREFIX };
enum { CM_SHIFT = 1, CM_CTRL = 2, CM_ALT = 4, CM_NOSHIFT = 8 };
typedef struct {
	unsigned long key;
	unsigned int mods;
	int ch, type;
	enum copymode_action action;
	const char *label;
} CopyKey;

CMotion copymotion(CPos, enum copymode_action, int, int);
int copyoperator(enum copymode_action, CRange);
void copymodesync(void);
void copymodelostselection(void);
int copymodecursor(int *, int *);
int copymodenumber(int);
void copymodekey(unsigned long, int, unsigned int, int, const CopyKey *, size_t);
void copyparserreset(void);
#endif

# st - simple terminal
# See LICENSE file for copyright and license details.
.POSIX:

include config.mk

SRC = st.c x.c boxdraw.c
OBJ = $(SRC:.c=.o)

# Recreate buildinfo.h when the checked-out commit changes.  Follow the
# symbolic ref when possible; detached checkouts are covered by HEAD itself.
GITHEAD = $(shell git rev-parse --git-path HEAD 2>/dev/null)
GITREF = $(shell ref=$$(git symbolic-ref -q HEAD 2>/dev/null); \
	if test -n "$$ref"; then \
		path=$$(git rev-parse --git-path "$$ref"); \
		if test -e "$$path"; then printf '%s' "$$path"; \
		else git rev-parse --git-path packed-refs; fi; \
	fi)

all: st

buildinfo.h: $(GITHEAD) $(GITREF) Makefile
	@hash=$$(git rev-parse --short HEAD 2>/dev/null || printf unknown); \
	date=$$(git show -s --format=%cs HEAD 2>/dev/null || printf unknown); \
	printf '#define ST_CHECKIN_HASH "%s"\n#define ST_CHECKIN_DATE "%s"\n' \
		"$$hash" "$$date" > $@.tmp; \
	if test -r $@ && cmp -s $@.tmp $@; then rm -f $@.tmp; \
	else mv -f $@.tmp $@; fi

config.h:
	cp config.def.h config.h

.c.o:
	$(CC) $(STCFLAGS) -c $<

st.o: config.h st.h win.h
x.o: arg.h buildinfo.h config.h st.h win.h
boxdraw.o: config.h st.h boxdraw_data.h

$(OBJ): config.h config.mk

st: $(OBJ)
	$(CC) -o $@ $(OBJ) $(STLDFLAGS)

clean:
	rm -f st $(OBJ) buildinfo.h buildinfo.h.tmp st-$(VERSION).tar.gz

dist: clean
	$(MAKE) buildinfo.h
	mkdir -p st-$(VERSION)
	cp -R FAQ LEGACY TODO LICENSE Makefile README config.mk\
		buildinfo.h config.def.h st.info st.1 arg.h st.h win.h $(SRC)\
		st-$(VERSION)
	tar -cf - st-$(VERSION) | gzip > st-$(VERSION).tar.gz
	rm -rf st-$(VERSION)

install: st
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f st $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/st
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < st.1 > $(DESTDIR)$(MANPREFIX)/man1/st.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/st.1
	tic -sx st.info
	@echo Please see README.md regarding the terminfo entry of st.

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/st
	rm -f $(DESTDIR)$(MANPREFIX)/man1/st.1

.PHONY: all clean dist install uninstall

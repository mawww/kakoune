debug ?= yes
static ?= no
pedantic ?= yes

ifeq ($(debug),yes)
    CPPFLAGS += -DKAK_DEBUG
    suffix := .debug
else
    ifeq ($(debug),no)
        CXXFLAGS += -O3
        suffix := .opt
    else
        $(error debug should be either yes or no)
    endif
endif

ifeq ($(pedantic),yes)
    CXXFLAGS += -pedantic
endif

sources := $(wildcard *.cc)
objects := $(addprefix ., $(sources:.cc=$(suffix).o))
deps := $(addprefix ., $(sources:.cc=$(suffix).d))
docs := $(wildcard ../doc/manpages/*.asciidoc)
mandocs := $(docs:.asciidoc=.gz)

PREFIX ?= /usr/local
DESTDIR ?= # root dir

NCURSESW_INCLUDE ?= /usr/include/ncursesw

bindir := $(DESTDIR)$(PREFIX)/bin
sharedir := $(DESTDIR)$(PREFIX)/share/kak
docdir := $(DESTDIR)$(PREFIX)/share/doc/kak
mandir := $(DESTDIR)$(PREFIX)/share/man/man1

os := $(shell uname)

ifeq ($(os),Darwin)
    LIBS += -lncurses -lboost_regex-mt
    CPPFLAGS += -I$(PREFIX)/opt/ncurses/include -I$(PREFIX)/opt/boost/include
    LDFLAGS += -L$(PREFIX)/opt/ncurses/lib -L$(PREFIX)/opt/boost/lib
else ifeq ($(os),FreeBSD)
    LIBS += -ltinfow -lncursesw -lboost_regex
    CPPFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
else ifeq ($(os),Haiku)
    LIBS += -lncursesw -lboost_regex -lnetwork -lbe
else ifeq ($(os),DragonFly)
    LIBS += -lncursesw -lboost_regex
    CPPFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
else ifneq (,$(findstring CYGWIN,$(os)))
    CPPFLAGS += -D_XOPEN_SOURCE=700
    LIBS += -lncursesw -lboost_regex -ldbghelp
else
    LIBS += -lncursesw -lboost_regex
    CPPFLAGS += -I$(NCURSESW_INCLUDE)
    LDFLAGS += -rdynamic
endif

ifeq ($(static),yes)
    LIBS += -ltinfo -lgpm
    LDFLAGS += -static -pthread
endif

CXXFLAGS += -std=gnu++11 -g -Wall -Wno-reorder -Wno-sign-compare -Wno-address

all : kak
kak : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) $(LIBS) -o $@

-include $(deps)

.%$(suffix).o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -MF $(addprefix ., $(<:.cc=$(suffix).d)) -c -o $@ $<

# Generate the man page
../doc/kak.1.gz: ../doc/kak.1.txt
	a2x --no-xmllint -f manpage $<
	gzip -f $(basename $<)

# Generate the editor's documentation pages
# Since `a2x` won't generate man pages if some sections are missing (which we don't need),
# we generate the pages, patch them and then compress them
../doc/manpages/%.gz: ../doc/manpages/%.asciidoc
	a2x --no-xmllint -f manpage $<
	sed -i -r -e "s,^\.TH .+,.TH KAKOUNE 1 \"\" \"\" \"$(basename $(notdir $<))\"," \
	-e "/^\.SH \"NAME\"/{N;d;}" $(@:.gz=.1)
	gzip -f $(@:.gz=.1)
	mv -f $(@:.gz=.1.gz) $@

check: test
test:
	cd ../test && ./run

TAGS: tags
tags:
	ctags -R
man: ../doc/kak.1.gz
doc: $(mandocs)

clean:
	rm -f .*.o .*.d

distclean: clean
	rm -f kak

installdirs:
	install	-d $(bindir) \
		$(sharedir)/rc/base	\
		$(sharedir)/rc/core	\
		$(sharedir)/rc/extra \
		$(sharedir)/colors \
		$(docdir)/manpages \
		$(mandir)

install: kak man doc installdirs
	install	-m 0755	kak	$(bindir)
	install	-m 0644	../share/kak/kakrc $(sharedir)
	install	-m 0644	../rc/base/* $(sharedir)/rc/base
	install	-m 0644	../rc/core/* $(sharedir)/rc/core
	install	-m 0644	../rc/extra/* $(sharedir)/rc/extra
	[ -e $(sharedir)/autoload ]	|| ln -s rc	$(sharedir)/autoload
	install	-m 0644	../colors/*	$(sharedir)/colors
	install	-m 0644	../README.asciidoc $(docdir)
	install	-m 0644	../doc/manpages/*.gz $(docdir)/manpages
	install	-m 0644	../doc/kak.1.gz	$(mandir)

install-strip: install
	strip -s $(bindir)/kak

uninstall:
	rm -rf $(bindir)/kak \
		$(sharedir)	\
		$(docdir) \
		$(mandir)/kak.1.gz

.PHONY: check TAGS clean distclean installdirs install install-strip uninstall
.PHONY: tags test man doc

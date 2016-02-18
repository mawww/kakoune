debug ?= yes
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
    LIBS += -lncursesw -lboost_regex -ldbghelp
else
    LIBS += -lncursesw -lboost_regex
    CPPFLAGS += -I$(NCURSESW_INCLUDE)
    LDFLAGS += -rdynamic
endif

CXXFLAGS += -std=gnu++11 -g -Wall -Wno-reorder -Wno-sign-compare -Wno-address

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

test:
	cd ../test && ./run
tags:
	ctags -R
man: ../doc/kak.1.gz
doc: $(mandocs)

clean:
	rm -f .*.o .*.d kak tags

XDG_CONFIG_HOME ?= $(HOME)/.config

install: kak man doc
	mkdir -p $(bindir)
	install -m 0755 kak $(bindir)
	mkdir -p $(sharedir)/rc/base
	mkdir -p $(sharedir)/rc/core
	mkdir -p $(sharedir)/rc/extra
	install -m 0644 ../share/kak/kakrc $(sharedir)
	install -m 0644 ../rc/base/* $(sharedir)/rc/base
	install -m 0644 ../rc/core/* $(sharedir)/rc/core
	install -m 0644 ../rc/extra/* $(sharedir)/rc/extra
	[ -e $(sharedir)/autoload ] || ln -s rc $(sharedir)/autoload
	mkdir -p $(sharedir)/colors
	install -m 0644 ../colors/* $(sharedir)/colors
	mkdir -p $(docdir)/manpages
	mkdir -p $(mandir)
	install -m 0644 ../README.asciidoc $(docdir)
	install -m 0644 ../doc/manpages/*.gz $(docdir)/manpages
	install -m 0644 ../doc/kak.1.gz $(mandir)

.PHONY: tags install

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
    CPPFLAGS += -I/usr/local/opt/ncurses/include
    LDFLAGS += -L/usr/local/opt/ncurses/lib
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


# Version number handling. This save the current version number to a file and
# rebuilds version.cc iff the version number has actually changed.
version := $(shell git describe --always --dirty 2>/dev/null)
version_old = $(shell cat .version 2>/dev/null)

ifneq ($(version_old),$(version))
# Mark version.cc for rebuild. Do not use .PHONY because this needs to be
# persistent if make is interrupted. Do not use the $(file) function as it
# segfaults in GNU Make 4.0
$(shell touch version.cc)
$(file > .version,$(version))
endif

# Special treatment for version.cc: define KAK_VERSION
# This needs to be an "=" rather than ":=" to be expanded in rule context
version_cppflag = $(if $(filter version.cc,$<),-DKAK_VERSION=\"$(version)\")


all : kak
kak : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) $(LIBS) -o $@

-include $(deps)

.%$(suffix).o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(version_cppflag) \
	    -MD -MP -MF $(addprefix ., $(<:.cc=$(suffix).d)) -c -o $@ $<

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


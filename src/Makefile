debug ?= yes
static ?= no

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

sources := $(sort $(wildcard *.cc))
objects := $(addprefix ., $(sources:.cc=$(suffix).o))
deps := $(addprefix ., $(sources:.cc=$(suffix).d))
docs := $(wildcard ../doc/manpages/*.asciidoc)
mandocs := $(docs:.asciidoc=.gz)

PREFIX ?= /usr/local
DESTDIR ?= # root dir

bindir := $(DESTDIR)$(PREFIX)/bin
sharedir := $(DESTDIR)$(PREFIX)/share/kak
docdir := $(DESTDIR)$(PREFIX)/share/doc/kak
mandir := $(DESTDIR)$(PREFIX)/share/man/man1

os := $(shell uname)

ifeq ($(os),Darwin)
    LIBS += -lncurses
    CPPFLAGS += -I$(PREFIX)/opt/ncurses/include -I/opt/local/include
    LDFLAGS += -L$(PREFIX)/opt/ncurses/lib -L/opt/local/lib
else ifeq ($(os),FreeBSD)
    LIBS += -ltinfow -lncursesw
    CPPFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
else ifeq ($(os),Haiku)
    LIBS += -lncursesw -lnetwork -lbe
else ifeq ($(os),DragonFly)
    LIBS += -lncursesw
    CPPFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
else ifneq (,$(findstring CYGWIN,$(os)))
    CPPFLAGS += -D_XOPEN_SOURCE=700
    LIBS += -lncursesw -ldbghelp
else
    LIBS += $(shell pkg-config --libs ncursesw)
    CPPFLAGS += $(shell pkg-config --cflags ncursesw)
    LDFLAGS += -rdynamic
endif

ifeq ($(static),yes)
    LIBS += -ltinfo -lgpm
    LDFLAGS += -static -pthread
endif

CXXFLAGS += -pedantic -std=gnu++14 -g -Wall -Wextra -Wno-unused-parameter -Wno-reorder -Wno-sign-compare -Wno-address -Wno-noexcept-type -Wno-unknown-attributes -Wno-unknown-warning-option

all : kak

kak : kak$(suffix)
	ln -sf $< $@

kak$(suffix) : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) $(LIBS) -o $@

-include $(deps)

.%$(suffix).o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -MF $(addprefix ., $(<:.cc=$(suffix).d)) -c -o $@ $<

# Generate the man page
../doc/kak.1.gz: ../doc/kak.1.txt
	a2x --no-xmllint -f manpage $<
	gzip -n -9 -f $(basename $<)

# Generate the editor's documentation pages
# Since `a2x` won't generate man pages if some sections are missing (which we don't need),
# we generate the pages, patch them and then compress them
../doc/manpages/%.gz: ../doc/manpages/%.asciidoc
	a2x --no-xmllint -f manpage $<
	sed -i -r -e "s,^\.TH .+,.TH kakoune k \"\" \"\" \"$(basename $(notdir $<))\"," \
	-e "/^\.SH \"NAME\"/{N;d;}" $(@:.gz=.k)
	gzip -n -9 -f $(@:.gz=.k)
	mv -f $(@:.gz=.k.gz) $@

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
	rm -f kak kak$(suffix)
	find ../doc -type f -name \*\\.gz -exec rm -f '{}' +

installdirs:
	install -d $(bindir) \
		$(sharedir)/rc/base \
		$(sharedir)/rc/core \
		$(sharedir)/rc/extra \
		$(sharedir)/colors \
		$(sharedir)/doc \
		$(docdir) \
		$(mandir)

install: kak man doc installdirs
	install -m 0755 kak $(bindir)
	install -m 0644 ../share/kak/kakrc $(sharedir)
	install -m 0644 ../doc/manpages/*.gz $(sharedir)/doc
	install -m 0644 ../rc/base/* $(sharedir)/rc/base
	install -m 0644 ../rc/core/* $(sharedir)/rc/core
	install -m 0644 ../rc/extra/* $(sharedir)/rc/extra
	[ -e $(sharedir)/autoload ] || ln -s rc $(sharedir)/autoload
	install -m 0644 ../colors/* $(sharedir)/colors
	install -m 0644 ../README.asciidoc $(docdir)
	install -m 0644 ../doc/kak.1.gz $(mandir)

install-strip: install
	strip -s $(bindir)/kak

uninstall:
	rm -rf $(bindir)/kak \
		$(sharedir) \
		$(docdir) \
		$(mandir)/kak.1.gz

.PHONY: check TAGS clean distclean installdirs install install-strip uninstall
.PHONY: tags test man doc kak

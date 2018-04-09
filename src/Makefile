debug ?= yes
static ?= no
gzip_man ?= yes

ifneq ($(gzip_man),yes)
    ifneq ($(gzip_man),no)
        $(error gzip_man should be either yes or no)
    endif
endif

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

version ?= $(shell if [ -f .version ]; then cat .version; elif  [ -d ../.git ]; then git rev-parse --short HEAD; else echo "unknown"; fi )

sources := $(sort $(wildcard *.cc))
objects := $(addprefix ., $(sources:.cc=$(suffix).o))
deps := $(addprefix ., $(sources:.cc=$(suffix).d))

PKG_CONFIG ?= $(shell command -v pkg-config 2>/dev/null)

ifeq ($(static),yes)
    PKG_CONFIG_FLAGS += --static
    LDFLAGS += -static -pthread
endif

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
    ifeq ($(PKG_CONFIG),)
    	$(error "pkg-config not found in PATH")
    endif

    LIBS += $(shell $(PKG_CONFIG) $(PKG_CONFIG_FLAGS) --libs ncursesw)
    CPPFLAGS += $(shell $(PKG_CONFIG) $(PKG_CONFIG_FLAGS) --cflags ncursesw)
    LDFLAGS += -rdynamic
endif

CXXFLAGS += -pedantic -std=gnu++14 -g -Wall -Wextra -Wno-unused-parameter -Wno-reorder -Wno-sign-compare -Wno-address -Wno-noexcept-type -Wno-unknown-attributes -Wno-unknown-warning-option

all : kak

kak : kak$(suffix)
	ln -sf $< $@

kak$(suffix) : $(objects) .version.o
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) .version.o $(LIBS) -o $@

-include $(deps)

.%$(suffix).o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -MF $(addprefix ., $(<:.cc=$(suffix).d)) -c -o $@ $<

.version.o: .version.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
	
.version.cc: FORCE
	@printf "%s" 'namespace Kakoune { const char* version = "$(version)"; }' > .version.cc.tmp
	@if cmp -s .version.cc.tmp .version.cc; then rm .version.cc.tmp; else mv .version.cc.tmp .version.cc; fi

# Generate the man page
ifeq ($(gzip_man),yes)
../doc/kak.1.gz: ../doc/kak.1.asciidoc
	a2x --no-xmllint -f manpage $<
	gzip -n -9 -f $(basename $<)
man: ../doc/kak.1.gz
else
../doc/kak.1: ../doc/kak.1.asciidoc
	a2x --no-xmllint -f manpage $<
man: ../doc/kak.1
endif

check: test
test:
	cd ../test && ./run

TAGS: tags
tags:
	ctags -R

clean:
	rm -f .*.o .*.d

dist:
	@if ! [ -d ../.git ]; then echo "make dist can only run from a git repo";  false; fi
	@if git status -s | grep -qEv '^\?\?'; then echo "working tree is not clean";  false; fi
	cd ../;                                                                               \
	git archive --format=tar --prefix=kakoune-$(version)/ HEAD -o kakoune-$(version).tar; \
	echo "$(version)" > src/.version;                                                     \
	tar --transform "s,^,kakoune-$(version)/," -rf kakoune-$(version).tar src/.version;   \
	rm src/.version;                                                                      \
	bzip2 kakoune-$(version).tar;

distclean: clean
	rm -f kak kak$(suffix)
	find ../doc -type f \( -name \*\\.gz -o -name \*\\.1 \) -exec rm -f '{}' +

installdirs:
	install -d $(bindir) \
		$(sharedir)/rc/base \
		$(sharedir)/rc/core \
		$(sharedir)/rc/extra \
		$(sharedir)/colors \
		$(sharedir)/doc \
		$(docdir) \
		$(mandir)

install: kak man installdirs
	install -m 0755 kak $(bindir)
	install -m 0644 ../share/kak/kakrc $(sharedir)
	install -m 0644 ../doc/pages/*.asciidoc $(sharedir)/doc
	install -m 0644 ../rc/base/* $(sharedir)/rc/base
	install -m 0644 ../rc/core/* $(sharedir)/rc/core
	install -m 0644 ../rc/extra/* $(sharedir)/rc/extra
	[ -e $(sharedir)/autoload ] || ln -s rc $(sharedir)/autoload
	install -m 0644 ../colors/* $(sharedir)/colors
	install -m 0644 ../README.asciidoc $(docdir)
ifeq ($(gzip_man),yes)
	install -m 0644 ../doc/kak.1.gz $(mandir)
else
	install -m 0644 ../doc/kak.1 $(mandir)
endif

install-strip: install
	strip -s $(bindir)/kak

uninstall:
	rm -rf $(bindir)/kak \
		$(sharedir) \
		$(docdir) \
		$(mandir)/kak.1.gz \
		$(mandir)/kak.1

.PHONY: check TAGS clean dist distclean installdirs install install-strip uninstall
.PHONY: tags test man kak FORCE

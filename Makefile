.POSIX:
.SUFFIXES:
.SUFFIXES: .o .cc

CXX = c++

debug = no
static = no
gzip_man = yes
# to get format compatible with GitHub archive use "gzip -S .gz" here
compress_bin = bzip

compress-suffix-bzip = bz2
compress-suffix-zstd = zst

CPPFLAGS-debug-yes = -DKAK_DEBUG
CXXFLAGS-debug-yes = -O0 -g
suffix-debug-yes = .debug

CXXFLAGS-debug-no = -O3
suffix-debug-no = .opt

CXXFLAGS-sanitize-address = -fsanitize=address
LDFLAGS-sanitize-address = -lasan
suffix-sanitize-address = .san_a

CXXFLAGS-sanitize-undefined = -fsanitize=undefined

LDFLAGS-sanitize-undefined = -lubsan
suffix-sanitize-undefined = .san_u

LDFLAGS-static-yes = -static -pthread

version != cat .version 2>/dev/null || git describe --tags HEAD 2>/dev/null || echo unknown

sources != find src -type f -name '*.cc' | sed -e '/\.version\.cc/d'
deps != find src -type f -name '*.d'
objects = $(sources:.cc=.o)

PREFIX = /usr/local
DESTDIR = # root dir

bindir = $(DESTDIR)$(PREFIX)/bin
libexecdir = $(DESTDIR)$(PREFIX)/libexec/kak
sharedir = $(DESTDIR)$(PREFIX)/share/kak
docdir = $(DESTDIR)$(PREFIX)/share/doc/kak
mandir = $(DESTDIR)$(PREFIX)/share/man/man1

# Both Cygwin and MSYS2 have "_NT" in their uname.
os != uname | sed 's/.*_NT.*/Windows/'

CPPFLAGS-os-Darwin = -I/opt/local/include
LDFLAGS-os-Darwin = -L/opt/local/lib

CPPFLAGS-os-FreeBSD = -I/usr/local/include
LDFLAGS-os-FreeBSD = -L/usr/local/lib

LIBS-os-Haiku = -lnetwork -lbe

CPPFLAGS-os-OpenBSD = -DKAK_BIN_PATH="$(bindir)/kak" -I/usr/local/include
LDFLAGS-os-OpenBSD = -L/usr/local/lib
mandir-os-OpenBSD = $(DESTDIR)$(PREFIX)/man/man1

LDFLAGS-os-SunOS = -lsocket -rdynamic

CPPFLAGS-os-Windows = -D_XOPEN_SOURCE=700
LIBS-os-Windows = -ldbghelp

CXXFLAGS-default = -std=c++2a -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-sign-compare

compiler != $(CXX) --version | grep -E -o 'clang|g\+\+' | head -1
#CXXFLAGS-compiler-clang = -frelaxed-template-template-args -Wno-ambiguous-reversed-operator
#CXXFLAGS-compiler-g++ = -Wno-init-list-lifetime
CXXFLAGS-compiler-clang =
CXXFLAGS-compiler-g++ = -Wno-init-list-lifetime -Wno-stringop-overflow

KAK_CPPFLAGS = \
	$(CPPFLAGS-default) \
	$(CPPFLAGS-debug-$(debug)) \
	$(CPPFLAGS-os-$(os)) \
	$(CPPFLAGS)

KAK_CXXFLAGS = \
	$(CXXFLAGS-default) \
	$(CXXFLAGS-debug-$(debug)) \
	$(CXXFLAGS-sanitize-$(sanitize)) \
	$(CXXFLAGS-compiler-$(compiler)) \
	$(CXXFLAGS)

KAK_LDFLAGS = \
	$(LDFLAGS-default) \
	$(LDFLAGS-sanitize-$(sanitize)) \
	$(LDFLAGS-static-$(static)) \
	$(LDFLAGS-os-$(os)) \
	$(LDFLAGS)

KAK_LIBS = \
	$(LIBS-os-$(os)) \
	$(LIBS)

suffix = $(suffix-debug-$(debug))$(suffix-sanitize-$(sanitize))

all: src/kak

src/kak: src/kak$(suffix)
	ln -sf kak$(suffix) $@

src/kak$(suffix): src/.version.o $(objects)
	$(CXX) $(KAK_LDFLAGS) $(KAK_CXXFLAGS) $(KAK_LIBS) $(objects) src/.version.o -o $@

include $(deps)

.cc.o:
	$(CXX) $(KAK_CPPFLAGS) $(KAK_CXXFLAGS) -MD -MP -MF $*.d -c -o $@ $<

src/.version.cc:
	echo 'namespace Kakoune { const char *version = "$(version)"; }' > $@

src/.version.o: src/.version.cc
	$(CXX) $(KAK_CPPFLAGS) $(KAK_CXXFLAGS) -c -o $@ $<

# Generate the man page
man: gzip-man-$(gzip_man)

gzip-man-yes: doc/kak.1.gz
gzip-man-no: doc/kak.1

doc/kak.1.gz: doc/kak.1
	gzip -n -9 -f < $< > $@

check: test
test: src/kak
	cd test && ./run

TAGS: tags
tags:
	ctags -R

clean:
	rm -f $(objects) $(deps) src/.version*

dist: kakoune-$(version).tar.zst

kakoune-$(version).tar.$(compress-suffix-$(compress_bin)): kakoune-$(version).tar
	$(compress_bin) -f $< -o $@

kakoune-$(version).tar:
	@if ! [ -d .git ]; then echo "make dist can only run from a git repo";  false; fi
	@if git status -s | grep -qEv '^\?\?'; then echo "working tree is not clean";  false; fi
	git archive --format=tar --prefix=$(@:.tar=)/ HEAD -o $@
	echo "$(version)" > src/.version
	tar --transform "s,^,$(@:.tar=)/," -rf $@ src/.version
	rm -f src/.version

distclean: clean
	rm -f src/kak src/kak$(suffix)
	find doc -type f -name '*.gz' -exec rm -f '{}' +

installdirs: installdirs-debug-$(debug)

installdirs-debug-no:
	mkdir -p \
		$(bindir) \
		$(libexecdir) \
		$(sharedir)/rc \
		$(sharedir)/colors \
		$(sharedir)/doc \
		$(docdir) \
		$(mandir)

installdirs-debug-yes: installdirs-debug-no
	mkdir -p $(sharedir)/gdb

install: src/kak installdirs install-debug-$(debug) install-gzip-man-$(gzip_man)
	cp src/kak$(suffix) $(bindir)
	chmod 0755 $(bindir)/kak

	ln -sf ../../bin/kak $(libexecdir)/kak

	cp src/kak/kakrc $(sharedir)
	chmod 0644 $(sharedir)/kakrc

	cp -r rc/* $(sharedir)/rc
	find $(sharedir)/rc -type f -exec chmod 0644 {} +
	[ -e $(sharedir)/autoload ] || ln -s rc $(sharedir)/autoload

	cp colors/* $(sharedir)/colors
	chmod 0644 $(sharedir)/colors/*

	cp doc/pages/*.asciidoc README.asciidoc $(docdir)
	chmod 0644 $(docdir)/*.asciidoc

install-gzip-man-yes: gzip-man-yes
	cp -f doc/kak.1.gz $(mandir)
	chmod 0644 $(mandir)/kak.1.gz

install-gzip-man-no: gzip-man-no
	cp -f doc/kak.1 $(mandir)
	chmod 0644 $(mandir)/kak.1

install-debug-yes: installdirs-debug-yes
	cp -f gdb/kakoune.py $(sharedir)/gdb
	chmod 0644 $(sharedir)/gdb/kakoune.py

install-debug-no: installdirs-debug-no

install-strip: install
	strip -s $(bindir)/kak

uninstall:
	rm -rf \
		$(bindir)/kak \
		$(libexecdir) \
		$(sharedir) \
		$(docdir) \
		$(mandir)/kak.*

debug ?= no
static ?= no
gzip_man ?= yes
# to get format compatible with GitHub archive use "gzip -S .gz" here
compress_bin ?= bzip2

ifneq ($(gzip_man),yes)
    ifneq ($(gzip_man),no)
        $(error gzip_man should be either yes or no)
    endif
endif

ifeq ($(debug),yes)
    CPPFLAGS += -DKAK_DEBUG
    CXXFLAGS += -O0
    suffix := .debug
else
    ifeq ($(debug),no)
        CXXFLAGS += -O3
        suffix := .opt
    else
        $(error debug should be either yes or no)
    endif
endif

ifneq (,$(findstring address,$(sanitize)))
    CXXFLAGS += -fsanitize=address
    LDFLAGS += -lasan
    sanitize_suffix := $(sanitize_suffix)a
endif
ifneq (,$(findstring undefined,$(sanitize)))
    CXXFLAGS += -fsanitize=undefined
    LDFLAGS += -lubsan
    sanitize_suffix := $(sanitize_suffix)u
endif

ifneq (,$(sanitize_suffix))
    suffix := $(suffix).san_$(sanitize_suffix)
endif

version ?= $(shell if [ -f .version ]; then cat .version; elif  [ -d ../.git ]; then git describe --tags HEAD; else echo "unknown"; fi)

sources := $(sort $(wildcard *.cc))
objects := $(addprefix ., $(sources:.cc=$(suffix).o))
deps := $(addprefix ., $(sources:.cc=$(suffix).d))

ifeq ($(static),yes)
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
    CPPFLAGS += -I/opt/local/include
    LDFLAGS += -L/opt/local/lib
else ifeq ($(os),FreeBSD)
    CPPFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
else ifeq ($(os),Haiku)
    LIBS += -lnetwork -lbe
else ifeq ($(os),OpenBSD)
    CPPFLAGS += -D'KAK_BIN_PATH="$(bindir)/kak"' -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
    mandir := $(DESTDIR)$(PREFIX)/man/man1
else ifneq (,$(findstring CYGWIN,$(os)))
    CPPFLAGS += -D_XOPEN_SOURCE=700
    LIBS += -ldbghelp
else
    LDFLAGS += -rdynamic
endif

CXXFLAGS += -pedantic -std=c++17 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-address

compiler := $(shell $(CXX) --version)
ifneq (,$(findstring clang,$(compiler)))
    CXXFLAGS += -frelaxed-template-template-args
else ifneq (,$(findstring g++,$(compiler)))
    CXXFLAGS += -Wno-init-list-lifetime
endif

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
../doc/kak.1.gz: ../doc/kak.1
	gzip -n -9 -f < $< > $@
man: ../doc/kak.1.gz
else
man: ../doc/kak.1
endif

check: test
test: kak
	cd ../test && ./run

TAGS: tags
tags:
	ctags -R

clean:
	rm -f $(objects) $(deps)

dist:
	@if ! [ -d ../.git ]; then echo "make dist can only run from a git repo";  false; fi
	@if git status -s | grep -qEv '^\?\?'; then echo "working tree is not clean";  false; fi
	cd ../;                                                                   \
	basename="kakoune-$$(echo "$(version)" | sed -e s/^v//)";                 \
	git archive --format=tar --prefix=$${basename}/ HEAD -o $${basename}.tar; \
	echo "$(version)" > src/.version;                                         \
	tar --transform "s,^,$${basename}/," -rf $${basename}.tar src/.version;   \
	rm src/.version;                                                          \
	$(compress_bin) $${basename}.tar;

distclean: clean
	rm -f kak kak$(suffix)
	find ../doc -type f \( -name \*\\.gz -o -name \*\\.1 \) -exec rm -f '{}' +

installdirs:
	install -d $(bindir) \
		$(sharedir)/rc \
		$(sharedir)/colors \
		$(sharedir)/doc \
		$(docdir) \
		$(mandir)

install: kak man installdirs
	install -m 0755 kak $(bindir)
	install -m 0644 ../share/kak/kakrc $(sharedir)
	install -m 0644 ../doc/pages/*.asciidoc $(sharedir)/doc
	cp -r ../rc/* $(sharedir)/rc
	find $(sharedir)/rc -type f -exec chmod 0644 {} +
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

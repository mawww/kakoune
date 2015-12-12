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

PREFIX ?= /usr/local
DESTDIR ?= # root dir

NCURSESW_INCLUDE ?= /usr/include/ncursesw

bindir := $(DESTDIR)$(PREFIX)/bin
sharedir := $(DESTDIR)$(PREFIX)/share/kak
docdir := $(DESTDIR)$(PREFIX)/share/doc/kak

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

CXXFLAGS += -std=gnu++11 -g -Wall -Wno-reorder -Wno-sign-compare

kak : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) $(LIBS) -o $@

-include $(deps)

.%$(suffix).o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MD -MP -MF $(addprefix ., $(<:.cc=$(suffix).d)) -c -o $@ $<

test:
	cd ../test && ./run
tags:
	ctags -R

clean:
	rm -f .*.o .*.d kak tags

XDG_CONFIG_HOME ?= $(HOME)/.config

install: kak
	mkdir -p $(bindir)
	install -m 0755 kak $(bindir)
	mkdir -p $(sharedir)/rc
	install -m 0644 ../share/kak/kakrc $(sharedir)
	install -m 0644 ../rc/* $(sharedir)/rc
	[ -e $(sharedir)/autoload ] || ln -s rc $(sharedir)/autoload
	mkdir -p $(sharedir)/colors
	install -m 0644 ../colors/* $(sharedir)/colors
	mkdir -p $(docdir)
	install -m 0644 ../README.asciidoc $(docdir)
	install -m 0644 ../doc/* $(docdir)

.PHONY: tags install

sources := $(wildcard *.cc)
objects := $(addprefix ., $(sources:.cc=.o))
deps := $(addprefix ., $(sources:.cc=.d))

PREFIX ?= /usr/local
DESTDIR ?= # root dir

bindir := $(DESTDIR)$(PREFIX)/bin
sharedir := $(DESTDIR)$(PREFIX)/share/kak
docdir := $(DESTDIR)$(PREFIX)/share/doc/kak

CXXFLAGS += -std=gnu++11 -g -Wall -Wno-reorder -Wno-sign-compare -pedantic
LDFLAGS += -rdynamic

os := $(shell uname)

ifeq ($(os),Darwin)
    LIBS += -lncurses
else
    LIBS += -lncursesw
endif

ifneq (,$(findstring CYGWIN,$(os)))
    LIBS += -lboost_regex
else ifeq ($(os),Darwin)
    LIBS += -lboost_regex-mt
else
    LIBS += -lboost_regex
endif


debug ?= yes
ifeq ($(debug),yes)
    CXXFLAGS += -DKAK_DEBUG
else
    ifeq ($(debug),no)
        CXXFLAGS += -O3
    else
        $(error debug should be either yes or no)
    endif
endif

kak : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) $(LIBS) -o $@

-include $(deps)

.%.o: %.cc
	$(CXX) $(CXXFLAGS) -MD -MP -MF $(addprefix ., $(<:.cc=.d)) -c -o $@ $<

test:
	cd ../test && ./run
tags:
	ctags -R

clean:
	rm -f .*.o .*.d kak tags

XDG_CONFIG_HOME ?= $(HOME)/.config

userconfig:
	mkdir -p $(XDG_CONFIG_HOME)/kak/autoload
	ln -s $(CURDIR)/../rc/*.kak $(XDG_CONFIG_HOME)/kak/autoload/

install: kak
	mkdir -p $(bindir)
	install -m 0755 kak $(bindir)
	mkdir -p $(sharedir)/rc
	install -m 0644 ../share/kak/kakrc $(sharedir)
	install -m 0644 ../rc/* $(sharedir)/rc
	mkdir -p $(docdir)
	install -m 0644 ../README.asciidoc $(docdir)
	install -m 0644 ../doc/* $(docdir)

.PHONY: tags userconfig install

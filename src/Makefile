sources := $(wildcard *.cc)
objects := $(addprefix ., $(sources:.cc=.o))
deps := $(addprefix ., $(sources:.cc=.d))

PREFIX ?= /usr/local
DESTDIR ?= /

bindir := $(DESTDIR)$(PREFIX)/bin
sharedir := $(DESTDIR)$(PREFIX)/share/kak
docdir := $(DESTDIR)$(PREFIX)/share/doc/kak

CXXFLAGS += -std=gnu++11 -g -Wall -Wno-reorder -Wno-sign-compare -pedantic
LIBS += -lncursesw

os := $(shell uname -o)
ifeq ($(os),Cygwin)
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
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(addprefix ., $(<:.cc=.d)) -c -o $@ $<

tags:
	ctags -R

clean:
	rm -f .*.o .*.d kak tags

XDG_CONFIG_HOME ?= $(HOME)/.config

userconfig:
	mkdir -p $(XDG_CONFIG_HOME)/kak/autoload
	ln -s $(CURDIR)/../rc/{asciidoc,client,cpp,diff,git,grep,kakrc,mime,make,sh,mail,man}.kak $(XDG_CONFIG_HOME)/kak/autoload/

install: kak
	mkdir -p $(bindir)
	install -D -m 0755 kak $(bindir)
	mkdir -p $(sharedir)/rc
	install -D -m 0644 ../share/kak/kakrc $(sharedir)
	install -D -m 0644 ../rc/* $(sharedir)/rc
	mkdir -p $(docdir)
	install -D -m 0644 ../README.asciidoc $(docdir)
	install -D -m 0644 ../doc/* $(docdir)

.PHONY: tags userconfig install

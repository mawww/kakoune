sources := $(wildcard *.cc)
objects := $(sources:.cc=.o)
deps := $(addprefix ., $(sources:.cc=.d))

CXXFLAGS += -std=c++0x -g -Wall -Wno-reorder -Wno-sign-compare
LDFLAGS += -lmenu -lncurses -lboost_regex

kak : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) -o $@

-include $(deps)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -MP -MF $(addprefix ., $(<:.cc=.d)) -c -o $@ $<

tags:
	ctags -R

clean:
	rm -f *.o .*.d kak tags

XDG_CONFIG_HOME ?= $(HOME)/.config

userconfig:
	mkdir -p $(XDG_CONFIG_HOME)/kak/autoload
	ln -rs rc/{asciidoc,cpp,diff,git,grep,kakrc,make}.kak $(XDG_CONFIG_HOME)/kak/autoload/

.PHONY: tags userconfig

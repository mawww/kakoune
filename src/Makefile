sources := $(wildcard *.cc)
objects := $(addprefix ., $(sources:.cc=.o))
deps := $(addprefix ., $(sources:.cc=.d))

CXXFLAGS += -std=c++11 -g -Wall -Wno-reorder -Wno-sign-compare -pedantic
LIBS += -lmenu -lncursesw -lboost_regex

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
	ln -s $(CURDIR)/rc/{asciidoc,client,cpp,diff,git,grep,kakrc,make,sh,mail}.kak $(XDG_CONFIG_HOME)/kak/autoload/

.PHONY: tags userconfig

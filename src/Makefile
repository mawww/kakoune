sources := $(wildcard *.cc)
objects := $(sources:.cc=.o)
deps := $(addprefix ., $(sources:.cc=.d))

CXXFLAGS += -std=c++0x -g
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

.PHONY: tags

sources := $(wildcard *.cc)
objects := $(sources:.cc=.o)
deps := $(addprefix ., $(sources:.cc=.d))

CXXFLAGS += -std=c++0x -g
LDFLAGS += -lncurses -lboost_regex

kak : $(objects)
	$(CXX) $(LDFLAGS) $(CXXFLAGS) $(objects) -o $@

include $(deps)

.%.d: %.cc
	$(CXX) $(CXXFLAGS) -MM $< -o $@

tags:
	ctags -R

clean :
	rm -f *.o .*.d kak tags

# This file will be included by the Makefile

# change to no to compile in release mode
debug ?= yes

XDG_CONFIG_HOME ?= $(HOME)/.config

# kakoune will be installed in $(DESTDIR)$(PREFIX)
DESTDIR ?= # root dir
PREFIX ?= /usr/local

# path of ncursesw headers
NCURSESW_INCLUDE ?= /usr/include/ncursesw

#uncomment the following lines to compile with clang
#CXX=clang++
#CXXFLAGS += -Wno-unknown-attributes

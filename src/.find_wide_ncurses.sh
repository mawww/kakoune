#!/bin/sh

# Finds wide ncurses and defines a macro:
#   FOUND_NCURSESW_NCURSES_H
#   FOUND_NCURSESW_H
#   FOUND_NCURSES_H


printf '%s\n%s\n\0' '#include <ncursesw/ncurses.h>' \
       'int main() { return 0; }' | ${CXX} -xc++ -o /dev/null - 2>/dev/null
if test "$?" -eq 0 ; then
  printf ' %s \0' '-DFOUND_NCURSESW_NCURSES_H=1'
else
  printf '%s\n%s\n\0' '#include <ncursesw.h>' \
         'int main() { return 0; }' | ${CXX} -xc++ -o /dev/null - 2>/dev/null
  if test "$?" -eq 0 ; then
    printf ' %s \0' '-DFOUND_NCURSESW_H=1'
  else
    printf '%s\n%s\n\0' '#include <ncurses.h>' \
           'int main() { return 0; }' | ${CXX} -xc++ -o /dev/null - 2>/dev/null
    if test "$?" -eq 0 ; then
      printf ' %s \0' '-DFOUND_NCURSES_H=1'
    fi
  fi
fi


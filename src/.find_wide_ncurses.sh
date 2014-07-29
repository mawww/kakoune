#!/bin/sh

# Finds wide ncurses and defines a macro:
#   FOUND_NCURSESW_NCURSES_H
#   FOUND_NCURSESW_H
#   FOUND_NCURSES_H


${CXX} -xc++ -o /dev/null - <<__EOF__ 2>/dev/null
#include <ncursesw/ncurses.h>
int main()
{
 return 0;
}
__EOF__
if test "$?" -eq 0 ; then
  printf ' %s \0' '-DFOUND_NCURSESW_NCURSES_H=1'
else
  ${CXX} -xc++ -o /dev/null - <<__EOF__ 2>/dev/null
#include <ncursesw.h>
int main()
{
 return 0;
}
__EOF__
  if test "$?" -eq 0 ; then
    printf ' %s \0' '-DFOUND_NCURSESW_H=1'
  else
    ${CXX} -xc++ -o /dev/null - <<__EOF__ 2>/dev/null
#include <ncurses.h>
int main()
{
 return 0;
}
__EOF__
    if test "$?" -eq 0 ; then
      printf ' %s \0' '-DFOUND_NCURSES_H=1'
    fi
  fi
fi


#.rst:
# FindCurses
# ----------
#
# Find the curses or ncurses include file and library.
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# This module defines the following variables:
#
# ``CURSES_FOUND``
#   True if Curses is found.
# ``CURSES_INCLUDE_DIRS``
#   The include directories needed to use Curses.
# ``CURSES_LIBRARIES``
#   The libraries needed to use Curses.
# ``CURSES_HAVE_CURSES_H``
#   True if curses.h is available.
# ``CURSES_HAVE_NCURSESW_H``
#   True if ncurses.h is available.
# ``CURSES_HAVE_NCURSESW_NCURSESW_H``
#   True if ``ncursesw/ncurses.h`` is available.
# ``CURSES_HAVE_NCURSESW_CURSES_H``
#   True if ``ncursesw/curses.h`` is available.
#
# Set ``CURSES_NEED_NCURSESW`` to ``TRUE`` before the
# ``find_package(Curses)`` call if NCurses functionality is required.
#
# Backward Compatibility
# ^^^^^^^^^^^^^^^^^^^^^^
#
# The following variable are provided for backward compatibility:
#
# ``CURSES_INCLUDE_DIR``
#   Path to Curses include.  Use ``CURSES_INCLUDE_DIRS`` instead.
# ``CURSES_LIBRARY``
#   Path to Curses library.  Use ``CURSES_LIBRARIES`` instead.

#=============================================================================
# Copyright 2001-2014 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

include(CheckLibraryExists)

find_library(CURSES_CURSES_LIBRARY NAMES curses )

find_library(CURSES_NCURSESW_LIBRARY NAMES ncursesw )
set(CURSES_USE_NCURSESW FALSE)

if(CURSES_NCURSESW_LIBRARY  AND ((NOT CURSES_CURSES_LIBRARY) OR CURSES_NEED_NCURSESW))
  set(CURSES_USE_NCURSESW TRUE)
endif()
# http://cygwin.com/ml/cygwin-announce/2010-01/msg00002.html
# cygwin ncurses stopped providing curses.h symlinks see above
# message.  Cygwin is an ncurses package, so force ncurses on
# cygwin if the curses.h is missing
if(CYGWIN)
  if(NOT EXISTS /usr/include/curses.h)
    set(CURSES_USE_NCURSESW TRUE)
  endif()
endif()


# Not sure the logic is correct here.
# If NCurses is required, use the function wsyncup() to check if the library
# has NCurses functionality (at least this is where it breaks on NetBSD).
# If wsyncup is in curses, use this one.
# If not, try to find ncurses and check if this has the symbol.
# Once the ncurses library is found, search the ncurses.h header first, but
# some web pages also say that even with ncurses there is not always a ncurses.h:
# http://osdir.com/ml/gnome.apps.mc.devel/2002-06/msg00029.html
# So at first try ncurses.h, if not found, try to find curses.h under the same
# prefix as the library was found, if still not found, try curses.h with the
# default search paths.
if(CURSES_CURSES_LIBRARY  AND  CURSES_NEED_NCURSESW)
  include(CMakePushCheckState)
  cmake_push_check_state()
  set(CMAKE_REQUIRED_QUIET ${Curses_FIND_QUIETLY})
  CHECK_LIBRARY_EXISTS("${CURSES_CURSES_LIBRARY}"
    wsyncup "" CURSES_CURSES_HAS_WSYNCUP)

  if(CURSES_NCURSESW_LIBRARY  AND NOT  CURSES_CURSES_HAS_WSYNCUP)
    CHECK_LIBRARY_EXISTS("${CURSES_NCURSESW_LIBRARY}"
      wsyncup "" CURSES_NCURSESW_HAS_WSYNCUP)
    if( CURSES_NCURSESW_HAS_WSYNCUP)
      set(CURSES_USE_NCURSESW TRUE)
    endif()
  endif()
  cmake_pop_check_state()

endif()

if(CURSES_USE_NCURSESW)
  get_filename_component(_cursesLibDir "${CURSES_NCURSESW_LIBRARY}" PATH)
  get_filename_component(_cursesParentDir "${_cursesLibDir}" PATH)

  # Use CURSES_NCURSESW_INCLUDE_PATH if set, for compatibility.
  if(CURSES_NCURSESW_INCLUDE_PATH)
    find_path(CURSES_INCLUDE_PATH
      NAMES ncursesw/ncurses.h ncursesw/curses.h ncurses.h curses.h
      PATHS ${CURSES_NCURSESW_INCLUDE_PATH}
      NO_DEFAULT_PATH
      )
  endif()

  find_path(CURSES_INCLUDE_PATH
    NAMES ncursesw/ncurses.h ncursesw/curses.h ncurses.h curses.h
    HINTS "${_cursesParentDir}/include"
    )

  # Previous versions of FindCurses provided these values.
  if(NOT DEFINED CURSES_LIBRARY)
    set(CURSES_LIBRARY "${CURSES_NCURSESW_LIBRARY}")
  endif()

  CHECK_LIBRARY_EXISTS("${CURSES_NCURSESW_LIBRARY}"
    cbreak "" CURSES_NCURSESW_HAS_CBREAK)
  if(NOT CURSES_NCURSESW_HAS_CBREAK)
    find_library(CURSES_EXTRA_LIBRARY tinfo HINTS "${_cursesLibDir}")
    find_library(CURSES_EXTRA_LIBRARY tinfo )
  endif()
else()
  get_filename_component(_cursesLibDir "${CURSES_CURSES_LIBRARY}" PATH)
  get_filename_component(_cursesParentDir "${_cursesLibDir}" PATH)

  find_path(CURSES_INCLUDE_PATH
    NAMES curses.h
    HINTS "${_cursesParentDir}/include"
    )

  # Previous versions of FindCurses provided these values.
  if(NOT DEFINED CURSES_CURSES_H_PATH)
    set(CURSES_CURSES_H_PATH "${CURSES_INCLUDE_PATH}")
  endif()
  if(NOT DEFINED CURSES_LIBRARY)
    set(CURSES_LIBRARY "${CURSES_CURSES_LIBRARY}")
  endif()
endif()

# Report whether each possible header name exists in the include directory.
if(NOT DEFINED CURSES_HAVE_NCURSESW_NCURSESW_H)
  if(EXISTS "${CURSES_INCLUDE_PATH}/ncursesw/ncurses.h")
    set(CURSES_HAVE_NCURSESW_NCURSESW_H "${CURSES_INCLUDE_PATH}/ncursesw/ncurses.h")
  else()
    set(CURSES_HAVE_NCURSESW_NCURSESW_H "CURSES_HAVE_NCURSESW_NCURSESW_H-NOTFOUND")
  endif()
endif()
if(NOT DEFINED CURSES_HAVE_NCURSESW_CURSES_H)
  if(EXISTS "${CURSES_INCLUDE_PATH}/ncursesw/curses.h")
    set(CURSES_HAVE_NCURSESW_CURSES_H "${CURSES_INCLUDE_PATH}/ncursesw/curses.h")
  else()
    set(CURSES_HAVE_NCURSESW_CURSES_H "CURSES_HAVE_NCURSESW_CURSES_H-NOTFOUND")
  endif()
endif()
if(NOT DEFINED CURSES_HAVE_NCURSESW_H)
  if(EXISTS "${CURSES_INCLUDE_PATH}/ncurses.h")
    set(CURSES_HAVE_NCURSESW_H "${CURSES_INCLUDE_PATH}/ncurses.h")
  else()
    set(CURSES_HAVE_NCURSESW_H "CURSES_HAVE_NCURSESW_H-NOTFOUND")
  endif()
endif()
if(NOT DEFINED CURSES_HAVE_CURSES_H)
  if(EXISTS "${CURSES_INCLUDE_PATH}/curses.h")
    set(CURSES_HAVE_CURSES_H "${CURSES_INCLUDE_PATH}/curses.h")
  else()
    set(CURSES_HAVE_CURSES_H "CURSES_HAVE_CURSES_H-NOTFOUND")
  endif()
endif()

find_library(CURSES_FORM_LIBRARY form HINTS "${_cursesLibDir}")
find_library(CURSES_FORM_LIBRARY form )

# Previous versions of FindCurses provided these values.
if(NOT DEFINED FORM_LIBRARY)
  set(FORM_LIBRARY "${CURSES_FORM_LIBRARY}")
endif()

# Need to provide the *_LIBRARIES
set(CURSES_LIBRARIES ${CURSES_LIBRARY})

if(CURSES_EXTRA_LIBRARY)
  set(CURSES_LIBRARIES ${CURSES_LIBRARIES} ${CURSES_EXTRA_LIBRARY})
endif()

if(CURSES_FORM_LIBRARY)
  set(CURSES_LIBRARIES ${CURSES_LIBRARIES} ${CURSES_FORM_LIBRARY})
endif()

# Provide the *_INCLUDE_DIRS result.
set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_PATH})
set(CURSES_INCLUDE_DIR ${CURSES_INCLUDE_PATH}) # compatibility

# handle the QUIETLY and REQUIRED arguments and set CURSES_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Ncursesw DEFAULT_MSG
  CURSES_LIBRARY CURSES_INCLUDE_PATH)

mark_as_advanced(
  CURSES_INCLUDE_PATH
  CURSES_CURSES_LIBRARY
  CURSES_NCURSESW_LIBRARY
  CURSES_EXTRA_LIBRARY
  )

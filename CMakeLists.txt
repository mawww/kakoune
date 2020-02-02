cmake_minimum_required(VERSION 3.11)
project(Kakoune LANGUAGES CXX)

# prerequisites
set(CMAKE_CXX_STANDARD 17)

# Version
option(MANUAL_VERSION "Use a manually defined version instead of the date" FALSE)
mark_as_advanced(MANUAL_VERSION)
if(${MANUAL_VERSION})
  set(KAKOUNE_VERSION "0.0" CACHE STRING "Kakoune version")
else()
  string(TIMESTAMP KAKOUNE_VERSION "%Y.%m.%d")
endif()
project(Kakoune VERSION ${KAKOUNE_VERSION})

# Global options
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release'.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()
option(BUILD_SHARED_LIBS "Use static or shared libs" TRUE)

# create the kak target
add_executable(kak)
add_subdirectory(src)
# speial case of the version.cc.in file
configure_file(src/version.cc.in src/version.cc)
list(APPEND KAKOUNE_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/src/version.cc)
# add cc files
target_sources(kak PRIVATE ${KAKOUNE_SOURCES})
install(TARGETS kak)

# update compile flags for kak
if(CMAKE_BUILD_TYPE MATCHES Debug)
  target_compile_definitions(kak PRIVATE KAK_DEBUG)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(kak PRIVATE "-Wno-init-list-lifetime")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(kak PRIVATE "-frelaxed-template-template-args")
endif()

# dependencies of kak
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
# target_include_directories(kak PUBLIC ${CURSES_INCLUDE_DIR})
# target_link_libraries(kak ${CURSES_LIBRARIES})
# Force flag (avoid bug on Gentoo)
if (CURSES_HAVE_NCURSES_NCURSES_H)
  target_link_options(kak PRIVATE "-lncursesw")
endif()
if (CURSES_EXTRA_LIBRARY)
  # generalize
  target_link_options(kak PRIVATE "-ltinfow")
endif()



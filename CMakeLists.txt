cmake_minimum_required(VERSION 3.11)
project(Kakoune LANGUAGES CXX)

# Prerequisites
set(CMAKE_CXX_STANDARD 17)

# Kakoune version
option(MANUAL_VERSION "Use a manually defined version instead of the date" FALSE)
mark_as_advanced(MANUAL_VERSION)
if(${MANUAL_VERSION})
  set(KAKOUNE_VERSION "0.0" CACHE STRING "Kakoune version")
else()
  string(TIMESTAMP KAKOUNE_VERSION "%Y.%m.%d")
endif()
project(Kakoune VERSION ${KAKOUNE_VERSION})

# Configuration variables
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release'.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()
option(BUILD_SHARED_LIBS "Use static or shared libs" TRUE)

# Compile files and main target
add_executable(kak)
add_subdirectory(src)
# speial case of the version.cc.in file
configure_file(src/version.cc.in src/version.cc)
list(APPEND KAKOUNE_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/src/version.cc)
# add cc files
target_sources(kak PRIVATE ${KAKOUNE_SOURCES})

# Compile flags for kak
if(CMAKE_BUILD_TYPE MATCHES Debug)
  target_compile_definitions(kak PRIVATE KAK_DEBUG)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(kak PRIVATE "-Wno-init-list-lifetime")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(kak PRIVATE "-frelaxed-template-template-args")
endif()

# Dependencies of kak
set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
# cmake way of linking against ncurse leads to bug because of the badly
# maintained FindCurses.cmake.
# Bug is: segfault on Gentoo systems
# target_link_libraries(kak ${CURSES_LIBRARY})
# if(CURSES_EXTRA_LIBRARY)
#   target_link_libraries(kak ${CURSES_EXTRA_LIBRARY})
# endif()
# We force flag manually instead:
if (CURSES_HAVE_NCURSES_NCURSES_H)
  target_link_options(kak PRIVATE "-lncursesw")
  if (CURSES_EXTRA_LIBRARY)
    target_link_options(kak PRIVATE "-ltinfow")
  endif()
else()
  target_link_options(kak PRIVATE "-lncurses")
  if (CURSES_EXTRA_LIBRARY)
    target_link_options(kak PRIVATE "-ltinfo")
  endif()
endif()

# Install binary and files
install(TARGETS kak)
# share folder
install(FILES share/kak/kakrc DESTINATION share/kak)
install(DIRECTORY colors/ DESTINATION share/kak/colors/)
install(DIRECTORY rc/ DESTINATION share/kak/rc/)
# share/kak/autoload/ is symlink to rc/
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E \
                create_symlink rc/ ${CMAKE_INSTALL_PREFIX}/share/kak/autoload \
              )"
)
# (TODO need to compress)
install(FILES doc/kak.1 DESTINATION share/man/man1/)
install(FILES README.asciidoc DESTINATION share/doc/kak/)
install(DIRECTORY doc/pages/ DESTINATION share/kak/doc/)

# Package configuration
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Selection-oriented code editor inspired by vim")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.asciidoc")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/UNLICENSE")
# Package generation
include(CPack)

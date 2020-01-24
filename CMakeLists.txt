cmake_minimum_required(VERSION 3.11)
project(Kakoune VERSION 0.1 LANGUAGES CXX)

# prerequisites
set(CMAKE_CXX_STANDARD 17)

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
set(KAKOUNE_VERSION ${PROJECT_VERSION})
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
set(CURSES_NEED_NCURSES TRUE)
# set(CURSES_NEED_WIDE TRUE)
find_package(Curses REQUIRED)
target_include_directories(kak PUBLIC ${CURSES_INCLUDE_DIR})
target_link_libraries(kak ${CURSES_LIBRARIES})


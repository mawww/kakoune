cmake_minimum_required(VERSION 2.6)
project(kakoune)

set(kakoune_SOURCE_DIR ../src)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(BIN_DIR ${kakoune_SOURCE_DIR}/bin)

# Bump up warning levels appropriately for clang+gcc and build in debug mode
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")

if(KAK_DEBUG)
	add_definitions(-DKAK_DEBUG)
endif()

set(Boost_USE_STATIC_LIBS OFF) 
set(Boost_USE_MULTITHREADED ON)  
set(Boost_USE_STATIC_RUNTIME OFF) 

find_package(Boost 1.45.0 COMPONENTS regex) 

set(CURSES_NEED_NCURSESW TRUE)
find_package(NCursesw REQUIRED)

include_directories("${kakoune_SOURCE_DIR}")
file (GLOB kakoune_SOURCES ${kakoune_SOURCE_DIR}/*.cc)

include_directories(${Boost_INCLUDE_DIRS} ${CURSES_INCLUDE_DIR})

add_executable(kak ${kakoune_SOURCES})
target_link_libraries(kak PRIVATE ${Boost_LIBRARIES} ${CURSES_LIBRARY})
install(TARGETS kak RUNTIME DESTINATION ${BIN_DIR})

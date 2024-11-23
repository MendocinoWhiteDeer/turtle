cmake_minimum_required(VERSION 3.20)
project(turtle C)

# default to release build if unspecified
if(NOT CMAKE_BUILD_TYPE)
 set(CMAKE_BUILD_TYPE Release)
endif()
message("Build type: ${CMAKE_BUILD_TYPE}")

# compile flags
set(CMAKE_C_FLAGS_DEBUG "-g -Wall -DTURTLE_DEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3")

add_executable(turtle "turtle.c")

add_subdirectory(bdwgc)
target_link_libraries(turtle PRIVATE gc)

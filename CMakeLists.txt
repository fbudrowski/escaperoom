cmake_minimum_required(VERSION 3.13)
project(escaperoom C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall")


add_library(err err.c)

# http://stackoverflow.com/questions/10555706/
macro (add_executable _name)
    # invoke built-in add_executable
    _add_executable(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} err)
    endif()
endmacro()

add_executable(manager manager.c player.c)
target_link_libraries(manager rt pthread)

cmake_minimum_required(VERSION 3.10)
project(escaperoom C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall")



add_executable(manager manager.c player.c storage.h)
target_link_libraries(manager rt pthread)

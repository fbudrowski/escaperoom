
cmake_minimum_required(VERSION 3.10)
project(escaperoom C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-g -Wall")



add_executable(manager manager.c player.c storage.c)
target_link_libraries(manager rt pthread)
add_executable(player playerExec.c player.c storage.c)
target_link_libraries(player rt pthread)

cmake_minimum_required(VERSION 2.6)
project(vimesd)

set(CMAKE_C_FLAGS "-Wall -std=c99")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
find_package (Threads)
add_executable(client client.c)
add_executable(server server.c)
target_link_libraries (server ${CMAKE_THREAD_LIBS_INIT})

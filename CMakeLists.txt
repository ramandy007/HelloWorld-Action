cmake_minimum_required(VERSION 3.16)

project(main)

add_executable(main main.cpp)
add_executable(main malicious.c)

install(TARGETS main)

enable_testing()
add_test(NAME main COMMAND main)

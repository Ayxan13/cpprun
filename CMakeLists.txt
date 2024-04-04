cmake_minimum_required(VERSION 3.27)

project(
  cpprun
  VERSION 1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
add_executable(
        cpprun
        main.cpp
        ScopeExit.h)
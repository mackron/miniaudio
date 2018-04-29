@echo off
SET c_compiler=gcc
SET cpp_compiler=g++
@echo on

%c_compiler% mal_test_0.c   -o ./bin/mal_test_0.exe     -Wall -mavx
%cpp_compiler% mal_test_0.cpp -o ./bin/mal_test_0_cpp.exe -Wall -mavx
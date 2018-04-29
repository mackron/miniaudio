@echo off
SET c_compiler=gcc
SET cpp_compiler=g++
SET options=-Wall -mavx
@echo on

%c_compiler% mal_test_0.c    -o ./bin/mal_test_0.exe        %options%
%cpp_compiler% mal_test_0.cpp  -o ./bin/mal_test_0_cpp.exe    %options%

%c_compiler% mal_profiling.c -o ./bin/mal_profiling.exe     %options%
%cpp_compiler% mal_profiling.c -o ./bin/mal_profiling_cpp.exe %options%

%c_compiler% mal_dithering.c -o ./bin/mal_dithering.exe     %options%
%cpp_compiler% mal_dithering.c -o ./bin/mal_dithering_cpp.exe %options%
@echo off
SET c_compiler=gcc
SET cpp_compiler=g++
SET options=-Wall
@echo on

%c_compiler% ma_test_0.c    -o ./bin/ma_test_0.exe        %options%
%cpp_compiler% ma_test_0.cpp  -o ./bin/ma_test_0_cpp.exe    %options%

%c_compiler% ma_profiling.c -o ./bin/ma_profiling.exe     %options% -s -O2
%cpp_compiler% ma_profiling.c -o ./bin/ma_profiling_cpp.exe %options% -s -O2

%c_compiler% ma_dithering.c -o ./bin/ma_dithering.exe     %options%
%cpp_compiler% ma_dithering.c -o ./bin/ma_dithering_cpp.exe %options%
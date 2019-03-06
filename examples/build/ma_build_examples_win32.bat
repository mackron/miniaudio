@echo off
SET c_compiler=gcc
SET cpp_compiler=g++
SET options=-Wall
@echo on

%c_compiler% ../simple_playback.c    -o ../bin/simple_playback.exe    %options%
%c_compiler% ../simple_capture.c     -o ../bin/simple_capture.exe     %options%
%c_compiler% ../simple_enumeration.c -o ../bin/simple_enumeration.exe %options%
%c_compiler% ../advanced_config.c    -o ../bin/advanced_config.exe    %options%
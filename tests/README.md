Building
========
Build and run these test from this folder. Example:

    clear && ./ma_build_tests_linux && ./bin/ma_test_0
    
These tests load resources from hard coded paths which point to the "res" folder. These
paths are based on the assumption that the current directory is where the build files
are located.

Emscripten
----------
On Windows, you need to move into this directory and run emsdk_env.bat from a command
prompt using an absolute path like "C:\emsdk\emsdk_env.bat". Note that PowerShell doesn't
work for me for some reason. Then, run the relevant batch file:

    ma_build_tests_emscripten.bat
    
The output will be placed in the bin folder. If you have output WASM it may not work when
running the web page locally. To test you can run with something like this:

    emrun bin/ma_test_0_emscripten.html
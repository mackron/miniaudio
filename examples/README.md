To compile these examples, cd into the "build" directory and run the applicable build script. Executables
will be placed in the "bin" directory.

    cd build
    ./ma_build_examples_linux
    
Then you can run executables like this:

    ../bin/simple_playback my_sound.wav
    
Emscripten
----------
On Windows, you need to move into the build and run emsdk_env.bat from a command prompt using an absolute
path like "C:\emsdk\emsdk_env.bat". Note that PowerShell doesn't work for me for some reason. Then, run the
relevant batch file:

    ma_build_examples_emscripten.bat
    
The output will be placed in the bin folder. If you have output WASM it may not work when running the web
page locally. To test you can run with something like this:

    emrun ../bin/simple_playback_emscripten.html
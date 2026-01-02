Building
========
Build and run from this directory. Example:

    gcc ../test_deviceio/ma_test_deviceio.c -o bin/test_deviceio -ldl -lm -lpthread -Wall -Wextra -Wpedantic -std=c89
    ./bin/test_deviceio
    
Output files will be placed in the "res/output" folder.


Emscripten
----------
On Linux, do `source ~/emsdk/emsdk_env.sh` before compiling.

On Windows, you need to move into the build and run emsdk_env.bat from a command prompt using an absolute
path like "C:\emsdk\emsdk_env.bat". Note that PowerShell doesn't work for me for some reason. Example:

    emcc ../emscripten/emscripten.c -o bin/emscripten.html -sAUDIO_WORKLET=1 -sWASM_WORKERS=1 -DMA_ENABLE_AUDIO_WORKLETS -Wall -Wextra
    
If you output WASM it may not work when running the web page locally. To test you can run with something
like this:

    emrun ./bin/emscripten.html

If you want to see stdout on the command line when running from emrun, add `--emrun` to your emcc command.

To use with CMake, you can do something like this:

    emcmake cmake -S ../../ -B cmake-emcc -DCMAKE_BUILD_TYPE=Debug -DMINIAUDIO_NO_LIBVORBIS=Yes -DMINIAUDIO_NO_LIBOPUS=Yes -DMINIAUDIO_BUILD_TESTS=Yes -DMINIAUDIO_BUILD_EXAMPLES=No

Then to compile with CMake:

    cmake --build cmake-emcc -j

To do a clean rebuild:

    cmake --build cmake-emcc -j --clean-first
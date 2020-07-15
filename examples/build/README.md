Examples
--------
    gcc ../simple_playback.c -o bin/simple_playback -ldl -lm -lpthread
    gcc ../simple_playback.c -o bin/simple_playback -ldl -lm -lpthread -Wall -Wextra -Wpedantic -std=c89
    
Emscripten
----------
On Windows, you need to move into the build and run emsdk_env.bat from a command prompt using an absolute
path like "C:\emsdk\emsdk_env.bat". Note that PowerShell doesn't work for me for some reason. Examples:

    emcc ../simple_playback_sine.c -o bin/simple_playback_sine.html
    emcc ../simple_playback_sine.c -o bin/simple_playback_sine.html -s WASM=0 -Wall -Wextra
    
If you output WASM it may not work when running the web page locally. To test you can run with something
like this:

    emrun ./bin/simple_playback_sine.html
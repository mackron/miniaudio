emcc ./mal_test_0.c -o ./bin/mal_test_0_emscripten.html -s WASM=0 -std=c99
emcc ./mal_duplex.c -o ./bin/mal_duplex.html            -s WASM=0 -std=c99
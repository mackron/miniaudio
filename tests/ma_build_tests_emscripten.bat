emcc ./ma_test_0.c -o ./bin/ma_test_0_emscripten.html -s WASM=0 -std=c99
emcc ./ma_duplex.c -o ./bin/ma_duplex.html            -s WASM=0 -std=c99
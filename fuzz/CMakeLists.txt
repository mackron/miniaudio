cmake_minimum_required(VERSION 3.10)
project(miniaudio_fuzz C)

# Build miniaudio as a library
add_library(miniaudio_fuzz_lib STATIC ../miniaudio.c)
target_include_directories(miniaudio_fuzz_lib PUBLIC ${CMAKE_SOURCE_DIR}/..)
target_compile_definitions(miniaudio_fuzz_lib PRIVATE
    MA_NO_DEVICE_IO
    MA_NO_THREADING
    MA_NO_GENERATION
    MA_NO_ENGINE
)

# Decoder fuzzer
add_executable(fuzz_decoder fuzz_decoder.c)
target_link_libraries(fuzz_decoder miniaudio_fuzz_lib m)
target_compile_definitions(fuzz_decoder PRIVATE
    MA_NO_DEVICE_IO
    MA_NO_THREADING
    MA_NO_GENERATION
    MA_NO_ENGINE
)

if(DEFINED ENV{LIB_FUZZING_ENGINE})
    target_link_libraries(fuzz_decoder $ENV{LIB_FUZZING_ENGINE})
else()
    target_compile_options(fuzz_decoder PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_decoder PRIVATE -fsanitize=fuzzer,address,undefined)
endif()

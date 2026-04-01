/*
 * Fuzz target for miniaudio decoder.
 *
 * Tests decoding of untrusted audio data (WAV, MP3, FLAC, Vorbis) from memory.
 * Exercises the full decode pipeline: format detection, header parsing, and
 * PCM frame decoding with seeking.
 *
 * Build with:
 *   clang -fsanitize=fuzzer,address,undefined -DMA_NO_DEVICE_IO -DMA_NO_THREADING \
 *     -DMA_NO_GENERATION -DMA_NO_ENGINE -I.. fuzz_decoder.c ../miniaudio.c -o fuzz_decoder -lm
 */

#include "miniaudio.h"

#include <stdint.h>
#include <stddef.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4 || size > 4 * 1024 * 1024) {
        return 0;
    }

    ma_decoder decoder;
    ma_decoder_config config;
    ma_result result;

    /* Test 1: Decode with default config (auto-detect format) */
    config = ma_decoder_config_init_default();
    result = ma_decoder_init_memory(data, size, &config, &decoder);
    if (result == MA_SUCCESS) {
        ma_uint8 pcmBuffer[4096 * 4];
        ma_uint64 framesRead = 0;

        ma_decoder_read_pcm_frames(&decoder, pcmBuffer, 1024, &framesRead);

        ma_decoder_seek_to_pcm_frame(&decoder, 0);
        ma_decoder_read_pcm_frames(&decoder, pcmBuffer, 512, &framesRead);

        ma_format format;
        ma_uint32 channels, sampleRate;
        ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate,
                                   NULL, 0);

        ma_uint64 length;
        ma_decoder_get_length_in_pcm_frames(&decoder, &length);

        ma_uint64 cursor;
        ma_decoder_get_cursor_in_pcm_frames(&decoder, &cursor);

        ma_decoder_uninit(&decoder);
    }

    /* Test 2: Decode with explicit output format (s16, mono, 44100) */
    config = ma_decoder_config_init(ma_format_s16, 1, 44100);
    result = ma_decoder_init_memory(data, size, &config, &decoder);
    if (result == MA_SUCCESS) {
        ma_int16 pcmBuffer[2048];
        ma_uint64 framesRead = 0;

        ma_decoder_read_pcm_frames(&decoder, pcmBuffer, 2048, &framesRead);

        ma_uint64 length = 0;
        ma_decoder_get_length_in_pcm_frames(&decoder, &length);
        if (length > 0) {
            ma_decoder_seek_to_pcm_frame(&decoder, length / 2);
            ma_decoder_read_pcm_frames(&decoder, pcmBuffer, 1024, &framesRead);
        }

        ma_decoder_uninit(&decoder);
    }

    /* Test 3: Decode with f32 stereo 48kHz output */
    config = ma_decoder_config_init(ma_format_f32, 2, 48000);
    result = ma_decoder_init_memory(data, size, &config, &decoder);
    if (result == MA_SUCCESS) {
        float pcmBuffer[4096];
        ma_uint64 framesRead = 0;

        ma_decoder_read_pcm_frames(&decoder, pcmBuffer, 1024, &framesRead);
        ma_decoder_uninit(&decoder);
    }

    return 0;
}

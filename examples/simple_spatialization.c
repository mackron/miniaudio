/*
Demonstrates how to do basic spatialization via the high level API.

You can position and orientate sounds to create a simple spatialization effect. This example shows
how to do this.

In addition to positioning sounds, there is the concept of a listener. This can also be positioned
and orientated to help with spatialization.

This example only covers the basics to get your started. See the documentation for more detailed
information on the available features.

To use this example, pass in the path of a sound as the first argument. The sound will be
positioned in front of the listener, while the listener rotates on the the spot to create an
orbiting effect. Terminate the program with Ctrl+C.
*/
#include "../miniaudio.c"

#include <stdio.h>
#include <math.h>   /* For sinf() and cosf() */

/* Silence warning about unreachable code for MSVC. */
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4702)
#endif

int main(int argc, char** argv)
{
    ma_result result;
    ma_engine engine;
    ma_sound sound;
    float listenerAngle = 0;

    if (argc < 2) {
        printf("No input file.\n");
        return -1;
    }

    result = ma_engine_init(NULL, &engine);
    if (result != MA_SUCCESS) {
        printf("Failed to initialize engine.\n");
        return -1;
    }

    result = ma_sound_init_from_file(&engine, argv[1], 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        printf("Failed to load sound: %s\n", argv[1]);
        ma_engine_uninit(&engine);
        return -1;
    }

    /* This sets the position of the sound. miniaudio follows the same coordinate system as OpenGL, where -Z is forward. */
    ma_sound_set_position(&sound, 0, 0, -1);

    /*
    This sets the position of the listener. The second parameter is the listener index. If you have only a single listener, which is
    most likely, just use 0. The position defaults to (0,0,0).
    */
    ma_engine_listener_set_position(&engine, 0, 0, 0, 0);


    /* Sounds are stopped by default. We'll start it once initial parameters have been setup. */
    ma_sound_start(&sound);


    /* Rotate the listener on the spot to create an orbiting effect. */
    for (;;) {
        listenerAngle += 0.01f;
        ma_engine_listener_set_direction(&engine, 0, (float)sin(listenerAngle), 0, (float)cos(listenerAngle));

        ma_sleep(1);
    }


    /* Won't actually get here, but do this to tear down. */
    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);

    return 0;
}

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

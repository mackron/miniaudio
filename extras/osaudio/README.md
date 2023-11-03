This is just a little experiment to explore some ideas for the kind of API that I would build if I
was building my own operation system. The name "osaudio" means Operating System Audio. Or maybe you
can think of it as Open Source Audio. It's whatever you want it to be.

The idea behind this project came about after considering the absurd complexity of audio APIs on
various platforms after years of working on miniaudio. This project aims to disprove the idea that
complete and flexible audio solutions and simple APIs are mutually exclusive and that it's possible
to have both. I challenge anybody to prove me wrong.

In addition to the above, I also wanted to explore some ideas for a different API design to
miniaudio. miniaudio uses a callback model for data transfer, whereas osaudio uses a blocking
read/write model.

This project is essentially just a header file with a reference implementation that uses miniaudio
under the hood. You can compile this very easily - just compile osaudio_miniaudio.c, and use
osaudio.h just like any other header. There are no dependencies for the header, and the miniaudio
implementation obviously requires miniaudio. Adjust the include path in osaudio_miniaudio.c if need
be.

See osaudio.h for full documentation. Below is an example to get you started:

```c
#include "osaudio.h"

...

osaudio_t audio;
osaudio_config_t config;

osaudio_config_init(&config, OSAUDIO_OUTPUT);
config.format   = OSAUDIO_FORMAT_F32;
config.channels = 2;
config.rate     = 48000;

osaudio_open(&audio, &config);

osaudio_write(audio, myAudioData, frameCount);  // <-- This will block until all of the data has been sent to the device.

osaudio_close(audio);
```

Compare the code above with the likes of other APIs like Core Audio and PipeWire. I challenge
anybody to argue their APIs are cleaner and easier to use than this when it comes to simple audio
playback.

If you have any feedback on this I'd be interested to hear it. In particular, I'd really like to
hear from people who believe the likes of Core Audio (Apple), PipeWire, PulseAudio or any other
audio API actually have good APIs (they don't!) and what makes their's better and/or worse than
this project.

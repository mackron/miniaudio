---
name: Bug report
about: Submit a bug report.
title: ''
labels: ''
assignees: ''

---

**DELETE ALL OF THIS TEXT BEFORE SUBMITTING**

If you think you've found a bug, it will be helpful to run with `#define MA_DEBUG_OUTPUT` above your miniaudio implementation like the code below and to include the output with this bug report:

    #define MA_DEBUG_OUTPUT
    #define MINIAUDIO_IMPLEMENTATION
    #include "miniaudio.h"

If you are having issues with playback, please run the simple_payback_sine example and report whether or not it's consistent with what's happening in your program.

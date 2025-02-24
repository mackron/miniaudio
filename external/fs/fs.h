/*
File system library. Choice of public domain or MIT-0. See license statements at the end of this file.
fs - v1.0.0 - Release Date TBD

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/fs
*/

/*
1. Introduction
===============
This library is used to abstract access to the regular file system and archives such as ZIP files.

1.1. Basic Usage
----------------
The main object in the library is the `fs` object. Below is the most basic way to initialize a `fs`
object:

```c
fs_result result;
fs* pFS;

result = fs_init(NULL, &pFS);
if (result != FS_SUCCESS) {
    // Failed to initialize.
}
```

The above code will initialize a `fs` object representing the system's regular file system. It uses
stdio under the hood. Once this is set up you can load files:

```c
fs_file* pFile;

result = fs_file_open(pFS, "file.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
    // Failed to open file.
}
```

Reading content from the file is very standard:

```c
size_t bytesRead;

result = fs_file_read(pFS, pBuffer, bytesToRead, &bytesRead);
if (result != FS_SUCCESS) {
    // Failed to read file. You can use FS_AT_END to check if reading failed due to being at EOF.
}
```

In the code above, the number of bytes actually read is output to a variable. You can use this to
determine if you've reached the end of the file. You can also check if the result is FS_AT_END.

To do more advanced stuff, such as opening from archives, you'll need to configure the `fs` object
with a config, which you pass into `fs_init()`:

```c
#include "extras/backends/zip/fs_zip.h" // <-- This is where FS_ZIP is declared.

...

fs_archive_type pArchiveTypes[] =
{
    {FS_ZIP, "zip"},
    {FS_ZIP, "pac"}
};

fs_config fsConfig = fs_config_init(FS_STDIO, NULL, NULL);
fsConfig.pArchiveTypes    = pArchiveTypes;
fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

fs_init(&fsConfig, &pFS);
```

In the code above we are registering support for ZIP archives (`FS_ZIP`). Whenever a file with a
"zip" or "pac" extension is found, the library will be able to access the archive. The library will
determine whether or not a file is an archive based on it's extension. You can use whatever
extension you would like for a backend, and you can associated multiple extensions to the same
backend. You can also associated different backends to the same extension, in which case the
library will use the first one that works. If the extension of a file does not match with one of
the registered archive types it'll assume it's not an archive and will skip it. Below is an example
of one way you can read from an archive:

```c
result = fs_file_open(pFS, "archive.zip/file-inside-archive.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
    // Failed to open file.
}
```

In the example above, we've explicitly specified the name of the archive in the file path. The
library also supports the ability to handle archives transparently, meaning you don't need to
explicitly specify the archive. The code below will also work:

```c
fs_file_open(pFS, "file-inside-archive.txt", FS_READ, &pFile);
```

Transparently handling archives like this has overhead because the library needs to scan the file
system and check every archive it finds. To avoid this, you can explicitly disable this feature:

```c
fs_file_open(pFS, "archive.zip/file-inside-archive.txt", FS_READ | FS_VERBOSE, &pFile);
```

In the code above, the `FS_VERBOSE` flag will require you to pass in a verbose file path, meaning
you need to explicitly specify the archive in the path. You can take this one step further by
disabling access to archives in this manner altogether via `FS_OPAQUE`:

```c
result = fs_file_open(pFS, "archive.zip/file-inside-archive.txt", FS_READ | FS_OPAQUE, &pFile);
if (result != FS_SUCCESS) {
    // This example will always fail.
}
```

In the example above, `FS_OPAQUE` is telling the library to treat archives as if they're totally
opaque and that the files within cannot be accessed.

Up to this point the handling of archives has been done automatically via `fs_file_open()`, however
the library allows you to manage archives manually. To do this you just initialize a `fs` object to
represent the archive:

```c
// Open the archive file itself first.
fs_file* pArchiveFile;

result = fs_file_open(pFS, "archive.zip", FS_READ, &pArchiveFile);
if (result != FS_SUCCESS) {
    // Failed to open archive file.
}


// Once we have the archive file we can create the `fs` object representing the archive.
fs* pArchive;
fs_config archiveConfig;

archiveConfig = fs_config_init(FS_ZIP, NULL, fs_file_get_stream(pArchiveFile));

result = fs_init(&archiveConfig, &pArchive);
if (result != FS_SUCCESS) {
    // Failed to initialize archive.
}

...

// During teardown, make sure the archive `fs` object is uninitialized before the stream.
fs_uninit(pArchive);
fs_file_close(pArchiveFile);
```

To initialize an `fs` object for an archive you need a stream to provide the raw archive data to
the backend. Conveniently, the `fs_file` object itself is a stream. In the example above we're just
opening a file from a different `fs` object (usually one representing the default file system) to
gain access to a stream. The stream does not need to be a `fs_file`. You can implement your own
`fs_stream` object, and a `fs_memory_stream` is included as stock with the library for when you
want to store the contents of an archive in-memory. Once you have the `fs` object for the archive
you can use it just like any other:

```c
result = fs_file_open(pArchive, "file-inside-archive.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
    // Failed to open file.
}
```

In addition to the above, you can use `fs_open_archive()` to open an archive from a file:

```c
fs* pArchive;

result = fs_open_archive(pFS, "archive.zip", FS_READ, &pArchive);
```

When opening an archive like this, it will inherit the archive types from the parent `fs` object
and will therefore support archives within archives. Use caution when doing this because if both
archives are compressed you will get a big performance hit. Only the inner-most archive should be
compressed.


1.2. Mounting
-------------
There is no notion of a "current directory" in this library. By default, relative paths will be
relative to whatever the backend deems appropriate. In practice, this means the "current" directory
for the default system backend, and the root directory for archives. There is still control over
how to load files from a relative path, however: mounting.

You can mount a physical directory to virtual path, similar in concept to Unix operating systems.
The difference, however, is that you can mount multiple directories to the same mount point in
which case a prioritization system will be used. There are separate mount points for reading and
writing. Below is an example of mounting for reading:

```c
fs_mount(pFS, "/some/actual/path", NULL, FS_MOUNT_PRIORITY_HIGHEST);
```

In the example above, `NULL` is equivalent to an empty path. If, for example, you have a file with
the path "/some/actual/path/file.txt", you can open it like the following:

```c
fs_file_open(pFS, "file.txt", FS_READ, &pFile);
```

You don't need to specify the "/some/actual/path" part because it's handled by the mount. If you
specify a virtual path, you can do something like the following:

```c
fs_mount(pFS, "/some/actual/path", "assets", FS_MOUNT_PRIORITY_HIGHEST);
```

In this case, loading files that are physically located in "/some/actual/path" would need to be
prexied with "assets":

```c
fs_file_open(pFS, "assets/file.txt", FS_READ, &pFile);
```

Archives can also be mounted:

```c
fs_mount(pFS, "/game/data/base/assets.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
```

You can mount multiple paths to the same mount point:

```c
fs_mount(pFS, "/game/data/base.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
fs_mount(pFS, "/game/data/mod1.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
fs_mount(pFS, "/game/data/mod2.zip", "assets", FS_MOUNT_PRIORITY_HIGHEST);
```

In the example above, the "base.zip" archive is mounted first. Then "mod1.zip" is mounted, which
takes higher priority over "base.zip". Then "mod2.zip" is mounted which takes higher priority
again. With this set up, any file that is loaded from the "assets" mount point will first be loaded
from "mod2.zip", and if it doesn't exist there, "mod1.zip", and if not there, finally "base.zip".
You could use this set up to support simple modding prioritization in a game, for example.

If the file cannot be opened from any mounts it will attempt to open the file from the backend's
default search path. Mounts always take priority. When opening in transparent mode with
`FS_TRANSPARENT` (default), it will first try opening the file as if it were not in an archive. If
that fails, it will look inside archives.

You can also mount directories for writing:

```c
fs_mount_write(pFS, "/home/user/.config/mygame", "config", FS_MOUNT_PRIORITY_HIGHEST);
```

You can then open a file for writing like so:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile);
```

When opening a file in write mode, the prefix is what determines which write mount point to use.
You can therefore have multiple write mounts:

```c
fs_mount_write(pFS, "/home/user/.config/mygame",            "config", FS_MOUNT_PRIORITY_HIGHEST);
fs_mount_write(pFS, "/home/user/.local/share/mygame/saves", "saves",  FS_MOUNT_PRIORITY_HIGHEST);
```

Now you can write out different types of files, with the prefix being used to determine where it'll
be saved:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile); // Prefixed with "config", so will use the "config" mount point.
fs_file_open(pFS, "saves/save0.sav", FS_WRITE, &pFile); // Prefixed with "saves", so will use the "saves" mount point.
```

When opening a file for writing, if you pass in NULL for the `pFS` parameter it will open the file
like normal using the standard file system. That is it'll work exactly as if you were using stdio
`fopen()` and you will not be able use mount points. Keep in mind that there is no notion of a
"current directory" in this library so you'll be stuck with the initial working directory.

By default, you can move outside the mount point with ".." segments. If you want to disable this
functionality, you can use the `FS_NO_ABOVE_ROOT_NAVIGATION` flag:

```c
fs_file_open(pFS, "../file.txt", FS_READ | FS_NO_ABOVE_ROOT_NAVIGATION, &pFile);
```

In addition, any mount points that start with a "/" will be considered absolute and will not allow
any above-root navigation:

```c
fs_mount(pFS, "/game/data/base", "/gamedata", FS_MOUNT_PRIORITY_HIGHEST);
```

In the example above, the "/gamedata" mount point starts with a "/", so it will not allow any
above-root navigation which means you cannot navigate above "/game/data/base" when using this mount
point.

Note that writing directly into an archive is not supported by this API. To write into an archive,
the backend itself must support writing, and you will need to manually initialize a `fs` object for
the archive an write into it directly.


1.3. Enumeration
----------------
You can enumerate over the contents of a directory like the following:

```c
for (fs_iterator* pIterator = fs_first(pFS, "directory/to/enumerate", FS_NULL_TERMINATED, 0); pIterator != NULL; pIterator = fs_next(pIterator)) {
    printf("Name: %s\n",   pIterator->pName);
    printf("Size: %llu\n", pIterator->info.size);
}
```

If you want to terminate iteration early, use `fs_free_iterator()` to free the iterator object.
`fs_next()` will free the iterator for you when it reaches the end.

Like when opening a file, you can specify `FS_OPAQUE`, `FS_VERBOSE` or `FS_TRANSPARENT` (default)
in `fs_first()` to control which files are enumerated. Enumerated files will be consistent with
what would be opened when using the same option with `fs_file_open()`.

Internally, `fs_first()` will gather all of the enumerated files. This means you should expect
`fs_first()` to be slow compared to `fs_next()`.

Enumerated entries will be sorted by name in terms of `strcmp()`.

Enumeration is not recursive. If you want to enumerate recursively you can inspect the `directory`
member of the `info` member in `fs_iterator`.



2. Thread Safety
================
The following points apply regarding thread safety.

  - Opening files across multiple threads is safe. Backends are responsible for ensuring thread
    safety when opening files.

  - An individual `fs_file` object is not thread safe. If you want to use a specific `fs_file`
    object across multiple threads, you will need to synchronize access to it yourself. Using
    different `fs_file` objects across multiple threads is safe.

  - Mounting and unmounting is not thread safe. You must use your own synchronization if you
    want to do this across multiple threads.

  - Opening a file on one thread while simultaneously mounting or unmounting on another thread is
    not safe. Again, you must use your own synchronization if you need to do this. The recommended
    usage is to set up your mount points once during initialization before opening any files.



3. Backends
===========
You can implement custom backends to support different file systems and archive formats. A stdio
backend is the default backend and is built into the library. A backend implements the functions
in the `fs_backend` structure.

A ZIP backend is included in the "extras" folder of this library's repository. Refer to this for
a complete example for how to implement a backend (not including write support, but I'm sure
you'll figure it out!).

The backend abstraction is designed to relieve backends from having to worry about the
implementation details of the main library. Backends should only concern themselves with their
own local content and not worry about things like mount points, archives, etc. Those details will
be handled at a higher level in the library.

Instances of a `fs` object can be configured with backend-specific configuration data. This is
passed to the backend as a void pointer to the necessary functions. This data will point to a 
backend-defined structure that the backend will know how to use.

In order for the library to know how much memory to allocate for the `fs` object, the backend
needs to implement the `alloc_size` function. This function should return the total size of the
backend-specific data to associate with the `fs` object. Internally, this memory will be stored
at the end of the `fs` object. The backend can access this data via `fs_get_backend_data()`:

```c
typedef struct my_fs_data
{
    int someData;
} my_fs_data;

...

my_fs_data* pBackendData = (my_fs_data*)fs_get_backend_data(pFS);
assert(pBackendData != NULL);

do_something(pBackendData->someData);
```

This pattern will be a central part of how backends are implemented. If you don't have any
backend-specific data, you can just return 0 from `alloc_size()` and simply not reference the
backend data pointer.

The main library will allocate the `fs` object, including any additional space specified by the
`alloc_size` function. Once this is done, it'll call the `init` function to initialize the backend.
This function will take a pointer to the `fs` object, the backend-specific configuration data, and
a stream object. The stream is used to provide the backend with the raw data of an archive, which
will be required for archive backends like ZIP. If your backend requires this, you should check
for if the stream is null, and if so, return an error. See section "4. Streams" for more details
on how to use streams. You need not take a copy of the stream pointer for use outside of `init()`.
Instead you can just use `fs_get_stream()` to get the stream object when you need it. You should
not ever close or otherwise take ownership of the stream - that will be handled at a higher level.

The `uninit` function is where you should do any cleanup. Do not close the stream here.

The `remove` function is used to remove a file. This is not recursive. If the path is a directory,
the backend should return an error if it is not empty. Backends do not need to implement this
function in which case they can leave the callback pointer as `NULL`, or have it return
`FS_NOT_IMPLEMENTED`.

The `rename` function is used to rename a file. This will act as a move if the source and
destination are in different directories. If the destination already exists, it should be
overwritten. This function is optional and can be left as `NULL` or return `FS_NOT_IMPLEMENTED`.

The `mkdir` function is used to create a directory. This is not recursive. If the directory already
exists, the backend should return `FS_SUCCESS`. This function is optional and can be left as `NULL`
or return `FS_NOT_IMPLEMENTED`.

The `info` function is used to get information about a file. If the backend does not have the
notion of the last modified or access time, it can set those values to 0. Set `directory` to 1 (or
FS_TRUE) if it's a directory. Likewise, set `symlink` to 1 if it's a symbolic link. It is important
that this function return the info of the exact file that would be opened with `file_open()`.

Like when initializing a `fs` object, the library needs to know how much backend-specific data to
allocate for the `fs_file` object. This is done with the `file_alloc_size` function. This function
is basically the same as `alloc_size` for the `fs` object, but for `fs_file`. If the backend does
not need any additional data, it can return 0. The backend can access this data via
`fs_file_get_backend_data()`.

The `file_open` function is where the backend should open the file. If the `fs` object that owns
the file was initialized with a stream, i.e. it's an archive, the stream will be non-null. You
should store this pointer for later use in `file_read`, etc. The `openMode` parameter will be a
combination of `FS_READ`, `FS_WRITE`, `FS_TRUNCATE`, `FS_APPEND` and `FS_OVERWRITE`. When opening
in write mode (`FS_WRITE`), it will default to truncate mode. You should ignore the `FS_OPAQUE`,
`FS_VERBOSE` and `FS_TRANSPARENT` flags. If the file does not exist, the backend should return
`FS_DOES_NOT_EXIST`. If the file is a directory, it should return `FS_IS_DIRECTORY`.

The file should be closed with `file_close`. This is where the backend should release any resources
associated with the file. The stream should not be closed here - it'll be cleaned up at a higher
level.

The `file_read` function is used to read data from the file. The backend should return `FS_AT_END`
when the end of the file is reached, but only if the number of bytes read is 0.

The `file_write` function is used to write data to the file. If the file is opened in append mode,
the backend should seek to the end of the file before writing. This is optional and need only be
specified if the backend supports writing.

The `file_seek` function is used to seek the cursor. The backend should return `FS_BAD_SEEK` if the
seek is out of bounds.

The `file_tell` function is used to get the cursor position.

The `file_flush` function is used to flush any buffered data to the file. This is optional and can
be left as `NULL` or return `FS_NOT_IMPLEMENTED`.

The `file_info` function is used to get information about an opened file. It returns the same
information as `info` but for an opened file.

The `file_duplicate` function is used to duplicate a file. The destination file will be a new file
and already allocated. The backend need only copy the necessary backend-specific data to the new
file.

The `first`, `next` and `free_iterator` functions are used to enumerate the contents of a directory.
If the directory is empty, or an error occurs, `fs_first` should return `NULL`. The `next` function
should return `NULL` when there are no more entries. When `next` returns `NULL`, the backend needs
to free the iterator object. The `free_iterator` function is used to free the iterator object
explicitly. The backend is responsible for any memory management of the name string. A typical way
to deal with this is to allocate the allocate additional space for the name immediately after the
`fs_iterator` allocation.

Backends are responsible for guaranteeing thread-safety of different files across different
threads. This should typically be quite easy since most system backends, such as stdio, are already
thread-safe, and archive backends are typically read-only which should make thread-safety trivial
on that front as well.


4. Streams
==========
Streams are the data delivery mechanism for archive backends. You can implement custom streams, but
this should be uncommon because `fs_file` itself is a stream, and a memory stream is included in
the library called `fs_memory_stream`. Between these two the majority of use cases should be
covered.

A stream is initialized using a specialized initialization function depending on the stream type.
For `fs_file`, simply opening the file is enough. For `fs_memory_stream`, you need to call
`fs_memory_stream_init_readonly()` for a standard read-only stream, or
`fs_memory_stream_init_write()` for a stream with write (and read) support. If you want to
implement your own stream type you would need to implement a similar initialization function.

Use `fs_stream_read()` and `fs_stream_write()` to read and write data from a stream. If the stream
does not support reading or writing, the respective function should return `FS_NOT_IMPLEMENTED`.

The cursor can be set and retrieved with `fs_stream_seek()` and `fs_stream_tell()`. There is only
a single cursor which is shared between reading and writing.

Streams can be duplicated. A duplicated stream is a fully independent stream. This functionality
is used heavily internally by the library so if you build a custom stream you should support it
if you can. Without duplication support, you will not be able to open files within archives. To
duplicate a stream, use `fs_stream_duplicate()`. To delete a duplicated stream, use
`fs_stream_delete_duplicate()`. Do not use implementation-specific uninitialization routines to
uninitialize a duplicated stream - `fs_stream_delete_duplicate()` will deal with that for you.

Streams are not thread safe. If you want to use a stream across multiple threads, you will need to
synchronize access to it yourself. Using different stream objects across multiple threads is safe.
A duplicated stream is entirely independent of the original stream and can be used across on a
different thread to the original stream.

The `fs_stream` object is a base class. If you want to implement your own stream, you should make
the first member of your stream object a `fs_stream` object. This will allow you to cast between
`fs_stream*` and your custom stream type.

See `fs_stream_vtable` for a list of functions that need to be implemented for a custom stream. If
the stream does not support writing, the `write` callback can be left as `NULL` or return
`FS_NOT_IMPLEMENTED`.

See `fs_memory_stream` for an example of how to implement a custom stream.
*/

/*
This library has been designed to be amalgamated into other libraries of mine. You will probably
see some random tags and stuff in this file. These are just used for doing a dumb amalgamation.
*/
#ifndef fs_h
#define fs_h

#include <stddef.h> /* For size_t. */
#include <stdarg.h> /* For va_list. */

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_compiler_compat.h */
#if defined(SIZE_MAX)
    #define FS_SIZE_MAX  SIZE_MAX
#else
    #define FS_SIZE_MAX  0xFFFFFFFF  /* When SIZE_MAX is not defined by the standard library just default to the maximum 32-bit unsigned integer. */
#endif

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(_M_ARM64) || defined(__powerpc64__)
    #define FS_SIZEOF_PTR    8
#else
    #define FS_SIZEOF_PTR    4
#endif

#if FS_SIZEOF_PTR == 8
    #define FS_64BIT
#else
    #define FS_32BIT
#endif

#if defined(FS_USE_STDINT)
    #include <stdint.h>
    typedef int8_t                  fs_int8;
    typedef uint8_t                 fs_uint8;
    typedef int16_t                 fs_int16;
    typedef uint16_t                fs_uint16;
    typedef int32_t                 fs_int32;
    typedef uint32_t                fs_uint32;
    typedef int64_t                 fs_int64;
    typedef uint64_t                fs_uint64;
#else
    typedef   signed char           fs_int8;
    typedef unsigned char           fs_uint8;
    typedef   signed short          fs_int16;
    typedef unsigned short          fs_uint16;
    typedef   signed int            fs_int32;
    typedef unsigned int            fs_uint32;
    #if defined(_MSC_VER) && !defined(__clang__)
        typedef   signed __int64    fs_int64;
        typedef unsigned __int64    fs_uint64;
    #else
        #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wlong-long"
            #if defined(__clang__)
                #pragma GCC diagnostic ignored "-Wc++11-long-long"
            #endif
        #endif
        typedef   signed long long  fs_int64;
        typedef unsigned long long  fs_uint64;
        #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
            #pragma GCC diagnostic pop
        #endif
    #endif
#endif  /* FS_USE_STDINT */

#if FS_SIZEOF_PTR == 8
    typedef fs_uint64 fs_uintptr;
    typedef fs_int64  fs_intptr;
#else
    typedef fs_uint32 fs_uintptr;
    typedef fs_int32  fs_intptr;
#endif

typedef unsigned char fs_bool8;
typedef unsigned int  fs_bool32;
#define FS_TRUE  1
#define FS_FALSE 0


#define FS_INT64_MAX ((fs_int64)(((fs_uint64)0x7FFFFFFF << 32) | 0xFFFFFFFF))


#ifndef FS_API
#define FS_API
#endif

#ifdef _MSC_VER
    #define FS_INLINE __forceinline
#elif defined(__GNUC__)
    /*
    I've had a bug report where GCC is emitting warnings about functions possibly not being inlineable. This warning happens when
    the __attribute__((always_inline)) attribute is defined without an "inline" statement. I think therefore there must be some
    case where "__inline__" is not always defined, thus the compiler emitting these warnings. When using -std=c89 or -ansi on the
    command line, we cannot use the "inline" keyword and instead need to use "__inline__". In an attempt to work around this issue
    I am using "__inline__" only when we're compiling in strict ANSI mode.
    */
    #if defined(__STRICT_ANSI__)
        #define FS_GNUC_INLINE_HINT __inline__
    #else
        #define FS_GNUC_INLINE_HINT inline
    #endif

    #if (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 2)) || defined(__clang__)
        #define FS_INLINE FS_GNUC_INLINE_HINT __attribute__((always_inline))
    #else
        #define FS_INLINE FS_GNUC_INLINE_HINT
    #endif
#elif defined(__WATCOMC__)
    #define FS_INLINE __inline
#else
    #define FS_INLINE
#endif


#if defined(__has_attribute)
    #if __has_attribute(format)
        #define FS_ATTRIBUTE_FORMAT(fmt, va) __attribute__((format(printf, fmt, va)))
    #endif
#endif
#ifndef FS_ATTRIBUTE_FORMAT
#define FS_ATTRIBUTE_FORMAT(fmt, va)
#endif


#define FS_NULL_TERMINATED  ((size_t)-1)
/* END fs_compiler_compat.h */


/* BEG fs_result.h */
typedef enum fs_result
{
    /* Compression Non-Error Result Codes. */
    FS_HAS_MORE_OUTPUT     = 102, /* Some stream has more output data to be read, but there's not enough room in the output buffer. */
    FS_NEEDS_MORE_INPUT    = 100, /* Some stream needs more input data before it can be processed. */

    /* Main Result Codes. */
    FS_SUCCESS             =  0,
    FS_ERROR               = -1,  /* Generic, unknown error. */
    FS_INVALID_ARGS        = -2,
    FS_INVALID_OPERATION   = -3,
    FS_OUT_OF_MEMORY       = -4,
    FS_OUT_OF_RANGE        = -5,
    FS_ACCESS_DENIED       = -6,
    FS_DOES_NOT_EXIST      = -7,
    FS_ALREADY_EXISTS      = -8,
    FS_INVALID_FILE        = -10,
    FS_TOO_BIG             = -11,
    FS_NOT_DIRECTORY       = -14,
    FS_IS_DIRECTORY        = -15,
    FS_DIRECTORY_NOT_EMPTY = -16,
    FS_AT_END              = -17,
    FS_BUSY                = -19,
    FS_BAD_SEEK            = -25,
    FS_NOT_IMPLEMENTED     = -29,
    FS_TIMEOUT             = -34,
    FS_CHECKSUM_MISMATCH   = -100,
    FS_NO_BACKEND          = -101
} fs_result;
/* END fs_result.h */


/* BEG fs_allocation_callbacks.h */
typedef struct fs_allocation_callbacks
{
    void* pUserData;
    void* (* onMalloc )(size_t sz, void* pUserData);
    void* (* onRealloc)(void* p, size_t sz, void* pUserData);
    void  (* onFree   )(void* p, void* pUserData);
} fs_allocation_callbacks;

FS_API void* fs_malloc(size_t sz, const fs_allocation_callbacks* pAllocationCallbacks);
FS_API void* fs_calloc(size_t sz, const fs_allocation_callbacks* pAllocationCallbacks);
FS_API void* fs_realloc(void* p, size_t sz, const fs_allocation_callbacks* pAllocationCallbacks);
FS_API void  fs_free(void* p, const fs_allocation_callbacks* pAllocationCallbacks);
/* END fs_allocation_callbacks.h */



/* BEG fs_stream.h */
/*
Streams.

The feeding of input and output data is done via a stream.

To implement a custom stream, such as a memory stream, or a file stream, you need to extend from
`fs_stream` and implement `fs_stream_vtable`. You can access your custom data by casting the
`fs_stream` to your custom type.

The stream vtable can support both reading and writing, but it doesn't need to support both at
the same time. If one is not supported, simply leave the relevant `read` or `write` callback as
`NULL`, or have them return FS_NOT_IMPLEMENTED.
*/

/* Seek Origins. */
typedef enum fs_seek_origin
{
    FS_SEEK_SET = 0,
    FS_SEEK_CUR = 1,
    FS_SEEK_END = 2
} fs_seek_origin;

typedef struct fs_stream_vtable fs_stream_vtable;
typedef struct fs_stream        fs_stream;

struct fs_stream_vtable
{
    fs_result (* read                )(fs_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead);
    fs_result (* write               )(fs_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
    fs_result (* seek                )(fs_stream* pStream, fs_int64 offset, fs_seek_origin origin);
    fs_result (* tell                )(fs_stream* pStream, fs_int64* pCursor);
    size_t    (* duplicate_alloc_size)(fs_stream* pStream);                                 /* Optional. Returns the allocation size of the stream. When not defined, duplicating is disabled. */
    fs_result (* duplicate           )(fs_stream* pStream, fs_stream* pDuplicatedStream);   /* Optional. Duplicate the stream. */
    void      (* uninit              )(fs_stream* pStream);                                 /* Optional. Uninitialize the stream. */
};

struct fs_stream
{
    const fs_stream_vtable* pVTable;
};

FS_API fs_result fs_stream_init(const fs_stream_vtable* pVTable, fs_stream* pStream);
FS_API fs_result fs_stream_read(fs_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead);
FS_API fs_result fs_stream_write(fs_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
FS_API fs_result fs_stream_writef(fs_stream* pStream, const char* fmt, ...) FS_ATTRIBUTE_FORMAT(2, 3);
FS_API fs_result fs_stream_writef_ex(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, const char* fmt, ...) FS_ATTRIBUTE_FORMAT(3, 4);
FS_API fs_result fs_stream_writefv(fs_stream* pStream, const char* fmt, va_list args);
FS_API fs_result fs_stream_writefv_ex(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, const char* fmt, va_list args);
FS_API fs_result fs_stream_seek(fs_stream* pStream, fs_int64 offset, fs_seek_origin origin);
FS_API fs_result fs_stream_tell(fs_stream* pStream, fs_int64* pCursor);

/*
Duplicates a stream.

This will allocate the new stream on the heap. The caller is responsible for freeing the stream
with `fs_stream_delete_duplicate()` when it's no longer needed.
*/
FS_API fs_result fs_stream_duplicate(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, fs_stream** ppDuplicatedStream);

/*
Deletes a duplicated stream.

Do not use this for a stream that was not duplicated with `fs_stream_duplicate()`.
*/
FS_API void fs_stream_delete_duplicate(fs_stream* pDuplicatedStream, const fs_allocation_callbacks* pAllocationCallbacks);


/*
Helper functions for reading the entire contents of a stream, starting from the current cursor position. Free
the returned pointer with fs_free().

The format (FS_FORMAT_TEXT or FS_FORMAT_BINARY) is used to determine whether or not a null terminator should be
appended to the end of the data.

For flexiblity in case the backend does not support cursor retrieval or positioning, the data will be read
in fixed sized chunks.
*/
typedef enum fs_format
{
    FS_FORMAT_TEXT,
    FS_FORMAT_BINARY
} fs_format;

FS_API fs_result fs_stream_read_to_end(fs_stream* pStream, fs_format format, const fs_allocation_callbacks* pAllocationCallbacks, void** ppData, size_t* pDataSize);
/* END fs_stream.h */



/* BEG fs.h */
/* Open mode flags. */
#define FS_READ                     0x0001
#define FS_WRITE                    0x0002
#define FS_APPEND                   (FS_WRITE | 0x0004)
#define FS_OVERWRITE                (FS_WRITE | 0x0008)
#define FS_TRUNCATE                 (FS_WRITE)

#define FS_TRANSPARENT              0x0000  /* Default. Opens a file such that archives of a known type are handled transparently. For example, "somefolder/archive.zip/file.txt" can be opened with "somefolder/file.txt" (the "archive.zip" part need not be specified). This assumes the `fs` object has been initialized with support for the relevant archive types. */
#define FS_OPAQUE                   0x0010  /* When used, files inside archives cannot be opened automatically. For example, "somefolder/archive.zip/file.txt" will fail. Mounted archives work fine. */
#define FS_VERBOSE                  0x0020  /* When used, files inside archives can be opened, but the name of the archive must be specified explicitly in the path, such as "somefolder/archive.zip/file.txt" */

#define FS_NO_CREATE_DIRS           0x0040  /* When used, directories will not be created automatically when opening files for writing. */
#define FS_IGNORE_MOUNTS            0x0080  /* When used, mounted directories and archives will be ignored when opening and iterating files. */
#define FS_ONLY_MOUNTS              0x0100  /* When used, only mounted directories and archives will be considered when opening and iterating files. */
#define FS_NO_SPECIAL_DIRS          0x0200  /* When used, the presence of special directories like "." and ".." will be result in an error when opening files. */
#define FS_NO_ABOVE_ROOT_NAVIGATION 0x0400  /* When used, navigating above the mount point with leading ".." segments will result in an error. Can be also be used with fs_path_normalize(). */


/* Garbage collection policies.*/
#define FS_GC_POLICY_THRESHOLD      0x0001  /* Only garbage collect unreferenced opened archives until the count is below the configured threshold. */
#define FS_GC_POLICY_FULL           0x0002  /* Garbage collect every unreferenced opened archive, regardless of how many are open.*/


typedef struct fs_config    fs_config;
typedef struct fs           fs;
typedef struct fs_file      fs_file;
typedef struct fs_file_info fs_file_info;
typedef struct fs_iterator  fs_iterator;
typedef struct fs_backend   fs_backend;

typedef enum fs_mount_priority
{
    FS_MOUNT_PRIORITY_HIGHEST = 0,
    FS_MOUNT_PRIORITY_LOWEST  = 1
} fs_mount_priority;

typedef struct fs_archive_type
{
    const fs_backend* pBackend;
    const char* pExtension;
} fs_archive_type;

struct fs_file_info
{
    fs_uint64 size;
    fs_uint64 lastModifiedTime;
    fs_uint64 lastAccessTime;
    int directory;
    int symlink;
};

struct fs_iterator
{
    fs* pFS;
    const char* pName;              /* Must be null terminated. The FS implementation is responsible for manageing the memory allocation. */
    size_t nameLen;
    fs_file_info info;
};

struct fs_config
{
    const fs_backend* pBackend;
    const void* pBackendConfig;
    fs_stream* pStream;
    const fs_archive_type* pArchiveTypes;
    size_t archiveTypeCount;
    const fs_allocation_callbacks* pAllocationCallbacks;
};

FS_API fs_config fs_config_init_default(void);
FS_API fs_config fs_config_init(const fs_backend* pBackend, void* pBackendConfig, fs_stream* pStream);


typedef struct fs_backend
{
    size_t       (* alloc_size      )(const void* pBackendConfig);
    fs_result    (* init            )(fs* pFS, const void* pBackendConfig, fs_stream* pStream);              /* Return 0 on success or an errno result code on error. pBackendConfig is a pointer to a backend-specific struct. The documentation for your backend will tell you how to use this. You can usually pass in NULL for this. */
    void         (* uninit          )(fs* pFS);
    fs_result    (* ioctl           )(fs* pFS, int op, void* pArg);                                          /* Optional. */
    fs_result    (* remove          )(fs* pFS, const char* pFilePath);
    fs_result    (* rename          )(fs* pFS, const char* pOldName, const char* pNewName);
    fs_result    (* mkdir           )(fs* pFS, const char* pPath);                                           /* This is not recursive. Return FS_SUCCESS if directory already exists. */
    fs_result    (* info            )(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo);        /* openMode flags can be ignored by most backends. It's primarily used by proxy of passthrough style backends. */
    size_t       (* file_alloc_size )(fs* pFS);
    fs_result    (* file_open       )(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile); /* Return 0 on success or an errno result code on error. Return ENOENT if the file does not exist. pStream will be null if the backend does not need a stream (the `pFS` object was not initialized with one). */
    fs_result    (* file_open_handle)(fs* pFS, void* hBackendFile, fs_file* pFile);                   /* Optional. Open a file from a file handle. Backend-specific. The format of hBackendFile will be specified by the backend. */
    void         (* file_close      )(fs_file* pFile);
    fs_result    (* file_read       )(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead);   /* Return 0 on success, or FS_AT_END on end of file. Only return FS_AT_END if *pBytesRead is 0. Return an errno code on error. Implementations must support reading when already at EOF, in which case FS_AT_END should be returned and *pBytesRead should be 0. */
    fs_result    (* file_write      )(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
    fs_result    (* file_seek       )(fs_file* pFile, fs_int64 offset, fs_seek_origin origin);
    fs_result    (* file_tell       )(fs_file* pFile, fs_int64* pCursor);
    fs_result    (* file_flush      )(fs_file* pFile);
    fs_result    (* file_info       )(fs_file* pFile, fs_file_info* pInfo);
    fs_result    (* file_duplicate  )(fs_file* pFile, fs_file* pDuplicate);                                  /* Duplicate the file handle. */
    fs_iterator* (* first           )(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen);
    fs_iterator* (* next            )(fs_iterator* pIterator);  /* <-- Must return null when there are no more files. In this case, free_iterator must be called internally. */
    void         (* free_iterator   )(fs_iterator* pIterator);  /* <-- Free the `fs_iterator` object here since `first` and `next` were the ones who allocated it. Also do any uninitialization routines. */
} fs_backend;

FS_API fs_result fs_init(const fs_config* pConfig, fs** ppFS);
FS_API void fs_uninit(fs* pFS);
FS_API fs_result fs_ioctl(fs* pFS, int op, void* pArg);
FS_API fs_result fs_remove(fs* pFS, const char* pFilePath);
FS_API fs_result fs_rename(fs* pFS, const char* pOldName, const char* pNewName);
FS_API fs_result fs_mkdir(fs* pFS, const char* pPath);  /* Does not consider mounts. Returns FS_SUCCESS if directory already exists. */
FS_API fs_result fs_info(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo);  /* openMode flags specify same options as openMode in file_open(), but FS_READ, FS_WRITE, FS_TRUNCATE, FS_APPEND, and FS_OVERWRITE are ignored. */
FS_API fs_stream* fs_get_stream(fs* pFS);
FS_API const fs_allocation_callbacks* fs_get_allocation_callbacks(fs* pFS);
FS_API void* fs_get_backend_data(fs* pFS);    /* For use by the backend. Will be the size returned by the alloc_size() function in the vtable. */
FS_API size_t fs_get_backend_data_size(fs* pFS);

FS_API fs_result fs_open_archive_ex(fs* pFS, const fs_backend* pBackend, void* pBackendConfig, const char* pArchivePath, size_t archivePathLen, int openMode, fs** ppArchive);
FS_API fs_result fs_open_archive(fs* pFS, const char* pArchivePath, int openMode, fs** ppArchive);
FS_API void fs_close_archive(fs* pArchive);
FS_API void fs_gc_archives(fs* pFS, int policy);
FS_API void fs_set_archive_gc_threshold(fs* pFS, size_t threshold);
FS_API size_t fs_get_archive_gc_threshold(fs* pFS);

FS_API fs_result fs_file_open(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile);
FS_API fs_result fs_file_open_from_handle(fs* pFS, void* hBackendFile, fs_file** ppFile);
FS_API void fs_file_close(fs_file* pFile);
FS_API fs_result fs_file_read(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead); /* Returns 0 on success, FS_AT_END on end of file, or an errno result code on error. Will only return FS_AT_END if *pBytesRead is 0. */
FS_API fs_result fs_file_write(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
FS_API fs_result fs_file_writef(fs_file* pFile, const char* fmt, ...) FS_ATTRIBUTE_FORMAT(2, 3);
FS_API fs_result fs_file_writefv(fs_file* pFile, const char* fmt, va_list args);
FS_API fs_result fs_file_seek(fs_file* pFile, fs_int64 offset, fs_seek_origin origin);
FS_API fs_result fs_file_tell(fs_file* pFile, fs_int64* pCursor);
FS_API fs_result fs_file_flush(fs_file* pFile);
FS_API fs_result fs_file_get_info(fs_file* pFile, fs_file_info* pInfo);
FS_API fs_result fs_file_duplicate(fs_file* pFile, fs_file** ppDuplicate);  /* Duplicate the file handle. */
FS_API void* fs_file_get_backend_data(fs_file* pFile);
FS_API size_t fs_file_get_backend_data_size(fs_file* pFile);
FS_API fs_stream* fs_file_get_stream(fs_file* pFile);     /* Files are streams. They can be cast directly to fs_stream*, but this function is here for people who prefer function style getters. */
FS_API fs* fs_file_get_fs(fs_file* pFile);

FS_API fs_iterator* fs_first_ex(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen, int mode);
FS_API fs_iterator* fs_first(fs* pFS, const char* pDirectoryPath, int mode);
FS_API fs_iterator* fs_next(fs_iterator* pIterator);
FS_API void fs_free_iterator(fs_iterator* pIterator);

FS_API fs_result fs_mount(fs* pFS, const char* pPathToMount, const char* pMountPoint, fs_mount_priority priority);
FS_API fs_result fs_unmount(fs* pFS, const char* pPathToMount_NotMountPoint);
FS_API fs_result fs_mount_fs(fs* pFS, fs* pOtherFS, const char* pMountPoint, fs_mount_priority priority);
FS_API fs_result fs_unmount_fs(fs* pFS, fs* pOtherFS);   /* Must be matched up with fs_mount_fs(). */

FS_API fs_result fs_mount_write(fs* pFS, const char* pPathToMount, const char* pMountPoint, fs_mount_priority priority);
FS_API fs_result fs_unmount_write(fs* pFS, const char* pPathToMount_NotMountPoint);

/*
Helper functions for reading the entire contents of a file, starting from the current cursor position. Free
the returned pointer with fs_free(), using the same allocation callbacks as the fs object. You can use
fs_get_allocation_callbacks() if necessary, like so:

    fs_free(pFileData, fs_get_allocation_callbacks(pFS));

The format (FS_FORMAT_TEXT or FS_FORMAT_BINARY) is used to determine whether or not a null terminator should be
appended to the end of the data.

For flexiblity in case the backend does not support cursor retrieval or positioning, the data will be read
in fixed sized chunks.
*/
FS_API fs_result fs_file_read_to_end(fs_file* pFile, fs_format format, void** ppData, size_t* pDataSize);
FS_API fs_result fs_file_open_and_read(fs* pFS, const char* pFilePath, fs_format format, void** ppData, size_t* pDataSize);
FS_API fs_result fs_file_open_and_write(fs* pFS, const char* pFilePath, void* pData, size_t dataSize);


/* Default Backend. */
extern const fs_backend* FS_STDIO;  /* The default stdio backend. The handle for fs_file_open_from_handle() is a FILE*. */
/* END fs.h */



/* BEG fs_helpers.h */
/*
This section just contains various helper functions, mainly for custom backends.
*/

/* Converts an errno code to our own error code. */
FS_API fs_result fs_result_from_errno(int error);


/* END fs_helpers.h */


/* BEG fs_path.h */
/*
These functions are low-level functions for working with paths. The most important part of this API
is probably the iteration functions. These functions are used for iterating over each of the
segments of a path. This library will recognize both '\' and '/'. If you want to use just one or
the other, or a different separator, you'll need to use a different library. Likewise, this library
will treat paths as case sensitive. Again, you'll need to use a different library if this is not
suitable for you.

Iteration will always return both sides of a separator. For example, if you iterate "abc/def",
you will get two items: "abc" and "def". Where this is of particular importance and where you must
be careful, is the handling of the root directory. If you iterate "/", it will also return two
items. The first will be length 0 with an offset of zero which represents the left side of the "/"
and the second will be length 0 with an offset of 1 which represents the right side. The reason for
this design is that it makes iteration unambiguous and makes it easier to reconstruct a path.

The path API does not do any kind of validation to check if it represents an actual path on the
file system. Likewise, it does not do any validation to check if the path contains invalid
characters. All it cares about is "/" and "\".
*/
typedef struct
{
    const char* pFullPath;
    size_t fullPathLength;
    size_t segmentOffset;
    size_t segmentLength;
} fs_path_iterator;

FS_API fs_result fs_path_first(const char* pPath, size_t pathLen, fs_path_iterator* pIterator);
FS_API fs_result fs_path_last(const char* pPath, size_t pathLen, fs_path_iterator* pIterator);
FS_API fs_result fs_path_next(fs_path_iterator* pIterator);
FS_API fs_result fs_path_prev(fs_path_iterator* pIterator);
FS_API fs_bool32 fs_path_is_first(const fs_path_iterator* pIterator);
FS_API fs_bool32 fs_path_is_last(const fs_path_iterator* pIterator);
FS_API int fs_path_iterators_compare(const fs_path_iterator* pIteratorA, const fs_path_iterator* pIteratorB);
FS_API const char* fs_path_file_name(const char* pPath, size_t pathLen);    /* Does *not* include the null terminator. Returns an offset of pPath. Will only be null terminated if pPath is. Returns null if the path ends with a slash. */
FS_API int fs_path_directory(char* pDst, size_t dstCap, const char* pPath, size_t pathLen); /* Returns the length, or < 0 on error. pDst can be null in which case the required length will be returned. Will not include a trailing slash. */
FS_API const char* fs_path_extension(const char* pPath, size_t pathLen);    /* Does *not* include the null terminator. Returns an offset of pPath. Will only be null terminated if pPath is. Returns null if the extension cannot be found. */
FS_API fs_bool32 fs_path_extension_equal(const char* pPath, size_t pathLen, const char* pExtension, size_t extensionLen); /* Returns true if the extension is equal to the given extension. Case insensitive. */
FS_API const char* fs_path_trim_base(const char* pPath, size_t pathLen, const char* pBasePath, size_t basePathLen);
FS_API int fs_path_append(char* pDst, size_t dstCap, const char* pBasePath, size_t basePathLen, const char* pPathToAppend, size_t pathToAppendLen); /* pDst can be equal to pBasePath in which case it will be appended in-place. pDst can be null in which case the function will return the required length. */
FS_API int fs_path_normalize(char* pDst, size_t dstCap, const char* pPath, size_t pathLen, unsigned int options); /* The only root component recognized is "/". The path cannot start with "C:", "//<address>", etc. This is not intended to be a general cross-platform path normalization routine. If the path starts with "/", this will fail with a negative result code if normalization would result in the path going above the root directory. Will convert all separators to "/". Will remove trailing slash. pDst can be null in which case the required length will be returned. */
/* END fs_path.h */


/* BEG fs_memory_stream.h */
/*
Memory streams support both reading and writing within the same stream. To only support read-only
mode, use fs_memory_stream_init_readonly(). With this you can pass in a standard data/size pair.

If you need writing support, use fs_memory_stream_init_write(). When writing data, the stream will
output to a buffer that is owned by the stream. When you need to access the data, do so by
inspecting the pointer directly with `stream.write.pData` and `stream.write.dataSize`. This mode
also supports reading.

You can overwrite data by seeking to the required location and then just writing like normal. To
append data, just seek to the end:

    fs_memory_stream_seek(pStream, 0, fs_SEEK_ORIGIN_END);

The memory stream need not be uninitialized in read-only mode. In write mode you can use
`fs_memory_stream_uninit()` to free the data. Alternatively you can just take ownership of the
buffer and free it yourself with `fs_free()`.

Below is an example for write mode.

    ```c
    fs_memory_stream stream;
    fs_memory_stream_init_write(NULL, &stream);
    
    // Write some data to the stream...
    fs_memory_stream_write(&stream, pSomeData, someDataSize, NULL);
    
    // Do something with the data.
    do_something_with_my_data(stream.write.pData, stream.write.dataSize);
    ```

To free the data, you can use `fs_memory_stream_uninit()`, or you can take ownership of the data
and free it yourself with `fs_free()`:

    ```c
    fs_memory_stream_uninit(&stream);
    ```

Or to take ownership:

    ```c
    size_t dataSize;
    void* pData = fs_memory_stream_take_ownership(&stream, &dataSize);
    ```

With the above, `pData` will be the pointer to the data and `dataSize` will be the size of the data
and you will be responsible for deleting the buffer with `fs_free()`.


Read mode is simpler:

    ```c
    fs_memory_stream stream;
    fs_memory_stream_init_readonly(pData, dataSize, &stream);

    // Read some data.
    fs_memory_stream_read(&stream, &myBuffer, bytesToRead, NULL);
    ```

There is only one cursor. As you read and write the cursor will move forward. If you need to
read and write from different locations from the same fs_memory_stream object, you need to
seek before doing your read or write. You cannot read and write at the same time across
multiple threads for the same fs_memory_stream object.
*/
typedef struct fs_memory_stream fs_memory_stream;

struct fs_memory_stream
{
    fs_stream base;
    void** ppData;      /* Will be set to &readonly.pData in readonly mode. */
    size_t* pDataSize;  /* Will be set to &readonly.dataSize in readonly mode. */
    struct
    {
        const void* pData;
        size_t dataSize;
    } readonly;
    struct
    {
        void* pData;        /* Will only be set in write mode. */
        size_t dataSize;
        size_t dataCap;
    } write;
    size_t cursor;
    fs_allocation_callbacks allocationCallbacks; /* This is copied from the allocation callbacks passed in from e_memory_stream_init(). Only used in write mode. */
};

FS_API fs_result fs_memory_stream_init_write(const fs_allocation_callbacks* pAllocationCallbacks, fs_memory_stream* pStream);
FS_API fs_result fs_memory_stream_init_readonly(const void* pData, size_t dataSize, fs_memory_stream* pStream);
FS_API void fs_memory_stream_uninit(fs_memory_stream* pStream);    /* Only needed for write mode. This will free the internal pointer so make sure you've done what you need to do with it. */
FS_API fs_result fs_memory_stream_read(fs_memory_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead);
FS_API fs_result fs_memory_stream_write(fs_memory_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
FS_API fs_result fs_memory_stream_seek(fs_memory_stream* pStream, fs_int64 offset, int origin);
FS_API fs_result fs_memory_stream_tell(fs_memory_stream* pStream, size_t* pCursor);
FS_API fs_result fs_memory_stream_remove(fs_memory_stream* pStream, size_t offset, size_t size);
FS_API fs_result fs_memory_stream_truncate(fs_memory_stream* pStream);
FS_API void* fs_memory_stream_take_ownership(fs_memory_stream* pStream, size_t* pSize);  /* Takes ownership of the buffer. The caller is responsible for freeing the buffer with fs_free(). Only valid in write mode. */
/* END fs_memory_stream.h */


/* BEG fs_utils.h */
FS_API void fs_sort(void* pBase, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData);
FS_API void* fs_binary_search(const void* pKey, const void* pList, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData);
FS_API void* fs_linear_search(const void* pKey, const void* pList, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData);
FS_API void* fs_sorted_search(const void* pKey, const void* pList, size_t count, size_t stride, int (*compareProc)(void*, const void*, const void*), void* pUserData);

FS_API int fs_strnicmp(const char* str1, const char* str2, size_t count);
/* END fs_utils.h */


/* BEG fs_snprintf.h */
FS_API int fs_vsprintf(char* buf, char const* fmt, va_list va);
FS_API int fs_vsnprintf(char* buf, size_t count, char const* fmt, va_list va);
FS_API int fs_sprintf(char* buf, char const* fmt, ...) FS_ATTRIBUTE_FORMAT(2, 3);
FS_API int fs_snprintf(char* buf, size_t count, char const* fmt, ...) FS_ATTRIBUTE_FORMAT(3, 4);
/* END fs_snprintf.h */


#if defined(__cplusplus)
}
#endif
#endif  /* fs_h */

/*
This software is available as a choice of the following licenses. Choose
whichever you prefer.

===============================================================================
ALTERNATIVE 1 - Public Domain (www.unlicense.org)
===============================================================================
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.

In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

===============================================================================
ALTERNATIVE 2 - MIT No Attribution
===============================================================================
Copyright 2025 David Reid

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

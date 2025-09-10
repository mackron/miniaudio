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

If you don't need any of the advanced features of the library, you can just pass in NULL for the
`fs` object which will just use the native file system like normal:

```c
fs_file_open(NULL, "file.txt", FS_READ, &pFile);
```

From here on out, examples will use an `fs` object for the sake of consistency, but all basic IO
APIs that do not use things like mounting and archive registration will work with NULL.

To close a file, use `fs_file_close()`:

```c
fs_file_close(pFile);
```

Reading content from the file is very standard:

```c
size_t bytesRead;

result = fs_file_read(pFile, pBuffer, bytesToRead, &bytesRead);
if (result != FS_SUCCESS) {
    // Failed to read file. You can use FS_AT_END to check if reading failed due to being at EOF.
}
```

In the code above, the number of bytes actually read is output to a variable. You can use this to
determine if you've reached the end of the file. You can also check if the result is FS_AT_END. You
can pass in null for the last parameter of `fs_file_read()` in which an error will be returned if
the exact number of bytes requested could not be read.

Writing works the same way as reading:

```c
fs_file* pFile;

result = fs_file_open(pFS, "file.txt", FS_WRITE, &pFile);
if (result != FS_SUCCESS) {
    // Failed to open file.
}

result = fs_file_write(pFile, pBuffer, bytesToWrite, &bytesWritten);
```

Formatted writing is also supported:

```c
result = fs_file_writef(pFile, "Hello %s!\n", "World");
if (result != FS_SUCCESS) {
    // Failed to write file.
}

va_list args;
va_start(args, format);
result = fs_file_writefv(pFile, "Hello %s!\n", args);
va_end(args);
```

The `FS_WRITE` option will default to overwrite mode. You can use `FS_TRUNCATE` if you want to
truncate the file instead of overwriting it.

```c
fs_file_open(pFS, "file.txt", FS_WRITE | FS_TRUNCATE, &pFile);
```

You can also open a file in append mode with `FS_APPEND`:

```c
fs_file_open(pFS, "file.txt", FS_WRITE | FS_APPEND, &pFile);
```

When using `FS_APPEND` mode, the file will always append to the end of the file and can never
overwrite existing content. This follows POSIX semantics. In this mode, it is not possible to
create sparse files.

To open a file in write mode, but fail if the file already exists, you can use `FS_EXCLUSIVE`:

```c
fs_file_open(pFS, "file.txt", FS_WRITE | FS_EXCLUSIVE, &pFile);
```

Files can be opened for both reading and writing by simply combining the two:

```c
fs_file_open(pFS, "file.txt", FS_READ | FS_WRITE, &pFile);
```

Seeking and telling is very standard as well:

```c
fs_file_seek(pFile, 0, FS_SEEK_END);

fs_int64 cursorPos;
fs_file_tell(pFile, &cursorPos);
```

When seeking, you can seek beyond the end of the file. Attempting to read from beyond the end of
the file will return `FS_AT_END`. Attempting to write beyond the end of the file will create a
hole if supported by the file system, or fill the space with data (the filled data can be left
undefined). When seeking from the end of the file with a negative offset, it will seek backwards
from the end. Seeking to before the start of the file is not allowed and will return an error.

Retrieving information about a file is done with `fs_file_get_info()`:

```c
fs_file_info info;
fs_file_get_info(pFile, &info);
```

If you want to get information about a file without opening it, you can use `fs_info()`:

```c
fs_file_info info;
fs_info(pFS, "file.txt", FS_READ, &info);   // FS_READ tells it to check read-only mounts (explained later)
```

A file handle can be duplicated with `fs_file_duplicate()`:

```c
fs_file* pFileDup;
fs_file_duplicate(pFile, &pFileDup);
```

Note that this will only duplicate the file handle. It does not make a copy of the file on the file
system itself. The duplicated file handle will be entirely independent of the original handle,
including having its own separate read/write cursor position. The initial position of the cursor of
the new file handle is undefined and you should explicitly seek to the appropriate location.

Important: `fs_file_duplicate()` can fail or work incorrectly if you use relative paths for mounts
and something changes the working directory. The reason being that it reopens the file based on the
original path to do the duplication.

To delete a file, use `fs_remove()`:

```c
fs_remove(pFS, "file.txt");
```

Note that files are deleted permanently. There is no recycle bin or trash functionality.

Files can be renamed and moved with `fs_rename()`:

```c
fs_rename(pFS, "file.txt", "new-file.txt");
```

To create a directory, use `fs_mkdir()`:

```c
fs_mkdir(pFS, "new-directory", 0);
```

By default, `fs_mkdir()` will create the directory hierarchy for you. If you want to disable this
so it fails if the directory hierarchy doesn't exist, you can use `FS_NO_CREATE_DIRS`:

```c
fs_mkdir(pFS, "new-directory", FS_NO_CREATE_DIRS);
```

1.2. Archives
-------------
To enable support for archives, you need an `fs` object, and it must be initialized with a config:

```c
#include "extras/backends/zip/fs_zip.h" // <-- This is where FS_ZIP is declared.
#include "extras/backends/pak/fs_pak.h" // <-- This is where FS_PAK is declared.

...

fs_archive_type pArchiveTypes[] =
{
    {FS_ZIP, "zip"},
    {FS_PAK, "pak"}
};

fs_config fsConfig = fs_config_init_default();
fsConfig.pArchiveTypes    = pArchiveTypes;
fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

fs* pFS;
fs_init(&fsConfig, &pFS);
```

In the code above we are registering support for ZIP archives (`FS_ZIP`) and Quake PAK archives
(`FS_PAK`). Whenever a file with a "zip" or "pak" extension is found, the library will be able to
access the archive. The library will determine whether or not a file is an archive based on its
extension. You can use whatever extension you would like for a backend, and you can associate
multiple extensions to the same backend. You can also associate different backends to the same
extension, in which case the library will use the first one that works. If the extension of a file
does not match with one of the registered archive types it'll assume it's not an archive and will
skip it. Below is an example of one way you can read from an archive:

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

In the example above, opening the file will fail because `FS_OPAQUE` is telling the library to
treat archives as if they're totally opaque which means the files within cannot be accessed.

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
fs_open_archive(pFS, "archive.zip", FS_READ, &pArchive);

...

// When tearing down, do *not* use `fs_uninit()`. Use `fs_close_archive()` instead.
fs_close_archive(pArchive);
```

Note that you need to use `fs_close_archive()` when opening an archive like this. The reason for
this is that there's some internal reference counting and memory management happening under the
hood. You should only call `fs_close_archive()` if `fs_open_archive()` succeeds.

When opening an archive with `fs_open_archive()`, it will inherit the archive types from the parent
`fs` object and will therefore support archives within archives. Use caution when doing this
because if both archives are compressed you will get a big performance hit. Only the inner-most
archive should be compressed.


1.3. Mounting
-------------
There is no ability to change the working directory in this library. Instead you can mount a
physical directory to a virtual path, similar in concept to Unix operating systems. The difference,
however, is that you can mount multiple directories to the same virtual path in which case a
prioritization system will be used (only for reading - in write mode only a single mount is used).
There are separate mount points for reading and writing. Below is an example of mounting for
reading:

```c
fs_mount(pFS, "/some/actual/path", NULL, FS_READ);
```

To unmount, you need to specify the actual path, not the virtual path:

```c
fs_unmount(pFS, "/some/actual/path", FS_READ);
```

In the example above, using `NULL` for the virtual path is equivalent to an empty path. If, for
example, you have a file with the path "/some/actual/path/file.txt", you can open it like the
following:

```c
fs_file_open(pFS, "file.txt", FS_READ, &pFile);
```

You don't need to specify the "/some/actual/path" part because it's handled by the mount. If you
specify a virtual path, you can do something like the following:

```c
fs_mount(pFS, "/some/actual/path", "assets", FS_READ);
```

In this case, loading files that are physically located in "/some/actual/path" would need to be
prefixed with "assets":

```c
fs_file_open(pFS, "assets/file.txt", FS_READ, &pFile);
```

You can mount multiple paths to the same virtual path in which case a prioritization system will be
used:

```c
fs_mount(pFS, "/usr/share/mygame/gamedata/base",              "gamedata", FS_READ); // Base game. Lowest priority.
fs_mount(pFS, "/home/user/.local/share/mygame/gamedata/mod1", "gamedata", FS_READ); // Mod #1. Middle priority.
fs_mount(pFS, "/home/user/.local/share/mygame/gamedata/mod2", "gamedata", FS_READ); // Mod #2. Highest priority.
```

The example above shows a basic system for setting up some kind of modding support in a game. In
this case, attempting to load a file from the "gamedata" mount point will first check the "mod2"
directory, and if it cannot be opened from there, it will check "mod1", and finally it'll fall back
to the base game data.

Internally there are a separate set of mounts for reading and writing. To set up a mount point for
opening files in write mode, you need to specify the `FS_WRITE` option:

```c
fs_mount(pFS, "/home/user/.config/mygame",            "config", FS_WRITE);
fs_mount(pFS, "/home/user/.local/share/mygame/saves", "saves",  FS_WRITE);
```

To open a file for writing, you need only prefix the path with the mount's virtual path, exactly
like read mode:

```c
fs_file_open(pFS, "config/game.cfg", FS_WRITE, &pFile); // Prefixed with "config", so will use the "config" mount point.
fs_file_open(pFS, "saves/save0.sav", FS_WRITE, &pFile); // Prefixed with "saves", so will use the "saves" mount point.
```

If you want to mount a directory for reading and writing, you can use both `FS_READ` and
`FS_WRITE` together:

```c
fs_mount(pFS, "/home/user/.config/mygame", "config", FS_READ | FS_WRITE);
```

You can set up read and write mount points to the same virtual path:

```c
fs_mount(pFS, "/usr/share/mygame/config",              "config", FS_READ);
fs_mount(pFS, "/home/user/.local/share/mygame/config", "config", FS_READ | FS_WRITE);
```

When opening a file for reading, it'll first try searching the second mount point, and if it's not
found will fall back to the first. When opening in write mode, it will only ever use the second
mount point as the output directory because that's the only one set up with `FS_WRITE`. With this
setup, the first mount point is essentially protected from modification.

When mounting a directory for writing, the library will create the directory hierarchy for you. If
you want to disable this functionality, you can use the `FS_NO_CREATE_DIRS` flag:

```c
fs_mount(pFS, "/home/user/.config/mygame", "config", FS_WRITE | FS_NO_CREATE_DIRS);
```


By default, you can move outside the mount point with ".." segments. If you want to disable this
functionality, you can use the `FS_NO_ABOVE_ROOT_NAVIGATION` flag when opening the file:

```c
fs_file_open(pFS, "../file.txt", FS_READ | FS_NO_ABOVE_ROOT_NAVIGATION, &pFile);
```

In addition, any mount points that start with a "/" will be considered absolute and will not allow
any above-root navigation:

```c
fs_mount(pFS, "/usr/share/mygame/gamedata/base", "/gamedata", FS_READ);
```

In the example above, the "/gamedata" mount point starts with a "/", so it will not allow any
above-root navigation which means you cannot navigate above "/usr/share/mygame/gamedata/base". When
opening a file with this kind of mount point, you would need to specify the leading slash:

```c
fs_file_open(pFS, "/gamedata/file.txt", FS_READ, &pFile);   // Note how the path starts with "/".
```

Important: When using mount points that start with "/", if the file cannot be opened from the mount,
it will fall back to trying the actual absolute path. To prevent this and ensure files are only
loaded from the mount point, use the `FS_ONLY_MOUNTS` flag when opening files. Alternatively,
simply avoid using "/" prefixed mounts and instead use `FS_NO_ABOVE_ROOT_NAVIGATION` for security.


You can also mount an archive to a virtual path:

```c
fs_mount(pFS, "/usr/share/mygame/gamedata.zip", "gamedata", FS_READ);
```

In order to do this, the `fs` object must have been configured with support for the given archive
type. Note that writing directly into an archive is not supported by this API. To write into an
archive, the backend itself must support writing, and you will need to manually initialize a `fs`
object for the archive and write into it directly.


The examples above have been hard coding paths, but you can use `fs_mount_sysdir()` to mount a
system directory to a virtual path. This is just a convenience helper function, and you need not
use it if you'd rather deal with system directories yourself:

```c
fs_mount_sysdir(pFS, FS_SYSDIR_CONFIG, "myapp", "/config", FS_READ | FS_WRITE);
```

This function requires that you specify a sub-directory of the system directory to mount. The reason
for this is to encourage the application to use good practice to avoid cluttering the file system.

Use `fs_unmount_sysdir()` to unmount a system directory. When using this you must specify the
sub-directory you used when mounting it:

```c
fs_unmount_sysdir(pFS, FS_SYSDIR_CONFIG, "myapp", FS_READ | FS_WRITE);
```


Mounting a `fs` object to a virtual path is also supported. 

```c
fs* pSomeOtherFS; // <-- This would have been initialized earlier.

fs_mount_fs(pFS, pSomeOtherFS, "assets.zip", FS_READ);

...

fs_unmount_fs(pFS, pSomeOtherFS, FS_READ);
```


If the file cannot be opened from any mounts it will attempt to open the file from the backend's
default search path. Mounts always take priority. When opening in transparent mode with
`FS_TRANSPARENT` (default), it will first try opening the file as if it were not in an archive. If
that fails, it will look inside archives.

When opening a file, if you pass in NULL for the `pFS` parameter it will open the file like normal
using the standard file system. That is, it'll work exactly as if you were using stdio `fopen()`,
and you will not have access to mount points. Keep in mind that there is no notion of a "current
directory" in this library so you'll be stuck with the initial working directory.

You can also skip mount points when opening a file by using the `FS_IGNORE_MOUNTS` flag:

```c
fs_file_open(pFS, "/absolute/path/to/file.txt", FS_READ | FS_IGNORE_MOUNTS, &pFile);
```

This can be useful when you want to access a file directly without going through the mount system,
such as when working with temporary files.



1.4. Enumeration
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

Enumeration is not recursive. If you want to enumerate recursively you will need to do it manually.
You can inspect the `directory` member of the `info` member in `fs_iterator` to determine if the
entry is a directory.


1.5. System Directories
-----------------------
It can often be useful to know the exact paths of known standard system directories, such as the
home directory. You can use the `fs_sysdir()` function for this:

```c
char pPath[256];
size_t pathLen = fs_sysdir(FS_SYSDIR_HOME, pPath, sizeof(pPath));
if (pathLen > 0) {
    if (pathLen < sizeof(pPath)) {
        // Success!
    } else {
        // The buffer was too small. Expand the buffer to at least `pathLen + 1` and try again.
    }
} else {
    // An error occurred.
}
```

`fs_sysdir()` will return the length of the path written to `pPath`, or 0 if an error occurred. If
the buffer is too small, it will return the required size, not including the null terminator.

Recognized system directories include the following:

  - FS_SYSDIR_HOME
  - FS_SYSDIR_TEMP
  - FS_SYSDIR_CONFIG
  - FS_SYSDIR_DATA
  - FS_SYSDIR_CACHE



1.6. Temporary Files
--------------------
You can create a temporary file or folder with `fs_mktmp()`. To create a temporary folder, use the
`FS_MKTMP_DIR` option:

```c
char pTmpPath[256];
fs_result result = fs_mktmp("prefix", pTmpPath, sizeof(pTmpPath), FS_MKTMP_DIR);
if (result != FS_SUCCESS) {
    // Failed to create temporary file.
}
```

Similarly, to create a temporary file, use the `FS_MKTMP_FILE` option:

```c
char pTmpPath[256];
fs_result result = fs_mktmp("prefix", pTmpPath, sizeof(pTmpPath), FS_MKTMP_FILE);
if (result != FS_SUCCESS) {
    // Failed to create temporary file.
}
```

`fs_mktmp()` will create a temporary file or folder with a unique name based on the provided
prefix and will return the full path to the created file or folder in `pTmpPath`. To open the
temporary file, you can pass in the path to `fs_file_open()`, making sure to ignore mount points
with `FS_IGNORE_MOUNTS`:

```c
fs_file* pFile;
result = fs_file_open(pFS, pTmpPath, FS_WRITE | FS_IGNORE_MOUNTS, &pFile);
if (result != FS_SUCCESS) {
    // Failed to open temporary file.
}
```

The prefix can include subdirectories, such as "myapp/subdir". In this case the library will create
the directory hierarchy for you, unless you pass in `FS_NO_CREATE_DIRS`.  Note that not all
platforms treat the name portion of the prefix the same. In particular, Windows will only use up to
the first 3 characters of the name portion of the prefix.

If you don't like the behavior of `fs_mktmp()`, you can consider using `fs_sysdir()` with
`FS_SYSDIR_TEMP` and create the temporary file yourself.



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



3. Platform Considerations
============================

3.1. Windows
--------------
On Windows, Unicode support is determined by the `UNICODE` preprocessor define. When `UNICODE` is
defined, the library will use the wide character versions of Windows APIs. When not defined, it
will use the ANSI versions.

3.2. POSIX
------------
On POSIX platforms, `ftruncate()` is unavailable with `-std=c89` unless `_XOPEN_SOURCE` is defined
to >= 500. This may affect the availability of file truncation functionality when using strict C89
compilation.



4. Backends
===========
You can implement custom backends to support different file systems and archive formats. A POSIX
or Win32 backend is the default backend depending on the platform, and is built into the library.
A backend implements the functions in the `fs_backend` structure.

A ZIP backend is included in the "extras" folder of this library's repository. Refer to this for
a complete example for how to implement a backend (not including write support, but I'm sure
you'll figure it out!). A PAK backend is also included in the "extras" folder, and is simpler than
the ZIP backend which might also be a good place to start.

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


4.1. Backend Functions
----------------------
alloc_size
    This function should return the size of the backend-specific data to associate with the `fs`
    object. If no additional data is required, return 0.
    
    The main library will allocate the `fs` object, including any additional space specified by the
    `alloc_size` function.

init
    This function is called after `alloc_size()` and after the `fs` object has been allocated. This
    is where you should initialize the backend.

    This function will take a pointer to the `fs` object, the backend-specific configuration data,
    and a stream object. The stream is used to provide the backend with the raw data of an archive,
    which will be required for archive backends like ZIP. If your backend requires this, you should
    check for if the stream is null, and if so, return an error. See section "5. Streams" for more
    details on how to use streams. You need not take a copy of the stream pointer for use outside
    of `init`. Instead you can just use `fs_get_stream()` to get the stream object when you need it.
    You should not ever close or otherwise take ownership of the stream - that will be handled
    at a higher level.

uninit
    This is where you should do any cleanup. Do not close the stream here.

ioctl
    This function is optional. You can use this to implement custom IO control commands. Return
    `FS_INVALID_OPERATION` if the command is not recognized. The format of the `pArg` parameter is
    command specific. If the backend does not need to implement this function, it can be left as
    `NULL` or return `FS_NOT_IMPLEMENTED`.

remove
    This function is used to delete a file or directory. This is not recursive. If the path is
    a directory, the backend should return an error if it is not empty. Backends do not need to
    implement this function in which case they can leave the callback pointer as `NULL`, or have
    it return `FS_NOT_IMPLEMENTED`.

rename
    This function is used to rename a file. This will act as a move if the source and destination
    are in different directories. If the destination already exists, it should be overwritten. This
    function is optional and can be left as `NULL` or return `FS_NOT_IMPLEMENTED`.

mkdir
    This function is used to create a directory. This is not recursive. If the directory already
    exists, the backend should return `FS_ALREADY_EXISTS`. If a parent directory does not exist,
    the backend should return `FS_DOES_NOT_EXIST`. This function is optional and can be left as
    `NULL` or return `FS_NOT_IMPLEMENTED`.

info
    This function is used to get information about a file. If the backend does not have the notion
    of the last modified or access time, it can set those values to 0. Set `directory` to 1 (or
    `FS_TRUE`) if it's a directory. Likewise, set `symlink` to 1 if it's a symbolic link. It is
    important that this function return the info of the exact file that would be opened with
    `file_open()`. This function is mandatory.

file_alloc_size
    Like when initializing a `fs` object, the library needs to know how much backend-specific data
    to allocate for the `fs_file` object. This is done with the `file_alloc_size` function. This
    function is basically the same as `alloc_size` for the `fs` object, but for `fs_file`. If the
    backend does not need any additional data, it can return 0. The backend can access this data
    via `fs_file_get_backend_data()`.

file_open
    The `file_open` function is where the backend should open the file.
    
    If the `fs` object that owns the file was initialized with a stream, the stream will be
    non-null. If your backends requires a stream, you should check that the stream is null, and if
    so, return an error. The reason you need to check for this is that the application itself may
    erroneously attempt to initialize a `fs` object for your backend without a stream, and since
    the library cannot know whether or not a backend requires a stream, it cannot check this on
    your behalf.

    If the backend requires a stream, it should take a copy of only the pointer and store it for
    later use. Do *not* make a duplicate of the stream with `fs_stream_duplicate()`.

    Backends need only handle the following open mode flags:

        FS_READ
        FS_WRITE
        FS_TRUNCATE
        FS_APPEND
        FS_EXCLUSIVE

    All other flags are for use at a higher level and should be ignored.

    When opening in write mode (`FS_WRITE`), the backend should default to overwrite mode. If
    `FS_TRUNCATE` is specified, the file should be truncated to 0 length. If `FS_APPEND` is
    specified, all writes should happen at the end of the file regardless of the position of the
    write cursor. If `FS_EXCLUSIVE` is specified, opening should fail if the file already exists.
    In all write modes,  the file should be created if it does not already exist.

    If any flags cannot be supported, the backend should return an error.

    When opening in read mode, if the file does not exist, `FS_DOES_NOT_EXIST` should be returned.
    Similarly, if the file is a directory, `FS_IS_DIRECTORY` should be returned.

    Before calling into this function, the library will normalize all paths to use forward slashes.
    Therefore, backends must support forward slashes ("/") as path separators.

file_close
    This function is where the file should be closed. This is where the backend should release any
    resources associated with the file. Do not uninitialize the stream here - it'll be cleaned up
    at a higher level.

file_read
    This is used to read data from the file. The backend should return `FS_AT_END` when the end of
    the file is reached, but only if the number of bytes read is 0.

file_write
    This is used to write data to the file. If the file is opened in append mode, the backend
    should always ensure writes are appended to the end, regardless of the position of the write
    cursor. This is optional and need only be specified if the backend supports writing.

file_seek
    The `file_seek` function is used to seek the read/write cursor. The backend should allow
    seeking beyond the end of the file. If the file is opened in write mode and data is written
    beyond the end of the file, the file should be extended, and if possible made into a sparse
    file. If sparse files are not supported, the backend should fill the gap with data, preferably
    with zeros if possible.

    Attempting to seek to before the start of the file should return `FS_BAD_SEEK`.

file_tell
    The `file_tell` function is used to get the current cursor position. There is only one cursor,
    even when the file is opened in read and write mode.

file_flush
    The `file_flush` function is used to flush any buffered data to the file. This is optional and
    can be left as `NULL` or return `FS_NOT_IMPLEMENTED`.

file_truncate
    The `file_truncate` function is used to truncate a file to the current cursor position. This is
    only useful for write mode, so is therefore optional and can be left as `NULL` or return
    `FS_NOT_IMPLEMENTED`.

file_info
    The `file_info` function is used to get information about an opened file. It returns the same
    information as `info` but for an opened file. This is mandatory.

file_duplicate
    The `file_duplicate` function is used to duplicate a file. The destination file will be a new
    file and already allocated. The backend need only copy the necessary backend-specific data to
    the new file. The backend must ensure that the duplicated file is totally independent of the
    original file and has its own independent read/write pointer. If the backend is unable to
    support duplicated files having their own independent read/write pointer, it must return an
    error.

    If a backend cannot support duplication, it can leave this as `NULL` or return
    `FS_NOT_IMPLEMENTED`. However, if this is not implemented, the backend will not be able to open
    files within archives.

first, next, free_iterator
    The `first`, `next` and `free_iterator` functions are used to enumerate the contents of a
    directory. If the directory is empty, or an error occurs, `fs_first` should return `NULL`. The
    `next` function should return `NULL` when there are no more entries. When `next` returns
    `NULL`, the backend needs to free the iterator object. The `free_iterator` function is used to
    free the iterator object explicitly. The backend is responsible for any memory management of
    the name string. A typical way to deal with this is to allocate additional space for the name
    immediately after the `fs_iterator` allocation.


4.2. Thread Safety
------------------
Backends are responsible for guaranteeing thread-safety of different files across different
threads. This should typically be quite easy since most system backends, such as stdio, are already
thread-safe, and archive backends are typically read-only which should make thread-safety trivial
on that front as well. You need not worry about thread-safety of a single individual file handle.
But when you have two different file handles, they must be able to be used on two different threads
at the same time.


5. Streams
==========
Streams are the data delivery mechanism for archive backends. You can implement custom streams, but
this should be uncommon because `fs_file` itself is a stream, and a memory stream is included in
the library called `fs_memory_stream`. Between these two the majority of use cases should be
covered. You can retrieve the stream associated with a `fs_file` using `fs_file_get_stream()`.

A stream is initialized using a specialized initialization function depending on the stream type.
For `fs_file`, simply opening the file is enough. For `fs_memory_stream`, you need to call
`fs_memory_stream_init_readonly()` for a standard read-only stream, or
`fs_memory_stream_init_write()` for a stream with write (and read) support. If you want to
implement your own stream type you would need to implement a similar initialization function.

Use `fs_stream_read()` and `fs_stream_write()` to read and write data from a stream. If the stream
does not support reading or writing, the respective function will return `FS_NOT_IMPLEMENTED`.

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
A duplicated stream is entirely independent of the original stream and can be used on a different
thread to the original stream.

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

#if defined(__cplusplus)
extern "C" {
#endif

/* BEG fs_compiler_compat.h */
#include <stddef.h> /* For size_t. */
#include <stdarg.h> /* For va_list. */

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
typedef enum
{
    FS_SUCCESS                       =  0,
    FS_ERROR                         = -1,  /* Generic, unknown error. */
    FS_INVALID_ARGS                  = -2,
    FS_INVALID_OPERATION             = -3,
    FS_OUT_OF_MEMORY                 = -4,
    FS_OUT_OF_RANGE                  = -5,
    FS_ACCESS_DENIED                 = -6,
    FS_DOES_NOT_EXIST                = -7,
    FS_ALREADY_EXISTS                = -8,
    FS_TOO_MANY_OPEN_FILES           = -9,
    FS_INVALID_FILE                  = -10,
    FS_TOO_BIG                       = -11,
    FS_PATH_TOO_LONG                 = -12,
    FS_NAME_TOO_LONG                 = -13,
    FS_NOT_DIRECTORY                 = -14,
    FS_IS_DIRECTORY                  = -15,
    FS_DIRECTORY_NOT_EMPTY           = -16,
    FS_AT_END                        = -17,
    FS_NO_SPACE                      = -18,
    FS_BUSY                          = -19,
    FS_IO_ERROR                      = -20,
    FS_INTERRUPT                     = -21,
    FS_UNAVAILABLE                   = -22,
    FS_ALREADY_IN_USE                = -23,
    FS_BAD_ADDRESS                   = -24,
    FS_BAD_SEEK                      = -25,
    FS_BAD_PIPE                      = -26,
    FS_DEADLOCK                      = -27,
    FS_TOO_MANY_LINKS                = -28,
    FS_NOT_IMPLEMENTED               = -29,
    FS_NO_MESSAGE                    = -30,
    FS_BAD_MESSAGE                   = -31,
    FS_NO_DATA_AVAILABLE             = -32,
    FS_INVALID_DATA                  = -33,
    FS_TIMEOUT                       = -34,
    FS_NO_NETWORK                    = -35,
    FS_NOT_UNIQUE                    = -36,
    FS_NOT_SOCKET                    = -37,
    FS_NO_ADDRESS                    = -38,
    FS_BAD_PROTOCOL                  = -39,
    FS_PROTOCOL_UNAVAILABLE          = -40,
    FS_PROTOCOL_NOT_SUPPORTED        = -41,
    FS_PROTOCOL_FAMILY_NOT_SUPPORTED = -42,
    FS_ADDRESS_FAMILY_NOT_SUPPORTED  = -43,
    FS_SOCKET_NOT_SUPPORTED          = -44,
    FS_CONNECTION_RESET              = -45,
    FS_ALREADY_CONNECTED             = -46,
    FS_NOT_CONNECTED                 = -47,
    FS_CONNECTION_REFUSED            = -48,
    FS_NO_HOST                       = -49,
    FS_IN_PROGRESS                   = -50,
    FS_CANCELLED                     = -51,
    FS_MEMORY_ALREADY_MAPPED         = -52,
    FS_DIFFERENT_DEVICE              = -53,
    FS_CHECKSUM_MISMATCH             = -100,
    FS_NO_BACKEND                    = -101,

    /* Non-Error Result Codes. */
    FS_NEEDS_MORE_INPUT              = 100, /* Some stream needs more input data before it can be processed. */
    FS_HAS_MORE_OUTPUT               = 102  /* Some stream has more output data to be read, but there's not enough room in the output buffer. */
} fs_result;

FS_API const char* fs_result_to_string(fs_result result);
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
    /* BEG fs_stream_vtable_duplicate */
    size_t    (* duplicate_alloc_size)(fs_stream* pStream);                                 /* Optional. Returns the allocation size of the stream. When not defined, duplicating is disabled. */
    fs_result (* duplicate           )(fs_stream* pStream, fs_stream* pDuplicatedStream);   /* Optional. Duplicate the stream. */
    void      (* uninit              )(fs_stream* pStream);                                 /* Optional. Uninitialize the stream. */
    /* END fs_stream_vtable_duplicate */
};

struct fs_stream
{
    const fs_stream_vtable* pVTable;
};

FS_API fs_result fs_stream_init(const fs_stream_vtable* pVTable, fs_stream* pStream);
FS_API fs_result fs_stream_read(fs_stream* pStream, void* pDst, size_t bytesToRead, size_t* pBytesRead);
FS_API fs_result fs_stream_write(fs_stream* pStream, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
FS_API fs_result fs_stream_seek(fs_stream* pStream, fs_int64 offset, fs_seek_origin origin);
FS_API fs_result fs_stream_tell(fs_stream* pStream, fs_int64* pCursor);

/* BEG fs_stream_writef.h */
FS_API fs_result fs_stream_writef(fs_stream* pStream, const char* fmt, ...) FS_ATTRIBUTE_FORMAT(2, 3);
FS_API fs_result fs_stream_writef_ex(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, const char* fmt, ...) FS_ATTRIBUTE_FORMAT(3, 4);
FS_API fs_result fs_stream_writefv(fs_stream* pStream, const char* fmt, va_list args);
FS_API fs_result fs_stream_writefv_ex(fs_stream* pStream, const fs_allocation_callbacks* pAllocationCallbacks, const char* fmt, va_list args);
/* END fs_stream_writef.h */

/* BEG fs_stream_duplicate.h */
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
/* END fs_stream_duplicate.h */

/* BEG fs_stream_helpers.h */
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
/* END fs_stream_helpers.h */
/* END fs_stream.h */


/* BEG fs_sysdir.h */
typedef enum fs_sysdir_type
{
    FS_SYSDIR_HOME,
    FS_SYSDIR_TEMP,
    FS_SYSDIR_CONFIG,
    FS_SYSDIR_DATA,
    FS_SYSDIR_CACHE
} fs_sysdir_type;

/*
Get the path of a known system directory.

The returned path will be null-terminated. If the output buffer is too small, the required size
will be returned, not including the null terminator.


Parameters
----------
type : (in)
    The type of system directory to query. See `fs_sysdir_type` for recognized values.

pDst : (out, optional)
    A pointer to a buffer that will receive the path. If NULL, the function will return the
    required length of the buffer, not including the null terminator.

dstCap : (in)
    The capacity of the output buffer, in bytes. This is ignored if `pDst` is NULL.


Return Value
------------
Returns the length of the string, not including the null terminator. Returns 0 on failure. If the
return value is >= to `dstCap` it means the output buffer was too small. Use the returned value to
know how big to make the buffer.


Example 1 - Querying the Home Directory
---------------------------------------
```c
size_t len = fs_sysdir(FS_SYSDIR_HOME, NULL, 0);
if (len == 0) {
    // Failed to query the length of the home directory path.
}

char* pPath = (char*)malloc(len + 1);  // +1 for null terminator.
if (pPath == NULL) {
    // Out of memory.
}

len = fs_sysdir(FS_SYSDIR_HOME, pPath, len + 1);
if (len == 0) {
    // Failed to get the home directory path.
}

printf("Home directory: %s\n", pPath);
free(pPath);
```


See Also
--------
fs_sysdir_type
fs_mktmp()
*/
FS_API size_t fs_sysdir(fs_sysdir_type type, char* pDst, size_t dstCap);
/* END fs_sysdir.h */


/* BEG fs_mktmp.h */
/*
Create a temporary file or directory.

This function creates a temporary file or directory with a unique name based on the provided
prefix. The full path to the created file or directory is returned in `pTmpPath`.

Use the option flag `FS_MKTMP_FILE` to create a temporary file, or `FS_MKTMP_DIR` to create a
temporary directory.


Parameters
----------
pPrefix : (in)
    A prefix for the temporary file or directory name. This should not include the system's base
    temp directory path. Do not include paths like "/tmp" in the prefix. The output path will
    include the system's base temp directory and the prefix.

    The prefix can include subdirectories, such as "myapp/subdir". In this case the library will
    create the directory hierarchy for you, unless you pass in `FS_NO_CREATE_DIRS`. Note that not
    all platforms treat the name portion of the prefix the same. In particular, Windows will only
    use up to the first 3 characters of the name portion of the prefix.

pTmpPath : (out)
    A pointer to a buffer that will receive the full path of the created temporary file or
    directory. This will be null-terminated.

tmpPathCap : (in)
    The capacity of the output buffer, in bytes.

options : (in)
    Options for creating the temporary file or directory. Can be a combination of the following:
        FS_MKTMP_FILE
            Creates a temporary file. Cannot be used with FS_MKTMP_DIR.

        FS_MKTMP_DIR
            Creates a temporary directory. Cannot be used with FS_MKTMP_FILE.

        FS_NO_CREATE_DIRS
            Do not create parent directories if they do not exist. If this flag is not set,
            parent directories will be created as needed.


Return Value
------------
Returns `FS_SUCCESS` on success; any other error code on failure. Will return `FS_PATH_TOO_LONG` if
the output buffer is too small.
*/
FS_API fs_result fs_mktmp(const char* pPrefix, char* pTmpPath, size_t tmpPathCap, int options);  /* Returns FS_PATH_TOO_LONG if the output buffer is too small. Use FS_MKTMP_FILE to create a file and FS_MKTMP_DIR to create a directory. pPrefix should not include the name of the system's base temp directory. Do not include paths like "/tmp" in the prefix. The output path will include the system's base temp directory and the prefix. */
/* END fs_mktmp.h */


/* BEG fs.h */
/**************************************************************************************************

Open Mode Flags

**************************************************************************************************/
#define FS_READ                     0x0001  /* Used by: fs_file_open(), fs_info(), fs_first(), fs_open_archive*(), fs_mount*() */
#define FS_WRITE                    0x0002  /* Used by: fs_file_open(), fs_info(), fs_first(), fs_open_archive*(), fs_mount*() */
#define FS_TRUNCATE                 0x0004  /* Used by: fs_file_open() */
#define FS_APPEND                   0x0008  /* Used by: fs_file_open() */
#define FS_EXCLUSIVE                0x0010  /* Used by: fs_file_open() */

#define FS_TRANSPARENT              0x0000  /* Used by: fs_file_open(), fs_info(), fs_first() */
#define FS_OPAQUE                   0x0020  /* Used by: fs_file_open(), fs_info(), fs_first() */
#define FS_VERBOSE                  0x0040  /* Used by: fs_file_open(), fs_info(), fs_first() */

#define FS_NO_CREATE_DIRS           0x0080  /* Used by: fs_file_open(), fs_info(), fs_mount(), fs_mkdir(), fs_mktmp() */
#define FS_IGNORE_MOUNTS            0x0100  /* Used by: fs_file_open(), fs_info(), fs_first() */
#define FS_ONLY_MOUNTS              0x0200  /* Used by: fs_file_open(), fs_info(), fs_first() */
#define FS_NO_SPECIAL_DIRS          0x0400  /* Used by: fs_file_open(), fs_info(), fs_first() */
#define FS_NO_ABOVE_ROOT_NAVIGATION 0x0800  /* Used by: fs_file_open(), fs_info(), fs_first() */

#define FS_LOWEST_PRIORITY          0x1000  /* Used by: fs_mount*() */

#define FS_MKTMP_DIR                0x2000  /* Used by: fs_mktmp() */
#define FS_MKTMP_FILE               0x4000  /* Used by: fs_mktmp() */

#define FS_NO_INCREMENT_REFCOUNT    0x8000  /* Do not use. Internal use only. Used with fs_open_archive_ex() internally. */


/* Garbage collection policies.*/
#define FS_GC_POLICY_THRESHOLD      0x0001  /* Only garbage collect unreferenced opened archives until the count is below the configured threshold. */
#define FS_GC_POLICY_FULL           0x0002  /* Garbage collect every unreferenced opened archive, regardless of how many are open.*/


typedef struct fs_config    fs_config;
typedef struct fs           fs;
typedef struct fs_file      fs_file;
typedef struct fs_file_info fs_file_info;
typedef struct fs_iterator  fs_iterator;
typedef struct fs_backend   fs_backend;


/* File paths for stdin, stdout and stderr. Use these with fs_file_open(). */
extern const char* FS_STDIN;
extern const char* FS_STDOUT;
extern const char* FS_STDERR;


/*
This callback is fired when the reference count of a fs object changes. This is useful if you want
to do some kind of advanced memory management, such as garbage collection. If the new reference count
is 1, it means no other objects are referencing the fs object.
*/
typedef void (* fs_on_refcount_changed_proc)(void* pUserData, fs* pFS, fs_uint32 newRefCount, fs_uint32 oldRefCount);

typedef struct fs_archive_type
{
    const fs_backend* pBackend;
    const char* pExtension;
} fs_archive_type;

FS_API fs_archive_type fs_archive_type_init(const fs_backend* pBackend, const char* pExtension);


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
    fs_on_refcount_changed_proc onRefCountChanged;
    void* pRefCountChangedUserData;
    const fs_allocation_callbacks* pAllocationCallbacks;
};

FS_API fs_config fs_config_init_default(void);
FS_API fs_config fs_config_init(const fs_backend* pBackend, const void* pBackendConfig, fs_stream* pStream);


struct fs_backend
{
    size_t       (* alloc_size      )(const void* pBackendConfig);
    fs_result    (* init            )(fs* pFS, const void* pBackendConfig, fs_stream* pStream);              /* Return 0 on success or an errno result code on error. pBackendConfig is a pointer to a backend-specific struct. The documentation for your backend will tell you how to use this. You can usually pass in NULL for this. */
    void         (* uninit          )(fs* pFS);
    fs_result    (* ioctl           )(fs* pFS, int op, void* pArg);                                          /* Optional. */
    fs_result    (* remove          )(fs* pFS, const char* pFilePath);
    fs_result    (* rename          )(fs* pFS, const char* pOldPath, const char* pNewPath);                  /* Return FS_DIFFERENT_DEVICE if the old and new paths are on different devices and would require a copy. */
    fs_result    (* mkdir           )(fs* pFS, const char* pPath);                                           /* This is not recursive. Return FS_ALREADY_EXISTS if directory already exists. Return FS_DOES_NOT_EXIST if a parent directory does not exist. */
    fs_result    (* info            )(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo);        /* openMode flags can be ignored by most backends. It's primarily used by passthrough style backends. */
    size_t       (* file_alloc_size )(fs* pFS);
    fs_result    (* file_open       )(fs* pFS, fs_stream* pStream, const char* pFilePath, int openMode, fs_file* pFile); /* Return 0 on success or an errno result code on error. Return FS_DOES_NOT_EXIST if the file does not exist. pStream will be null if the backend does not need a stream (the `pFS` object was not initialized with one). */
    void         (* file_close      )(fs_file* pFile);
    fs_result    (* file_read       )(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead);   /* Return 0 on success, or FS_AT_END on end of file. Only return FS_AT_END if *pBytesRead is 0. Return an errno code on error. Implementations must support reading when already at EOF, in which case FS_AT_END should be returned and *pBytesRead should be 0. */
    fs_result    (* file_write      )(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);
    fs_result    (* file_seek       )(fs_file* pFile, fs_int64 offset, fs_seek_origin origin);
    fs_result    (* file_tell       )(fs_file* pFile, fs_int64* pCursor);
    fs_result    (* file_flush      )(fs_file* pFile);
    fs_result    (* file_truncate   )(fs_file* pFile);
    fs_result    (* file_info       )(fs_file* pFile, fs_file_info* pInfo);
    fs_result    (* file_duplicate  )(fs_file* pFile, fs_file* pDuplicate);                                  /* Duplicate the file handle. */
    fs_iterator* (* first           )(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen);
    fs_iterator* (* next            )(fs_iterator* pIterator);  /* <-- Must return null when there are no more files. In this case, free_iterator must be called internally. */
    void         (* free_iterator   )(fs_iterator* pIterator);  /* <-- Free the `fs_iterator` object here since `first` and `next` were the ones who allocated it. Also do any uninitialization routines. */
};

/*
Initializes a file system object.

This is the main object that you will use to open files. There are different types of file system
backends, such as the standard file system, ZIP archives, etc. which you can configure via the
config.

The config is used to select which backend to use and to register archive types against known
file extensions. If you just want to use the regular file system and don't care about archives,
you can just pass in NULL for the config.

By registering archive types, you'll be able to open files from within them straight from a file
path without without needing to do any manual management. For example, if you register ZIP archives
to the ".zip" extension, you can open a file from a path like this:

    somefolder/archive.zip/somefile.txt

These can also be handled transparently, so the above path can be opened with this:

    somefolder/somefile.txt

Note that the `archive.zip` part is not needed. If you want this functionality, you must register
the archive types with the config.

Most of the time you will use a `fs` object that represents the normal file system, which is the
default backend if you don't pass in a config, but sometimes you may want to have a `fs` object
that represents an archive, such as a ZIP archive. To do this, you need to provide a stream that
reads the actual data of the archive. Most of the time you will just use the stream provided by
a `fs_file` object you opened earlier from the regular file system, but if you would rather source
your data from elsewhere, like a memory buffer, you can pass in your own stream. You also need to
specify the backend to use, such as `FS_ZIP` in the case of ZIP archives. See examples below for
more information.

If you want to use custom allocation callbacks, you can do so by passing in a pointer to a
`fs_allocation_callbacks` struct into the config. If you pass in NULL, the default allocation
callbacks which use malloc/realloc/free will be used. If you pass in non-NULL, this function will
make a copy of the struct, so you can free or modify the struct after this function returns.


Parameters
----------
pConfig : (in, optional)
    A pointer to a configuration struct. Can be NULL, in which case the regular file system will be
    used, and archives will not be supported unless explicitly mounted later with `fs_mount_fs()`.

ppFS : (out)
    A pointer to a pointer which will receive the initialized file system object. The object must
    be uninitialized with `fs_uninit()` when no longer needed.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.


Example 1 - Basic Usage
-----------------------
The example below shows how to initialize a `fs` object which uses the regular file system and does
not support archives. This is the most basic usage of the `fs` object.

```c
#include "fs.h"

...

fs* pFS;
fs_result result = fs_init(NULL, &pFS);
if (result != FS_SUCCESS) {
    // Handle error.
}

...

fs_uninit(pFS);
```


Example 2 - Supporting Archives
-------------------------------
The example below shows how to initialize a `fs` object which uses the regular file system and
supports ZIP archives. Error checking has been omitted for clarity.

```c
#include "fs.h"
#include "extras/backends/zip/fs_zip.h"  // For FS_ZIP backend.

...

fs* pFS;
fs_config fsConfig;

// Archive types are supported by mapping a backend (`FS_ZIP` in this case) to a file extension.
fs_archive_type pArchiveTypes[] =
{
    {FS_ZIP, "zip"}
};

// The archive types are registered via the config.
fsConfig = fs_config_init_default();
fsConfig.pArchiveTypes    = pArchiveTypes;
fsConfig.archiveTypeCount = sizeof(pArchiveTypes) / sizeof(pArchiveTypes[0]);

// Once the config is ready, initialize the fs object.
fs_init(&fsConfig, &pFS);

// Now you can open files from within ZIP archives from a file path.
fs_file* pFileInArchive;
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ, &pFileInArchive);
```


Example 3 - Archive Backends
----------------------------
This example shows how you can open an archive file directly, and then create a new `fs` object
which uses the archive as its backend. This is useful if, for example, you want to use a ZIP file
as a virtual file system.

```c
#include "fs.h"
#include "extras/backends/zip/fs_zip.h"  // For FS_ZIP backend.

...

fs* pFS;                // Main file system object.
fs* pArchiveFS;         // File system object for the archive.
fs_config archiveConfig;
fs_file* pArchiveFile;  // The actual archive file.

// Open the archive file itself first, usually from the regular file system.
fs_file_open(pFS, "somefolder/archive.zip", FS_READ, &pArchiveFile);

...

// Setup the config for the archive `fs` object such that it uses the ZIP backend (FS_ZIP), and
// reads from the stream of the actual archive file (pArchiveFile) which was opened earlier.
archiveConfig = fs_config_init(FS_ZIP, NULL, fs_file_get_stream(pArchiveFile));

// With the config ready we can now initialize the `fs` object for the archive.
fs_init(&archiveConfig, &pArchiveFS);

...

// Now that we have a `fs` object representing the archive, we can open files from within it like
// normal.
fs_file* pFileInArchive;
fs_file_open(pArchiveFS, "fileinsidearchive.txt", FS_READ, &pFileInArchive);
```


See Also
--------
fs_uninit()
*/
FS_API fs_result fs_init(const fs_config* pConfig, fs** ppFS);


/*
Uninitializes a file system object.

This does not do any file closing for you. You must close any opened files yourself before calling
this function.


Parameters
----------
pFS : (in)
    A pointer to the file system object to uninitialize. Must not be NULL.


See Also
--------
fs_init()
*/
FS_API void fs_uninit(fs* pFS);


/*
Performs a control operation on the file system.

This is backend-specific. Check the documentation for the backend you are using to see what
operations are supported.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

op : (in)
    The operation to perform. This is backend-specific.

pArg : (in, optional)
    An optional pointer to an argument struct. This is backend-specific. Can be NULL if the
    operation does not require any arguments.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. May return FS_NOT_IMPLEMENTED if
the operation is not supported by the backend.
*/
FS_API fs_result fs_ioctl(fs* pFS, int op, void* pArg);


/*
Removes a file or empty directory.

This function will delete a file or an empty directory from the file system. It will consider write
mount points unless the FS_IGNORE_MOUNTS flag is specified in the options parameter in which case
the path will be treated as a real path.

See fs_file_open() for information about the options flags.


Parameters
----------
pFS : (in, optional)
    A pointer to the file system object. Can be NULL to use the native file system.

pFilePath : (in)
    The path to the file or directory to remove. Must not be NULL.

options : (in)
    Options for the operation. Can be 0 or a combination of the following flags:
        FS_IGNORE_MOUNTS
        FS_NO_SPECIAL_DIRS
        FS_NO_ABOVE_ROOT_NAVIGATION


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
file or directory does not exist. Returns FS_DIRECTORY_NOT_EMPTY if attempting to remove a
non-empty directory.


See Also
--------
fs_file_open()
*/
FS_API fs_result fs_remove(fs* pFS, const char* pFilePath, int options);


/*
Renames or moves a file or directory.

This function will rename or move a file or directory from one location to another. It will
consider write mount points unless the FS_IGNORE_MOUNTS flag is specified in the options parameter
in which case the paths will be treated as real paths.

This will fail with FS_DIFFERENT_DEVICE if the source and destination are on different devices.

See fs_file_open() for information about the options flags.


Parameters
----------
pFS : (in, optional)
    A pointer to the file system object. Can be NULL to use the native file system.

pOldPath : (in)
    The current path of the file or directory to rename/move. Must not be NULL.

pNewPath : (in)
    The new path for the file or directory. Must not be NULL.

options : (in)
    Options for the operation. Can be 0 or a combination of the following flags:
        FS_IGNORE_MOUNTS
        FS_NO_SPECIAL_DIRS
        FS_NO_ABOVE_ROOT_NAVIGATION


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
source file or directory does not exist. Returns FS_ALREADY_EXISTS if the destination path already
exists.


See Also
--------
fs_file_open()
*/
FS_API fs_result fs_rename(fs* pFS, const char* pOldPath, const char* pNewPath, int options);


/*
Creates a directory.

This function creates a directory at the specified path. By default, it will create the entire
directory hierarchy if parent directories do not exist. It will consider write mount points unless
the FS_IGNORE_MOUNTS flag is specified in the options parameter in which case the path will be
treated as a real path.

See fs_file_open() for information about the options flags.


Parameters
----------
pFS : (in, optional)
    A pointer to the file system object. Can be NULL to use the native file system.

pPath : (in)
    The path of the directory to create. Must not be NULL.

options : (in)
    Options for the operation. Can be 0 or a combination of the following flags:
        FS_IGNORE_MOUNTS
        FS_NO_CREATE_DIRS


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_ALREADY_EXISTS if the
directory already exists. Returns FS_DOES_NOT_EXIST if FS_NO_CREATE_DIRS is specified and a
parent directory does not exist.


See Also
--------
fs_file_open()
*/
FS_API fs_result fs_mkdir(fs* pFS, const char* pPath, int options);


/*
Retrieves information about a file or directory without opening it.

This function gets information about a file or directory such as its size, modification time,
and whether it is a directory or symbolic link. The openMode parameter accepts the same flags as
fs_file_open() but FS_READ, FS_WRITE, FS_TRUNCATE, FS_APPEND, and FS_EXCLUSIVE are ignored.


Parameters
----------
pFS : (in, optional)
    A pointer to the file system object. Can be NULL to use the native file system.

pPath : (in)
    The path to the file or directory to get information about. Must not be NULL.

openMode : (in)
    Open mode flags that may affect how the file is accessed. See fs_file_open() for details.

pInfo : (out)
    A pointer to a fs_file_info structure that will receive the file information. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
file or directory does not exist.


See Also
--------
fs_file_get_info()
fs_file_open()
*/
FS_API fs_result fs_info(fs* pFS, const char* pPath, int openMode, fs_file_info* pInfo);


/*
Retrieves a pointer to the stream used by the file system object.

This is only relevant if the file system will initialized with a stream (such as when opening an
archive). If the file system was not initialized with a stream, this will return NULL.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns a pointer to the stream used by the file system object, or NULL if no stream was provided
at initialization time.
*/
FS_API fs_stream* fs_get_stream(fs* pFS);


/*
Retrieves a pointer to the allocation callbacks used by the file system object.

Note that this will *not* return the same pointer that was specified in the config at initialization
time. This function returns a pointer to the internal copy of the struct.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns a pointer to the allocation callbacks used by the file system object. If `pFS` is NULL, this
will return NULL.
*/
FS_API const fs_allocation_callbacks* fs_get_allocation_callbacks(fs* pFS);


/*
For use only by backend implementations. Retrieves a pointer to the backend-specific data
associated with the file system object.

You should never call this function unless you are implementing a custom backend. The size of the
data can be retrieved with `fs_get_backend_data_size()`.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns a pointer to the backend-specific data associated with the file system object, or NULL if no
backend data is available.


See Also
--------
fs_get_backend_data_size()
*/
FS_API void* fs_get_backend_data(fs* pFS);


/*
For use only by backend implementations. Retrieves the size of the backend-specific data
associated with the file system object.

You should never call this function unless you are implementing a custom backend. The data can be
accessed with `fs_get_backend_data()`.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns the size of the backend-specific data associated with the file system object, or 0 if no
backend data is available.


See Also
--------
fs_get_backend_data()
*/
FS_API size_t fs_get_backend_data_size(fs* pFS);


/*
Increments the reference count of the file system object.

This function would be used to prevent garbage collection of opened archives. It should be rare to
ever need to call this function directly.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns `pFS`.


See Also
--------
fs_unref()
fs_refcount()
*/
FS_API fs* fs_ref(fs* pFS);

/*
Decrements the reference count of the file system object.

This does not uninitialize the object once the reference count hits zero.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns the new reference count.


See Also
--------
fs_ref()
fs_refcount()
*/
FS_API fs_uint32 fs_unref(fs* pFS);

/*
Retrieves the current reference count of the file system object.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns the current reference count of the file system object.


See Also
--------
fs_ref()
fs_unref()
*/
FS_API fs_uint32 fs_refcount(fs* pFS);


/*
Opens a file.

If the file path is prefixed with the virtual path of a mount point, this function will first try
opening the file from that mount. If that fails, it will fall back to the native file system and
treat the path as a real path. If the FS_ONLY_MOUNTS flag is specified in the openMode parameter,
the last step of falling back to the native file system will be skipped.

By default, opening a file will transparently look inside archives of known types (registered at
initialization time of the `fs` object). This can slow, and if you would rather not have this
behavior, consider using the `FS_OPAQUE` option (see below).

This function opens a file for reading and/or writing. The openMode parameter specifies how the
file should be opened. It can be a combination of the following flags:

FS_READ
    Open the file for reading. If used with `FS_WRITE`, the file will be opened in read/write mode.
    When opening in read-only mode, the file must exist.

FS_WRITE
    Open the file for writing. If used with `FS_READ`, the file will be opened in read/write mode.
    When opening in write-only mode, the file will be created if it does not exist. By default, the
    file will be opened in overwrite mode. To change this, combine this with either one of
    the `FS_TRUNCATE` or `FS_APPEND` flags.

FS_TRUNCATE
    Only valid when used with `FS_WRITE`. If the file already exists, it will be truncated to zero
    length when opened. If the file does not exist, it will be created. Not compatible with
    `FS_APPEND`.

FS_APPEND
    Only valid when used with `FS_WRITE`. All writes will occur at the end of the file, regardless
    of the current cursor position. If the file does not exist, it will be created. Not compatible
    with `FS_TRUNCATE`.

FS_EXCLUSIVE
    Only valid when used with `FS_WRITE`. The file will be created, but if it already exists, the
    open will fail with FS_ALREADY_EXISTS.

FS_TRANSPARENT
    This is the default behavior. When used, files inside archives can be opened transparently. For
    example, "somefolder/archive.zip/file.txt" can be opened with "somefolder/file.txt" (the
    "archive.zip" part need not be specified). This assumes the `fs` object has been initialized
    with support for the relevant archive types.

    Transparent mode is the slowest mode since it requires searching through the file system for
    archives, and then open those archives, and then searching through the archive for the file. If
    this is prohibitive, consider using `FS_OPAQUE` (fastest) or `FS_VERBOSE` modes instead.
    Furthermore, you can consider having a rule in your application that instead of opening files
    inside archives from a transparent path, that you instead mount the archive, and then open all
    files with `FS_OPAQUE`, but with a virtual path that points to the archive. For example:

        fs_mount(pFS, "somefolder/archive.zip", "assets", FS_READ);
        fs_file_open(pFS, "assets/file.txt", FS_READ | FS_OPAQUE, &pFile);

    Here the archive is mounted to the virtual path "assets". Because the path "assets/file.txt"
    is prefixed with "assets", the file system knows to look inside the mounted archive without
    having to search for it.

FS_OPAQUE
    When used, archives will be treated as opaque, meaning attempting to open a file from an
    unmounted archive will fail. For example, "somefolder/archive.zip/file.txt" will fail because
    it is inside an archive. This is the fastest mode, but you will not be able to open files from
    inside archives unless it is sourced from a mount.

FS_VERBOSE
    When used, files inside archives can be opened, but the name of the archive must be specified
    explicitly in the path, such as "somefolder/archive.zip/file.txt". This is faster than
    `FS_TRANSPARENT` mode since it does not require searching for archives.

FS_NO_CREATE_DIRS
    When opening a file in write mode, the default behavior is to create the directory structure
    automatically if required. When this options is used, directories will *not* be created
    automatically. If the necessary parent directories do not exist, the open will fail with
    FS_DOES_NOT_EXIST.

FS_IGNORE_MOUNTS
    When used, mounted directories and archives will be ignored when opening files. The path will
    be treated as a real path.

FS_ONLY_MOUNTS
    When used, only mounted directories and archives will be considered when opening files. When a
    file is opened, it will first search through mounts, and if the file is not found in any of
    those it will fall back to the native file system and try treating the path as a real path.
    When this flag is set, that last step of falling back to the native file system is skipped.

FS_NO_SPECIAL_DIRS
    When used, the presence of special directories like "." and ".." in the path will result in an
    error. When using this option, you need not specify FS_NO_ABOVE_ROOT_NAVIGATION since it is
    implied.

FS_NO_ABOVE_ROOT_NAVIGATION
    When used, navigating above the mount point with leading ".." segments will result in an error.
    This option is implied when using FS_NO_SPECIAL_DIRS.


Close the file with `fs_file_close()` when no longer needed. The file will not be closed
automatically when the `fs` object is uninitialized.


Parameters
----------
pFS : (in, optional)
    A pointer to the file system object. Can be NULL to use the native file system. Note that when
    this is NULL, archives and mounts will not be supported.

pFilePath : (in)
    The path to the file to open. Must not be NULL.

openMode : (in)
    The mode to open the file with. A combination of the flags described above.

ppFile : (out)
    A pointer to a pointer which will receive the opened file object. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. Returns FS_DOES_NOT_EXIST if the
file does not exist when opening for reading. Returns FS_ALREADY_EXISTS if the file already exists
when opening with FS_EXCLUSIVE. Returns FS_IS_DIRECTORY if the path refers to a directory.


Example 1 - Basic Usage
-----------------------
The example below shows how to open a file for reading from the regular file system.

```c
fs_result result;
fs_file* pFile;

result = fs_file_open(pFS, "somefolder/somefile.txt", FS_READ, &pFile);
if (result != FS_SUCCESS) {
    // Handle error.
}

// Use the file...

// Close the file when no longer needed.
fs_file_close(pFile);
```

Example 2 - Opening from a Mount
--------------------------------
The example below shows how to mount a directory and then open a file from it. Error checking has
been omitted for clarity.

```c
// "assets" is the virtual path. FS_READ indicates this is a mount for use when opening a file in
// read-only mode (write mounts would use FS_WRITE).
fs_mount(pFS, "some_actual_path", "assets", FS_READ);

...

// The file path is prefixed with the virtual path "assets" so the file system will look inside the
// mounted directory first. Since the "assets" virtual path points to "some_actual_path", the file
// that will actually be opened is "some_actual_path/file.txt".
fs_file_open(pFS, "assets/file.txt", FS_READ, &pFile);
```

Example 3 - Opening from an Archive
-----------------------------------
The example below shows how to open a file from within a ZIP archive. This assumes the `fs` object
was initialized with support for ZIP archives (see fs_init() documentation for more information).

```c
// Opening the file directly from the archive by specifying the archive name in the path.
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ, &pFile);

// Same as above. The "archive.zip" part is not needed because transparent mode is used by default.
fs_file_open(pFS, "somefolder/somefile.txt", FS_READ, &pFile);

// Opening a file in verbose mode. The archive name must be specified in the path.
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ | FS_VERBOSE, &pFile); // This is a valid path in verbose mode.

// This will fail because the archive name is not specified in the path.
fs_file_open(pFS, "somefolder/somefile.txt", FS_READ | FS_VERBOSE, &pFile); // This will fail because verbose mode requires explicit archive names.

// This will fail because opaque mode treats archives as opaque.
fs_file_open(pFS, "somefolder/archive.zip/somefile.txt", FS_READ | FS_OPAQUE, &pFile);
```

Example 4 - Opening from a Mounted Archive
------------------------------------------
It is OK to use opaque mode when opening files from a mounted archive. This is the only way to open
files from an archive when using opaque mode.

```c
// Mount the archive to the virtual path "assets".
fs_mount(pFS, "somefolder/archive.zip", "assets", FS_READ);

// Now you can open files from the archive in opaque mode. Note how the path is prefixed with the
// virtual path "assets" which is how the mapping back to "somefolder/archive.zip" is made.
fs_file_open(pFS, "assets/somefile.txt", FS_READ | FS_OPAQUE, &pFile);
```


See Also
--------
fs_file_close()
fs_file_read()
fs_file_write()
fs_file_seek()
fs_file_tell()
fs_file_flush()
fs_file_truncate()
fs_file_get_info()
fs_file_duplicate()
*/
FS_API fs_result fs_file_open(fs* pFS, const char* pFilePath, int openMode, fs_file** ppFile);


/*
Closes a file.

You must close any opened files with this function when they are no longer needed. The owner `fs`
object will not close files automatically when it is uninitialized with `fs_uninit()`.


Parameters
----------
pFile : (in)
    A pointer to the file to close. Must not be NULL.


See Also
--------
fs_file_open()
*/
FS_API void fs_file_close(fs_file* pFile);


/*
Reads data from a file.

This function reads up to `bytesToRead` bytes from the file into the buffer pointed to by `pDst`.
The number of bytes actually read will be stored in the variable pointed to by `pBytesRead`.

If the end of the file is reached before any bytes are read, this function will return `FS_AT_END`
and `*pBytesRead` will be set to 0. `FS_AT_END` will only be returned if `*pBytesRead` is 0.


Parameters
----------
pFile : (in)
    A pointer to the file to read from. Must not be NULL.

pDst : (out)
    A pointer to the buffer that will receive the read data. Must not be NULL.

bytesToRead : (in)
    The maximum number of bytes to read from the file.

pBytesRead : (out, optional)
    A pointer to a variable that will receive the number of bytes actually read. Can be NULL if you
    do not care about this information. If NULL, the function will return an error if not all
    requested bytes could be read.


Return Value
------------
Returns `FS_SUCCESS` on success, `FS_AT_END` on end of file, or an error code otherwise. Will only
return `FS_AT_END` if `*pBytesRead` is 0.

If `pBytesRead` is NULL, the function will return an error if not all requested bytes could be
read. Otherwise, if `pBytesRead` is not NULL, the function will return `FS_SUCCESS` even if fewer
than `bytesToRead` bytes were read.


See Also
--------
fs_file_open()
fs_file_write()
fs_file_seek()
fs_file_tell()
*/
FS_API fs_result fs_file_read(fs_file* pFile, void* pDst, size_t bytesToRead, size_t* pBytesRead); /* Returns 0 on success, FS_AT_END on end of file, or an errno result code on error. Will only return FS_AT_END if *pBytesRead is 0. */


/*
Writes data to a file.

This function writes up to `bytesToWrite` bytes from the buffer pointed to by `pSrc` to the file.
The number of bytes actually written will be stored in the variable pointed to by `pBytesWritten`.


Parameters
----------
pFile : (in)
    A pointer to the file to write to. Must not be NULL.

pSrc : (in)
    A pointer to the buffer containing the data to write. Must not be NULL.

bytesToWrite : (in)
    The number of bytes to write to the file.

pBytesWritten : (out, optional)
    A pointer to a variable that will receive the number of bytes actually written. Can be NULL if
    you do not care about this information. If NULL, the function will return an error if not all
    requested bytes could be written.


Return Value
------------
Returns `FS_SUCCESS` on success, or an error code otherwise.

If `pBytesWritten` is NULL, the function will return an error if not all requested bytes could be
written. Otherwise, if `pBytesWritten` is not NULL, the function will return `FS_SUCCESS` even if
fewer than `bytesToWrite` bytes were written.


See Also
--------
fs_file_open()
fs_file_read()
fs_file_seek()
fs_file_tell()
fs_file_flush()
fs_file_truncate()
*/
FS_API fs_result fs_file_write(fs_file* pFile, const void* pSrc, size_t bytesToWrite, size_t* pBytesWritten);

/*
A helper for writing formatted data to a file.


Parameters
----------
pFile : (in)
    A pointer to the file to write to. Must not be NULL.

fmt : (in)
    A printf-style format string. Must not be NULL.

... : (in)
    Additional arguments as required by the format string.


Return Value
------------
Same as `fs_file_write()`.


See Also
--------
fs_file_write()
fs_file_writefv()
*/
FS_API fs_result fs_file_writef(fs_file* pFile, const char* fmt, ...) FS_ATTRIBUTE_FORMAT(2, 3);

/*
A helper for writing formatted data to a file.


Parameters
----------
pFile : (in)
    A pointer to the file to write to. Must not be NULL.

fmt : (in)
    A printf-style format string. Must not be NULL.

args : (in)
    Additional arguments as required by the format string.


Return Value
------------
Same as `fs_file_write()`.


See Also
--------
fs_file_write()
fs_file_writef()
*/
FS_API fs_result fs_file_writefv(fs_file* pFile, const char* fmt, va_list args);

/*
Moves the read/write cursor of a file.

You can seek relative to the start of the file, the current cursor position, or the end of the file.
A negative offset seeks backwards.

It is not an error to seek beyond the end of the file. If you seek beyond the end of the file and
then write, the exact behavior depends on the backend. On POSIX systems, it will most likely result
in a sparse file. In read mode, attempting to read beyond the end of the file will simply result
in zero bytes being read, and `FS_AT_END` being returned by `fs_file_read()`.

It is an error to try seeking to before the start of the file.


Parameters
----------
pFile : (in)
    A pointer to the file to seek. Must not be NULL.

offset : (in)
    The offset to seek to, relative to the position specified by `origin`. A negative value seeks
    backwards.

origin : (in)
    The origin from which to seek. One of the following values:
        FS_SEEK_SET
            Seek from the start of the file.

        FS_SEEK_CUR
            Seek from the current cursor position.

        FS_SEEK_END
            Seek from the end of the file.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.


See Also
--------
fs_file_tell()
fs_file_read()
fs_file_write()
*/
FS_API fs_result fs_file_seek(fs_file* pFile, fs_int64 offset, fs_seek_origin origin);

/*
Retrieves the current position of the read/write cursor in a file.


Parameters
----------
pFile : (in)
    A pointer to the file to query. Must not be NULL.

pCursor : (out)
    A pointer to a variable that will receive the current cursor position. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.


See Also
--------
fs_file_seek()
fs_file_read()
fs_file_write()
*/
FS_API fs_result fs_file_tell(fs_file* pFile, fs_int64* pCursor);


/*
Flushes any buffered data to disk.


Parameters
----------
pFile : (in)
    A pointer to the file to flush. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.
*/
FS_API fs_result fs_file_flush(fs_file* pFile);


/*
Truncates a file to the current cursor position.

It is possible for a backend to not support truncation, in which case this function will return
`FS_NOT_IMPLEMENTED`.


Parameters
----------
pFile : (in)
    A pointer to the file to truncate. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise. Will return `FS_NOT_IMPLEMENTED` if
the backend does not support truncation.
*/
FS_API fs_result fs_file_truncate(fs_file* pFile);


/*
Retrieves information about an opened file.


Parameters
----------
pFile : (in)
    A pointer to the file to query. Must not be NULL.

pInfo : (out)
    A pointer to a fs_file_info structure that will receive the file information. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.
*/
FS_API fs_result fs_file_get_info(fs_file* pFile, fs_file_info* pInfo);


/*
Duplicates a file handle.

This creates a new file handle that refers to the same underlying file as the original. The new
file handle will have its own independent cursor position. The initial position of the new file's
cursor will be undefined. You should call `fs_file_seek()` to set it to a known position before
using it.

Note that this does not duplicate the actual file on the file system itself. It just creates a
new `fs_file` object that refers to the same file.


Parameters
----------
pFile : (in)
    A pointer to the file to duplicate. Must not be NULL.

ppDuplicate : (out)
    A pointer to a pointer which will receive the duplicated file handle. Must not be NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.
*/
FS_API fs_result fs_file_duplicate(fs_file* pFile, fs_file** ppDuplicate);

/*
Retrieves the backend-specific data associated with a file.

You should never call this function unless you are implementing a custom backend. The size of the
data can be retrieved with `fs_file_get_backend_data_size()`.


Parameters
----------
pFile : (in)
    A pointer to the file to query. Must not be NULL.


Return Value
------------
Returns a pointer to the backend-specific data associated with the file, or NULL if there is no
such data.


See Also
--------
fs_file_get_backend_data_size()
*/
FS_API void* fs_file_get_backend_data(fs_file* pFile);

/*
Retrieves the size of the backend-specific data associated with a file.

You should never call this function unless you are implementing a custom backend. The data can be
accessed with `fs_file_get_backend_data()`.


Parameters
----------
pFile : (in)
    A pointer to the file to query. Must not be NULL.


Return Value
------------
Returns the size of the backend-specific data associated with the file, or 0 if there is no such
data.


See Also
--------
fs_file_get_backend_data()
*/
FS_API size_t fs_file_get_backend_data_size(fs_file* pFile);

/*
Files are streams. This function returns a pointer to the `fs_stream` interface of the file.


Parameters
----------
pFile : (in)
    A pointer to the file whose stream pointer is being retrieved. Must not be NULL.


Return Value
------------
Returns a pointer to the `fs_stream` interface of the file, or NULL if `pFile` is NULL.


See Also
--------
fs_file_get_fs()
*/
FS_API fs_stream* fs_file_get_stream(fs_file* pFile);

/*
Retrieves the file system that owns a file.


Parameters
----------
pFile : (in)
    A pointer to the file whose file system pointer is being retrieved. Must not be NULL.


Return Value
------------
Returns a pointer to the `fs` interface of the file's file system, or NULL if `pFile` is NULL.


See Also
--------
fs_file_get_stream()
*/
FS_API fs* fs_file_get_fs(fs_file* pFile);


/*
The same as `fs_first()`, but with the length of the directory path specified explicitly.


Parameters
----------
pFS : (in, optional)
    A pointer to the file system object. This can be NULL in which case the native file system will
    be used.

pDirectoryPath : (in)
    The path to the directory to iterate. Must not be NULL.

directoryPathLen : (in)
    The length of the directory path. Can be set to `FS_NULL_TERMINATED` if the path is
    null-terminated.

mode : (in)
    Options for the iterator. See `fs_file_open()` for a description of the available flags.


Return Value
------------
Same as `fs_first()`.
*/
FS_API fs_iterator* fs_first_ex(fs* pFS, const char* pDirectoryPath, size_t directoryPathLen, int mode);

/*
Creates an iterator for the first entry in a directory.

This function creates an iterator that can be used to iterate over the entries in a directory. This
will be the first function called when iterating over the files inside a directory.

To get the next entry in the directory, call `fs_next()`. When `fs_next()` returns NULL, there are
no more entries in the directory. If you want to end iteration early, use `fs_free_iterator()` to
free the iterator.

See `fs_file_open()` for a description of the available flags that can be used in the `mode`
parameter. When `FS_WRITE` is specified, it will look at write mounts. Otherwise, it will look at
read mounts.


Parameter
---------
pFS : (in, optional)
    A pointer to the file system object. This can be NULL in which case the native file system will
    be used.

pDirectoryPath : (in)
    The path to the directory to iterate. Must not be NULL.

mode : (in)
    Options for the iterator. See `fs_file_open()` for a description of the available flags.


Return Value
------------
Returns a pointer to an iterator object on success; NULL on failure or if the directory is empty.


Example
-------
The example below shows how to iterate over all entries in a directory. Error checking has been
omitted for clarity.

```c
fs_iterator* pIterator = fs_first(pFS, "somefolder", FS_READ);
while (pIterator != NULL) {
    // Use pIterator->name and pIterator->info here...
    printf("Found entry: %.*s\n", (int)pIterator->nameLength, pIterator->pName);
    pIterator = fs_next(pIterator);
}

fs_free_iterator(pIterator); // This is only needed if you want to terminate iteration early. If `fs_next()` returns NULL, you need not call this.
```

See Also
--------
fs_next()
fs_free_iterator()
fs_first_ex()
*/
FS_API fs_iterator* fs_first(fs* pFS, const char* pDirectoryPath, int mode);

/*
Gets the next entry in a directory iteration.

This function is used to get the next entry in a directory iteration. It should be called after
`fs_first()` or `fs_first_ex()` to retrieve the first entry, and then subsequently called to
retrieve each following entry.

If there are no more entries in the directory, this function will return NULL, and an explicit call
to `fs_free_iterator()` is not needed.


Parameters
----------
pIterator : (in)
    A pointer to the iterator object. Must not be NULL.


Return Value
------------
Returns a pointer to the next iterator object on success; NULL if there are no more entries. If
NULL is returned, you need not call `fs_free_iterator()`. If you want to terminate iteration early,
you must call `fs_free_iterator()` to free the iterator.

You cannot assume that the returned pointer is the same as the input pointer. It may need to be
reallocated internally to hold the data of the next entry.


See Also
--------
fs_first()
fs_first_ex()
fs_free_iterator()
*/
FS_API fs_iterator* fs_next(fs_iterator* pIterator);

/*
Frees an iterator object.

This function frees an iterator object that was created by `fs_first()` or `fs_first_ex()`. You
need not call this if `fs_next()` returned NULL from an earlier iteration. However, if you want to
terminate iteration early, you must call this function to free the iterator.

It is safe to call this function with a NULL pointer, in which case it will do nothing.


Parameters
----------
pIterator : (in, optional)
    A pointer to the iterator object. Can be NULL.


See Also
--------
fs_first()
fs_first_ex()
fs_next()
*/
FS_API void fs_free_iterator(fs_iterator* pIterator);


/*
The same as `fs_open_archive()`, but with the ability to explicitly specify the backend to use.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pBackend : (in)
    A pointer to the backend to use for opening the archive. Must not be NULL.

pBackendConfig : (in, optional)
    A pointer to backend-specific configuration data. Can be NULL if the backend does not require
    any configuration.

pArchivePath : (in)
    The path to the archive to open. Must not be NULL.

archivePathLen : (in)
    The length of the archive path. Can be set to `FS_NULL_TERMINATED` if the path is null-terminated.

openMode : (in)
    The mode to open the archive with.

ppArchive : (out)
    A pointer to a pointer which will receive the opened archive file system object. Must not be
    NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.


See Also
--------
fs_open_archive()
fs_close_archive()
*/
FS_API fs_result fs_open_archive_ex(fs* pFS, const fs_backend* pBackend, const void* pBackendConfig, const char* pArchivePath, size_t archivePathLen, int openMode, fs** ppArchive);

/*
Helper function for initializing a file system object for an archive, such as a ZIP file.

To uninitialize the archive, you must use `fs_close_archive()`. Do not use `fs_uninit()` to
uninitialize an archive. The reason for this is that archives opened in this way are garbage
collected, and there are reference counting implications.

Note that opening the archive in write mode (`FS_WRITE`) does not automatically mean you will be
able to write to it. None of the stock backends support writing to archives at this time.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pArchivePath : (in)
    The path to the archive to open. Must not be NULL.

openMode : (in)
    The open mode flags to open the archive with. See `fs_file_open()` for a description of the
    available flags.

ppArchive : (out)
    A pointer to a pointer which will receive the opened archive file system object. Must not be
    NULL.


Return Value
------------
Returns FS_SUCCESS on success; any other result code otherwise.


See Also
--------
fs_close_archive()
fs_open_archive_ex()
*/
FS_API fs_result fs_open_archive(fs* pFS, const char* pArchivePath, int openMode, fs** ppArchive);


/*
Closes an archive that was previously opened with `fs_open_archive()`.

You must use this function to close an archive opened with `fs_open_archive()`. Do not use
`fs_uninit()` to uninitialize an archive.

Note that when an archive is closed, it does not necessarily mean that the underlying file is
closed immediately. This is because archives are reference counted and garbage collected. You can
force garbage collection of unused archives with `fs_gc_archives()`.


Parameters
----------
pArchive : (in)
    A pointer to the archive file system object to close. Must not be NULL.


See Also
--------
fs_open_archive()
fs_open_archive_ex()
*/
FS_API void fs_close_archive(fs* pArchive);


/*
Garbage collects unused archives.

This function will close any opened archives that are no longer in use depending on the specified
policy.

You should rarely need to call this function directly. Archives will automatically be garbage collected
when the `fs` object is uninitialized with `fs_uninit()`.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

policy : (in)
    The garbage collection policy to use. Set this to FS_GC_POLICY_THRESHOLD to only collect archives
    if the number of opened archives exceeds the threshold set with `fs_set_archive_gc_threshold()`
    which defaults to 10. Set this to FS_GC_POLICY_ALL to collect all unused archives regardless of the
    threshold.


See Also
--------
fs_open_archive()
fs_close_archive()
fs_set_archive_gc_threshold()
*/
FS_API void fs_gc_archives(fs* pFS, int policy);

/*
Sets the threshold for garbage collecting unused archives.

When an archive is no longer in use (its reference count drops to zero), it will not be closed
immediately. Instead, it will be kept open in case it is needed again soon. The threshold is what
determines how many unused archives will be kept open before they are garbage collected. The
default threshold is 10.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

threshold : (in)
    The threshold for garbage collecting unused archives.


See Also
--------
fs_gc_archives()
*/
FS_API void fs_set_archive_gc_threshold(fs* pFS, size_t threshold);

/*
Retrieves the threshold for garbage collecting unused archives.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.


Return Value
------------
Returns the threshold for garbage collecting unused archives.
*/
FS_API size_t fs_get_archive_gc_threshold(fs* pFS);

/*
A helper function for checking if a path looks like it could be an archive.

This only checks the path string itself. It does not actually attempt to open and validate the
archive itself.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pPath : (in)
    The path to check. Must not be NULL.

pathLen : (in)
    The length of the path string. Can be set to `FS_NULL_TERMINATED` if the path is null-terminated.

Return Value
------------
Returns FS_TRUE if the path looks like an archive, FS_FALSE otherwise.
*/
FS_API fs_bool32 fs_path_looks_like_archive(fs* pFS, const char* pPath, size_t pathLen);    /* Does not validate that it's an actual valid archive. */


/*
Mounts a real directory or archive to a virtual path.

You must specify the actual path to the directory or archive on the file system referred to by
`pFS`. The virtual path can be NULL, in which case it will be treated as an empty string.

The virtual path is the path prefix that will be used when opening files. For example, if you mount
the actual path "somefolder" to the virtual path "assets", then when you open a file with the path
"assets/somefile.txt", it will actually open "somefolder/somefile.txt".

There are two groups of mounts - read-only and write. Read-only mounts are used when opening a file
in read-only mode (i.e. without the `FS_WRITE` flag). Write mounts are used when opening a file in
write mode (i.e. with the `FS_WRITE` flag). To control this, set the appropriate flag in the
`options` parameter.

The following flags are supported in the `options` parameter:

    FS_READ
        This is a read-only mount. It will be used when opening files without the `FS_WRITE` flag.

    FS_WRITE
        This is a write mount. It will be used when opening files with the `FS_WRITE

    FS_LOWEST_PRIORITY
        By default, mounts are searched in the reverse order that they were added. This means that
        the most recently added mount has the highest priority. When this flag is specified, the
        mount will have the lowest priority instead.

For a read-only mount, you can have multiple mounts with the same virtual path in which case they
will be searched in order or priority when opening a file.

For write mounts, you can have multiple mounts with the same virtual path, but when opening a file
for writing, only the first matching mount will be used. You can have multiple write mounts where
the virtual path is a sub-path of another write mount. For example, you could have one write
mount with the virtual path "assets" and another with the virtual path "assets/images". When
opening a file for writing, if the path starts with "assets/images", that mount will be used
because it is a more specific match. Otherwise, if the path starts with "assets" but not
"assets/images", the other mount will be used.

You can specify both `FS_READ` and `FS_WRITE` in the `options` parameter to create one read-only
mount, and one write mount in a single call. This is equivalent to calling `fs_mount()` twice -
once with `FS_READ`, and again with `FS_WRITE`.

Unmounting a directory or archive is done with `fs_unmount()`. You must specify the actual path
when unmounting.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pActualPath : (in)
    The actual path to the directory or archive to mount. Must not be NULL.

pVirtualPath : (in, optional)
    The virtual path to mount the directory or archive to. Can be NULL in which case it will be
    treated as an empty string.

options : (in)
    Options for the mount. A combination of the flags described above.


Return Value
------------
Returns `FS_SUCCESS` on success; any other result code otherwise. If an identical mount already exists,
`FS_SUCCESS` will be returned.


Example 1 - Basic Usage
-----------------------
```c
// Mount two directories to the same virtual path.
fs_mount(pFS, "some/actual/path", "assets", FS_READ);   // Lowest priority.
fs_mount(pFS, "some/other/path",  "assets", FS_READ);

// Mount a directory for writing.
fs_mount(pFS, "some/write/path",      "assets",        FS_WRITE);
fs_mount(pFS, "some/more/write/path", "assets/images", FS_WRITE); // More specific write mount.

// Mount a read-only mount, and a write mount in a single call.
fs_mount(pFS, "some/actual/path", "assets", FS_READ | FS_WRITE);
```


Example 2 - Mounting an Archive
-------------------------------
```c
// Mount a ZIP archive to the virtual path "assets".
fs_mount(pFS, "some/actual/path/archive.zip", "assets", FS_READ);
```


See Also
--------
fs_unmount()
fs_mount_sysdir()
fs_mount_fs()
*/
FS_API fs_result fs_mount(fs* pFS, const char* pActualPath, const char* pVirtualPath, int options);

/*
Unmounts a directory or archive that was previously mounted with `fs_mount()`.

You must specify the actual path to the directory or archive that was used when mounting. The
virtual path is not needed.

The only options that matter here are `FS_READ` and `FS_WRITE`. If you want to unmount a read-only
mount, you must specify `FS_READ`. If you want to unmount a write mount, you must specify
`FS_WRITE`. If you want to unmount both a read-only mount, and a write mount in a single call, you
can specify both flags. Using both flags is the same as calling `fs_unmount()` twice - once for the
read-only mount, and once for the write mount.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pActualPath : (in)
    The actual path to the directory or archive to unmount. Must not be NULL.

options : (in)
    Either `FS_READ`, `FS_WRITE`, or both to unmount the corresponding mounts.


Return Value
------------
Returns `FS_SUCCESS` on success; any other result code otherwise. If no matching mount could be
found, `FS_SUCCESS` will be returned (it will just be a no-op).
*/
FS_API fs_result fs_unmount(fs* pFS, const char* pActualPath, int options);

/*
A helper function for mounting a standard system directory to a virtual path.

When calling this function you specify the type of system directory you want to mount. The actual
path of the system directory will often be generic, like "/home/yourname/" which is not useful for
a real program. For this reason, this function forces you to specify a sub-directory that will be
used with the system directory. This would often be something like the name of your application,
such as "myapp". It can also include sub-directories, such as "mycompany/myapp".

Otherwise, this function behaves exactly like `fs_mount()`.

Unmount the directory with `fs_unmount_sysdir()`. You must specify the same type and sub-directory
that was used when mounting.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

type : (in)
    The type of system directory to mount.

pSubDir : (in)
    The sub-directory to use with the system directory. Must not be NULL nor an empty string.

pVirtualPath : (in, optional)
    The virtual path to mount the system directory to. Can be NULL in which case it will be treated
    as an empty string.

options : (in)
    Options for the mount. A combination of the flags described in `fs_mount()`.


Return Value
------------
Returns `FS_SUCCESS` on success; any other result code otherwise. If an identical mount already
exists, `FS_SUCCESS` will be returned.


See Also
--------
fs_mount()
fs_unmount_sysdir()
*/
FS_API fs_result fs_mount_sysdir(fs* pFS, fs_sysdir_type type, const char* pSubDir, const char* pVirtualPath, int options);

/*
Unmounts a system directory that was previously mounted with `fs_mount_sysdir()`.

This is the same as `fs_unmount()`, but follows the "type" and sub-directory semantics of
`fs_mount_sysdir()`.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

type : (in)
    The type of system directory to unmount.

pSubDir : (in)
    The sub-directory that was used with the system directory when mounting. Must not be NULL nor
    an empty string.

options : (in)
    Either `FS_READ`, `FS_WRITE`, or both to unmount the corresponding mounts.


Return Value
------------
Returns `FS_SUCCESS` on success; any other result code otherwise. If no matching mount could be
found, `FS_SUCCESS` will be returned (it will just be a no-op).
*/
FS_API fs_result fs_unmount_sysdir(fs* pFS, fs_sysdir_type type, const char* pSubDir, int options);

/*
Mounts another `fs` object to a virtual path.

This is the same as `fs_mount()`, but instead of specifying an actual path to a directory or
archive, you specify another `fs` object.

Use `fs_unmount_fs()` to unmount the file system.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pOtherFS : (in)
    A pointer to the other file system object to mount. Must not be NULL.

pVirtualPath : (in, optional)
    The virtual path to mount the other file system to. Can be NULL in which case it will be treated
    as an empty string.

options : (in)
    Options for the mount. A combination of the flags described in `fs_mount()`.


Return Value
------------
Returns `FS_SUCCESS` on success; any other result code otherwise. If an identical mount already
exists, `FS_SUCCESS` will be returned.


See Also
--------
fs_mount()
fs_unmount_fs()
*/
FS_API fs_result fs_mount_fs(fs* pFS, fs* pOtherFS, const char* pVirtualPath, int options);

/*
Unmounts a file system that was previously mounted with `fs_mount_fs()`.


Parameters
----------
pFS : (in)
    A pointer to the file system object. Must not be NULL.

pOtherFS : (in)
    A pointer to the other file system object to unmount. Must not be NULL.

options : (in)
    Options for the unmount. A combination of the flags described in `fs_unmount()`.


Return Value
------------
Returns `FS_SUCCESS` on success; any other result code otherwise. If no matching mount could be
found, `FS_SUCCESS` will be returned (it will just be a no-op).


See Also
--------
fs_unmount()
fs_mount_fs()
*/
FS_API fs_result fs_unmount_fs(fs* pFS, fs* pOtherFS, int options);


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
FS_API fs_result fs_file_open_and_write(fs* pFS, const char* pFilePath, const void* pData, size_t dataSize);


/* BEG fs_backend_posix.h */
extern const fs_backend* FS_BACKEND_POSIX;
/* END fs_backend_posix.h */

/* BEG fs_backend_win32.h */
extern const fs_backend* FS_BACKEND_WIN32;
/* END fs_backend_win32.h */
/* END fs.h */


/* BEG fs_errno.h */
FS_API fs_result fs_result_from_errno(int error);
/* END fs_errno.h */


/* BEG fs_path.h */
/*
These functions are low-level functions for working with paths. The most important part of this API
is probably the iteration functions. These functions are used for iterating over each of the
segments of a path. This library will recognize both '\' and '/'. If you want to use a different
separator, you'll need to use a different library. Likewise, this library will treat paths as case
sensitive. Again, you'll need to use a different library if this is not suitable for you.

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
FS_API int fs_path_compare(const char* pPathA, size_t pathALen, const char* pPathB, size_t pathBLen);
FS_API const char* fs_path_file_name(const char* pPath, size_t pathLen);    /* Does *not* include the null terminator. Returns an offset of pPath. Will only be null terminated if pPath is. Returns null if the path ends with a slash. */
FS_API int fs_path_directory(char* pDst, size_t dstCap, const char* pPath, size_t pathLen); /* Returns the length, or < 0 on error. pDst can be null in which case the required length will be returned. Will not include a trailing slash. */
FS_API const char* fs_path_extension(const char* pPath, size_t pathLen);    /* Does *not* include the null terminator. Returns an offset of pPath. Will only be null terminated if pPath is. Returns null if the extension cannot be found. */
FS_API fs_bool32 fs_path_extension_equal(const char* pPath, size_t pathLen, const char* pExtension, size_t extensionLen); /* Returns true if the extension is equal to the given extension. Case insensitive. */
FS_API const char* fs_path_trim_base(const char* pPath, size_t pathLen, const char* pBasePath, size_t basePathLen);
FS_API fs_bool32 fs_path_begins_with(const char* pPath, size_t pathLen, const char* pBasePath, size_t basePathLen);
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

    fs_memory_stream_seek(pStream, 0, FS_SEEK_END);

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

FS_API int fs_strncmp(const char* str1, const char* str2, size_t maxLen);
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

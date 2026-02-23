# MaestroCore

**MaestroCore** is a modular C library providing reusable core
functionality for networking, TLS, HTTP, utilities, and system-level
helpers. It is designed as a **static library** with a clean include
structure and minimal external dependencies.

------------------------------------------------------------------------

## Features

-   Modular architecture (`modules` + `utils`)
-   Static library build (`libmaestrocore.a`)
-   TLS support via **mbedTLS** (AnonyMaestro)
-   Optional JSON support via **cJSON**
-   HTTP client & parser
-   TCP / TLS transport abstraction
-   Logging, file utilities, time utilities, error handling

------------------------------------------------------------------------

## Build Requirements

-   C compiler with C11 support (`gcc`, `clang`, or `cc`)
-   `make`
-   `cmake` (for mbedTLS / AnonyMaestro)
-   POSIX-compatible system

------------------------------------------------------------------------

## Building the Library

### Default build

    make

Produces:

    build/lib/libmaestrocore.a

------------------------------------------------------------------------

### Enable JSON support

    make JSON=1

------------------------------------------------------------------------

# Using MaestroCore in Other Projects

## Option 1 -- Git Submodule (Recommended)

Add MaestroCore as a submodule:

    git submodule add https://your.repo.url/MaestroCore.git external/MaestroCore
    git submodule update --init --recursive

Inside your main project, build it:

    cd external/MaestroCore
    make

Then link it from your project build system.

### Example (Makefile-based project)

    -Iexternal/MaestroCore/modules/include
    -Iexternal/MaestroCore/utils/include
    -Lexternal/MaestroCore/build/lib
    -lmaestrocore
    -lpthread -lm

If TLS is used, also link the mbedTLS static libraries:

    external/MaestroCore/build/anonymaestro/library/libmbedtls.a
    external/MaestroCore/build/anonymaestro/library/libmbedx509.a
    external/MaestroCore/build/anonymaestro/library/libmbedcrypto.a

------------------------------------------------------------------------

## Option 2 -- Add as a CMake Subdirectory

If your main project uses CMake:

    add_subdirectory(external/MaestroCore)

You can then reference the static library in your target linking
configuration.

------------------------------------------------------------------------

## Option 3 -- Prebuilt Static Library

1.  Build MaestroCore separately:

```{=html}
<!-- -->
```
    make

2.  Copy:

```{=html}
<!-- -->
```
    build/lib/libmaestrocore.a
    modules/include/
    utils/include/

3.  Add include paths and link flags in your project.

------------------------------------------------------------------------

## Include Headers

Umbrella headers allow clean includes:

``` c
#include <maestromodules/http_client.h>
#include <maestroutils/file_logging.h>
```

Include paths required:

    -Ipath/to/MaestroCore/modules/include
    -Ipath/to/MaestroCore/utils/include

------------------------------------------------------------------------

## Manual Test

Build:

    make make-test

Binary:

    build/http_manual_test

------------------------------------------------------------------------

## Cleaning

    make clean

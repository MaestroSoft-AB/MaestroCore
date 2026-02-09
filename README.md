# MaestroCore

MaestroCore is a lightweight, modular C library that provides reusable networking modules and general-purpose utilities.  
It is designed to be easily vendored into other projects or added as a Git submodule.

The library is split into two main parts:

- **MaestroModules** – networking and core functionality  
  (TCP client, HTTP client, scheduler, linked lists, etc.)
- **MaestroUtils** – reusable utilities  
  (file helpers, JSON helpers, time helpers, error handling, HTTP status codes, etc.)

You can build and use them independently or as one combined library.

---

## Directory Layout

```
MaestroCore/
├── modules/
│   ├── include/
│   │   ├── maestromodules.h
│   │   └── maestromodules/
│   └── src/
├── utils/
│   ├── include/
│   │   ├── maestroutils.h
│   │   └── maestroutils/
│   └── src/
├── build/
├── Makefile
└── compile_flags.txt
```

Public headers are exposed through:

```c
#include <maestromodules.h>
#include <maestroutils.h>
```

or via their namespaced forms:

```c
#include <maestromodules/tcp_client.h>
#include <maestroutils/file_utils.h>
```

---

## Building

The project uses a plain Makefile and produces static libraries.

### Build everything (combined library)

```bash
make
# or
make core
```

Produces:
```
build/lib/libmaestrocore.a
```

### Build only modules

```bash
make modules
```

Produces:
```
build/lib/libmaestromodules.a
```

### Build only utils

```bash
make utils
```

Produces:
```
build/lib/libmaestroutils.a
```

### Clean

```bash
make clean
```

---

## Optional JSON Support (cJSON)

JSON support in MaestroUtils is optional and depends on **cJSON**.

To enable JSON:

```bash
make JSON=1
```

This:

- Builds `json_utils.c`
- Builds cJSON
- Defines:

```c
#define MAESTROUTILS_WITH_CJSON
```

in all compilation units.

Without `JSON=1`, JSON helpers are not built and not available.

In code:

```c
#ifdef MAESTROUTILS_WITH_CJSON
#include <maestroutils/json_utils.h>
#endif
```

---

## Using MaestroCore in Another Project

You can vendor the repository or add it as a submodule:

```bash
git submodule add https://github.com/MaestroSoft-AB/MaestroCore.git external/MaestroCore
```

Then build it once:

```bash
cd external/MaestroCore
make
```

Link against it from your project.

Example:

```
YourProject/
├── src/
├── external/
│   └── MaestroCore/
│       └── build/lib/libmaestrocore.a
```

Compiler flags:

```make
CFLAGS += -Iexternal/MaestroCore/modules/include
CFLAGS += -Iexternal/MaestroCore/utils/include
```

Linker flags:

```make
LDFLAGS += external/MaestroCore/build/lib/libmaestrocore.a
```

Or if you only want modules:

```make
LDFLAGS += external/MaestroCore/build/lib/libmaestromodules.a
```

Or utils:

```make
LDFLAGS += external/MaestroCore/build/lib/libmaestroutils.a
```

---

## Example Makefile Snippet

```make
MAESTRO_DIR := external/MaestroCore

CFLAGS += -I$(MAESTRO_DIR)/modules/include
CFLAGS += -I$(MAESTRO_DIR)/utils/include

# Build MaestroCore first
maestro:
	$(MAKE) -C $(MAESTRO_DIR)

# Link your app with MaestroCore
app: $(OBJS) maestro
	$(CC) $(OBJS) $(MAESTRO_DIR)/build/lib/libmaestrocore.a -o app
```

With JSON support:

```make
maestro:
	$(MAKE) -C $(MAESTRO_DIR) JSON=1
```

---

## Installation (Optional)

You can install headers and libraries system-wide:

```bash
sudo make install PREFIX=/usr/local
```

Headers:
```
/usr/local/include/maestromodules.h
/usr/local/include/maestroutils.h
/usr/local/include/maestromodules/
/usr/local/include/maestroutils/
```

Libraries:
```
/usr/local/lib/libmaestrocore.a
/usr/local/lib/libmaestromodules.a
/usr/local/lib/libmaestroutils.a
```

Then use normally:

```c
#include <maestromodules.h>
#include <maestroutils.h>
```

and compile:

```bash
cc main.c -lmaestrocore
```


---

## Testing

MaestroCore uses **Unity** and **CMock** for unit testing. This allows for testing complex network state machines by mocking the TCP layer.

### Prerequisites
To run the tests, you need:
- **GCC/Clang** (C11 support)
- **Make**
- **Ruby** (Required by CMock to generate mock objects from headers)

On Ubuntu/Debian:
```bash
sudo apt install build-essential ruby
```

### Running tests

The test suite automatically generates mocks for internal dependencies before compiling the test runner.

```make test```



// HOW TO BUILD \\
FULL PROJECT:
cmake -DBUILD_MODULES=ON -DBUILD_UTILS=ON -DBUILD_TESTS=ON ..

ONLY MODULES:
cmake -DBUILD_MODULES=ON ..

ONLY UTILS:
cmake -DBUILD_UTILS=ON ..


MODULES WITHOUT TESTS:
cmake -DBUILD_MODULES=ON -DBUILD_TESTS=OFF ..

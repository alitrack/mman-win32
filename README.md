mman-win32
==========

mman library for Windows. mirror of https://code.google.com/p/mman-win32/

A light implementation of the mmap functions for MinGW.

The mmap-win32 library implements a wrapper for mmap functions around the memory mapping Windows API.

License: MIT License

Build
-----

Requirements: a C compiler (MinGW-w64 or MSVC) and CMake >= 3.10.

### CMake (recommended)

    mkdir build && cd build
    cmake .. -DBUILD_TESTS=ON
    cmake --build .

    # cross-compile from Linux (MinGW-w64):
    #   cmake .. -DCMAKE_SYSTEM_NAME=Windows \
    #            -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    #            -DBUILD_TESTS=ON

This produces `libmman` (shared by default; pass
`-DBUILD_SHARED_LIBS=OFF` for a static library) and, with
`-DBUILD_TESTS=ON`, the `t_mman` test executable.

Run the tests (Windows, or under Wine):

    t_mman.exe

### MinGW Makefile

    ./configure --enable-static [--prefix=/usr/local]
    make
    make test     # builds and runs test.exe
    make install

### MSVC

Open `mman.sln` in Visual Studio.

Notes
-----

* `madvise(MADV_WILLNEED)` uses `PrefetchVirtualMemory`; `madvise(MADV_DONTNEED)`
  uses `OfferVirtualMemory`, which only exists on Windows 11 21H2+. Both are
  resolved at runtime and degrade to a no-op when unavailable.
* `MAP_FIXED` uses `MapViewOfFileEx`.

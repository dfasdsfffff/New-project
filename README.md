# CppWordKit

CppWordKit is a lightweight C++20 library for reading and editing `.docx`
files without Microsoft Office automation.

The first version focuses on:

- Opening and saving `.docx` packages.
- Reading text from `word/document.xml`.
- Replacing text inside simple `w:t` runs.
- Filling existing tables row by row.

## Dependencies

- C++20 compiler
- CMake 3.20+
- zlib
- libxml2

## Build

With vcpkg:

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build
ctest --test-dir build
```

With the local MinGW dependency folders used by this workspace:

```sh
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCPPWORDKIT_USE_LOCAL_DEPS=ON
cmake --build build-mingw
ctest --test-dir build-mingw
```

The local defaults are:

- `D:/libraries/MinGW/libxml2-v2.12.10`
- `D:/libraries/MinGW/zlib`

You can override them with `CPPWORDKIT_LOCAL_LIBXML2_ROOT` and
`CPPWORDKIT_LOCAL_ZLIB_ROOT`.

Generic system dependency discovery:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Example

```cpp
#include <cppwordkit/CppWordKit.hpp>

int main() {
    auto doc = cppwordkit::WordDocument::open("input.docx");
    doc.replaceText("{{name}}", "CppWordKit");
    doc.saveAs("output.docx");
}
```

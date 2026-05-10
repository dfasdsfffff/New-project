# CppWordKit

CppWordKit is a lightweight C++20 toolkit for `.docx` files without
Microsoft Office automation.

The project is split into two layers:

- **CppWordKitCore** (`CppWordKit::Core`) handles OOXML/OPC document work:
  opening and saving `.docx`, editing XML parts, replacing text, inserting
  table rows, and adding images.
- **CppWordTpl** (`CppWordKit::Tpl`) builds on Core and provides a docxtpl-style
  template renderer for existing `.docx` templates and dynamic data.

`CppWordKit::CppWordKit` remains as a compatibility aggregate target that links
both layers.

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

Core document processing:

```cpp
#include <cppwordkit/core/CppWordKitCore.hpp>

int main() {
    auto doc = cppwordkit::WordDocument::open("input.docx");
    doc.replaceText("{{name}}", "CppWordKit");
    doc.saveAs("output.docx");
}
```

Template rendering:

```cpp
#include <cppwordkit/tpl/CppWordTpl.hpp>

int main() {
    cppwordkit::TemplateContext context = {
        {"user", cppwordkit::TemplateValue::Object{{"name", "Alice"}}}
    };

    auto tpl = cppwordkit::DocxTemplate::open("template.docx");
    tpl.render(context);
    tpl.saveAs("output.docx");
}
```

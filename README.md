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

Editor-oriented document model:

```cpp
#include <cppwordkit/core/CppWordKitCore.hpp>

int main() {
    auto doc = cppwordkit::WordDocument::open("input.docx");

    auto model = doc.model();
    cppwordkit::Paragraph paragraph;
    paragraph.runs.push_back({.text = "Added from the public model API"});
    model.paragraphs.push_back(paragraph);

    doc.replaceBody(model);
    doc.saveAs("output.docx");
}
```

The model API is the recommended surface for editors and other applications
that need structured access to paragraphs, runs, basic styles, simple tables,
and inline images. The lower-level `XmlPart` XPath APIs remain available for
advanced OOXML work, but UI applications should avoid depending on raw XPath
or hand-written WordprocessingML when the model API covers the task.

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

`CppWordTpl` supports a docxtpl-style template subset implemented in native C++:

- variables with dot paths, such as `{{ user.name }}`
- filters including `upper`, `lower`, `default`, `escape`, `length`, `join`,
  `capitalize`, `title`, `trim`, and `replace`
- control flow with `{% if %}`, `{% elif %}`, `{% else %}`, `{% endif %}`
- loops with `{% for item in items %}` and `{% endfor %}`, including
  `loop.index`, `loop.index0`, `loop.first`, and `loop.last`
- Word structure tags for common templates, especially `{%p ... %}` paragraph
  blocks and `{%tr ... %}` table row blocks

RichText, InlineImage template values, sub-documents, and media replacement are
reserved for a later CppWordTpl phase.

## Test notes on Windows

When building with the local MinGW dependency folders, tests need the libxml2,
zlib, and compiler runtime DLL folders on `PATH`. The CMake test targets add
those folders automatically when `CPPWORDKIT_USE_LOCAL_DEPS=ON`; if tests are
run manually, launch them from an environment that includes:

- `D:/libraries/MinGW/libxml2-v2.12.10/bin`
- `D:/libraries/MinGW/zlib/bin`
- the active MinGW compiler `bin` directory

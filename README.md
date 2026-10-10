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

### Windows / MSVC dependencies

MSVC builds require MSVC-compatible x64 libraries; do not reuse MinGW `.dll.a`
libraries. To build zlib 1.3.1 and libxml2 2.12.10 from existing source trees:

```powershell
./cmake/build-msvc-deps.ps1 -ZlibSource "D:/sources/zlib-1.3.1" -LibXml2Source "D:/sources/libxml2-v2.12.10"
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCPPWORDKIT_USE_LOCAL_DEPS=ON -DCPPWORDKIT_LOCAL_ZLIB_ROOT="$PWD/build-msvc-deps/install" -DCPPWORDKIT_LOCAL_LIBXML2_ROOT="$PWD/build-msvc-deps/install"
cmake --build build --config Debug --parallel 4
ctest --test-dir build -C Debug --output-on-failure
```

Replace the source paths with your local folders. The helper installs Release
DLLs and import libraries into `build-msvc-deps/install`; the shared C API can
also be used by the Debug application. VS Code workspace settings use this
installation. Use a separate build directory when switching to MinGW.
To enable the editor, also set `CPPWORDKIT_BUILD_WORD_EDITOR=ON` and point
`CMAKE_PREFIX_PATH` at your Qt6 **msvc2022_64** installation.

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

    cppwordkit::Paragraph paragraph;
    paragraph.runs.push_back({.text = "Added from the public model API"});
    doc.appendParagraph(paragraph);
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

## Current scope and limitations

The project is a basic DOCX toolkit and editor, not a complete Word replacement.
Core supports paragraphs, basic run/paragraph formatting, simple text tables,
inline images and page settings. The editor maps these features to Qt; advanced
OOXML structures may lose information when the body is rebuilt during editing.
The planned independent `editorcore` command layer is not implemented yet.

### 核心库功能检查（2026-10-10）

结论：已具备基础工具库能力，但尚不足以支持任意 Word 文档的无损编辑。
本轮优先修复局部编辑的数据保留和模型索引一致性，没有引入新依赖。

| 模块 | 当前能力 | 尚存缺口 |
| --- | --- | --- |
| OPC / ZIP | ZIP32 读写、XML/二进制部件、暂存后提交保存 | 主文档及关系路径硬编码；读取缺少 CRC 校验和完整的目录边界校验；不支持 ZIP64/加密 |
| 文档模型 | 顶层段落、Run、基础样式、顶层表格顺序、内联图片、页面设置 | 未完整建模超链接、域、修订、内容控件、编号、多节、合并/嵌套表格；制表符和换行不能完整往返 |
| 编辑 | 段落增删改、单段位置化 insertText/deleteRange/applyStyle、样式、文本替换、表格填充、图片插入 | 跨段落位置编辑、替换边界、全词跨 Run 匹配仍需完善 |
| 模板 | 变量、过滤器、条件、循环、段落/表格行标签 | 是 docxtpl 子集；不支持 RichText、模板图片值、子文档；渲染失败不保证整包回滚 |
| 保存 | 覆盖控制、失败保留目标、未知部件保留 | SaveOptions 的 prettyPrintXml 和 preserveUnknownParts 暂未生效 |

本轮已修复：

- `setParagraph/insertParagraph/appendParagraph/removeParagraph` 直接修改目标 XML
  节点，保留其他段落、复杂表格、内容控件及节属性。`setParagraph` 的目标段落
  仍按输入模型重建，因此目标段落内未建模的内容需要调用方自行处理。
- WordDocument 段落样式与 Run 样式 API 均使用正文直属段落索引，避免将样式
  误读/误写到前面的表格单元格；底层 XmlPart 仍使用全部段落索引。
- 模型只将顶层表格映射到 bodyBlocks，避免嵌套表格挤占后续表格的位置。
- `rawPart()` 对 XML 返回当前内存状态的序列化，二进制部件仍返回原始字节。

`model()` 同时返回 bodyBlocks 和 paragraphs/tables 快照。bodyBlocks 非空时，
`replaceBody()` 仅使用 bodyBlocks；修改快照不会同步它。局部编辑应使用段落 API。
`replaceBody()` 和目前的 `insertImage()` 仍会重建正文，不能保证高级结构保真。
建议后续依次完善 ZIP/OPC 校验、跨段落位置编辑与文本边界、复杂模型和模板事务。

新增 `cppwordkit_core_integrity_tests` 验证混合正文样式索引、嵌套表格映射、局部
编辑保真、失败编辑不变、rawPart 一致性及保存重开；检查在 Release 下也生效。

### 通用编辑 API：单段位置编辑

Core 提供无 Qt 依赖的 `paragraphText/insertText/deleteRange/applyStyle`。
实现直接操作 XML 节点，保留段落属性、书签、超链接、Run 的未识别样式以及其他
段落和包部件，不依赖有损的 `DocumentModel` 重建。修改先在段落副本中完成，
全部成功后提交；非法范围、UTF-8、样式或不支持的内容不会留下部分修改。

```cpp
#include <cppwordkit/core/CppWordKitCore.hpp>

auto doc = cppwordkit::WordDocument::create();
doc.insertText(0, 0, "你好😀\tWorld\n第二行");
doc.deleteRange(0, 2, 1); // 删除 emoji：offset/length 按 Unicode 标量计数
cppwordkit::TextStyle style;
style.bold = true;
style.colorHex = "336699";
doc.applyStyle(0, 0, 2, style); // 只格式化“你好”，其余样式保持原样
auto text = doc.paragraphText(0);
doc.saveAs("edited.docx");
```

位置规则和支持边界：

- WordDocument 的段落索引仍只包含正文直属段落。范围是单段内的
  `[offset, offset + length)`，可跨多个 Run 及超链接；目前不能跨段落。
  底层 XmlPart 的同名 API 使用该部件内全部 `w:p` 的索引，可处理表格单元格等段落。
- 输入是 UTF-8，偏移是 Unicode 标量数量，不是字节、UTF-16 单元或用户感知的
  字素簇。组合字符和 emoji 序列可能占多个位置；Qt 调用者需要转换 UTF-16 偏移。
- `paragraphText()` 返回与编辑位置一致的文本：制表符为 `\t`，软换行为 `\n`，
  图片/对象为 U+FFFC 并占一个位置。旧的 `text()` / `model()` 文本提取语义未改变，
  不应使用它们计算新 API 的位置。
- 插入时把 `\t` 写为 `w:tab`，LF/CRLF/CR 写为 `w:br`。换行是段内软换行，
  不会创建新段落；插入 Run 继承所在 Run 的属性，边界优先继承左侧（段首为右侧），
  包括超链接归属。空段落也可插入。
- `applyStyle()` 是补丁：未设置的属性保持原值，`false` 可关闭布尔样式；只有
  命中的 Run 需要拆分。位置引用和 Run 索引不保证在编辑后稳定，应重新查询。
- 文本范围不允许删除或格式化嵌入对象。含域、修订、内容控件、分页/分栏符及
  其他未支持内容的目标段落会抛 `WordProcessingException`，不会静默跳过结构。
- 这组 API 提供单次操作的异常回滚，不是编辑器的撤销/重做或多操作事务。

`cppwordkit_text_editing_tests` 覆盖中文/emoji、实体字符、制表符/换行、跨 Run 和
超链接、局部样式、非法输入回滚、对象保留、保存重开，以及固定种子的随机编辑
对照测试。`cppwordkit_edit_text` 示例只链接 `CppWordKit::Core`。

Saving writes a complete staging file beside the destination before replacing
it. Write/close/commit errors are reported, existing files survive failed
replacement, and `overwrite=false` is enforced at commit time. This needs space
for both files, does not guarantee power-loss durability, and may not preserve
all filesystem metadata. Abnormal process termination may leave a
`.cppwordkit-save-*` staging directory. Normal failures clean it up.
Windows Unicode document paths are supported by the Qt controller. Missing or
failed image resources stop editor saving instead of silently dropping images.

`cppwordkit_save_tests` covers file replacement, no-overwrite, failed destinations,
Unicode paths and staging cleanup (plus locked targets on Windows).
Enable `CPPWORDKIT_BUILD_WORD_EDITOR` and `CPPWORDKIT_BUILD_TESTS` to also run
the offscreen Qt lifecycle and Unicode-path tests.

## Test notes on Windows

When building with the local MinGW dependency folders, tests need the libxml2,
zlib, and compiler runtime DLL folders on `PATH`. The CMake test targets add
those folders automatically when `CPPWORDKIT_USE_LOCAL_DEPS=ON`; if tests are
run manually, launch them from an environment that includes:

- `D:/libraries/MinGW/libxml2-v2.12.10/bin`
- `D:/libraries/MinGW/zlib/bin`
- the active MinGW compiler `bin` directory

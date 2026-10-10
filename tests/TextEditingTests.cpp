//! 位置编辑回归：Unicode、跨 Run、控制字符、局部格式和事务失败。
#include "cppwordkit/core/CppWordKitCore.hpp"
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

using namespace cppwordkit;
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void unchangedOnFailure(WordDocument& doc, Action action) {
    const auto before = doc.mainDocumentPart().toString();
    bool failed = false;
    try { action(); } catch (const WordProcessingException&) { failed = true; }
    require(failed, "invalid edit succeeded");
    require(doc.mainDocumentPart().toString() == before, "failed edit changed XML");
}
WordDocument document(std::string_view body) {
    auto doc = WordDocument::create();
    doc.mainDocumentPart() = XmlPart::fromString(
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"><w:body>" +
        std::string(body) + "<w:sectPr><w:cols w:num=\"2\"/></w:sectPr></w:body></w:document>");
    return doc;
}
void unicodeAndControls() {
    auto doc = WordDocument::create();
    doc.insertText(0, 0, "中文😀 &<>\tA\r\nB\rC");
    require(doc.paragraphText(0) == "中文😀 &<>\tA\nB\nC", "Unicode/control insertion");
    require(doc.mainDocumentPart().countByXPath("//w:tab") == 1, "tab must use w:tab");
    require(doc.mainDocumentPart().countByXPath("//w:br") == 2, "newline must use w:br");
    doc.deleteRange(0, 1, 2);
    require(doc.paragraphText(0) == "中 &<>\tA\nB\nC", "scalar deletion cut UTF-8");
    doc.insertText(0, 1, "界");
    require(doc.paragraphText(0) == "中界 &<>\tA\nB\nC", "scalar insertion");
    for (const auto& invalid : {std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
         std::string("\xf4\x90\x80\x80"), std::string("\xe4\xb8"), std::string("\0", 1), std::string("\x01")}) {
        unchangedOnFailure(doc, [&] { doc.insertText(0, 0, invalid); });
    }
    unchangedOnFailure(doc, [&] { doc.insertText(0, 99, "x"); });
    unchangedOnFailure(doc, [&] { doc.deleteRange(0, 1, std::numeric_limits<std::size_t>::max()); });
    unchangedOnFailure(doc, [&] { doc.deleteRange(99, 0, 0); });
}
void formattingAndPreservation() {
    auto doc = document(R"(<w:tbl><w:tr><w:tc><w:p><w:r><w:t>Cell</w:t></w:r></w:p></w:tc></w:tr></w:tbl>
<w:p w:rsidR="12345678"><w:pPr><w:keepNext/></w:pPr><w:bookmarkStart w:id="1" w:name="keep"/>
<w:r><w:rPr><w:b/><w:lang w:val="zh-CN"/></w:rPr><w:t>ab中</w:t><w:t>文</w:t></w:r>
<w:hyperlink r:id="rIdLink"><w:r><w:rPr><w:i/></w:rPr><w:t>😀xy</w:t></w:r></w:hyperlink>
<w:bookmarkEnd w:id="1"/><w:r><w:t>z</w:t></w:r></w:p>)");
    TextStyle patch;
    patch.colorHex = "aabbcc";
    doc.applyStyle(0, 1, 5, patch);
    const auto& xml = doc.mainDocumentPart();
    require(doc.paragraphText(0) == "ab中文😀xyz", "formatting changed text");
    require(xml.hasXPath("//w:r[w:t='a']/w:rPr/w:b"), "prefix original style lost");
    require(!xml.hasXPath("//w:r[w:t='a']/w:rPr/w:color"), "style leaked before range");
    require(xml.hasXPath("//w:r[w:t='b中']/w:rPr/w:color[@w:val='AABBCC']"), "selected run style missing");
    require(xml.hasXPath("//w:r[w:t='b中']/w:rPr/w:lang"), "unknown run property lost");
    require(xml.hasXPath("//w:hyperlink/w:r[w:t='😀x']/w:rPr/w:i"), "hyperlink style lost");
    require(!xml.hasXPath("//w:r[w:t='y']/w:rPr/w:color"), "style leaked after range");
    require(!xml.hasXPath("//w:tc//w:color"), "table cell was formatted");
    doc.insertText(0, 5, "Q");
    require(xml.hasXPath("//w:hyperlink[@r:id='rIdLink']/w:r[w:t='Q']/w:rPr/w:i"), "insert did not inherit hyperlink");
    doc.deleteRange(0, 2, 4);
    require(doc.paragraphText(0) == "abxyz", "cross-run/hyperlink delete");
    require(xml.hasXPath("//w:pPr/w:keepNext") && xml.hasXPath("//w:bookmarkStart") &&
        xml.hasXPath("//w:bookmarkEnd") && xml.hasXPath("//w:cols[@w:num='2']"), "unrelated structure lost");
    TextStyle invalid;
    invalid.fontFamily = "Changed";
    invalid.colorHex = "invalid";
    unchangedOnFailure(doc, [&] { doc.applyStyle(0, 1, 3, invalid); });
    unchangedOnFailure(doc, [&] { doc.setRunStyle(0, 0, invalid); });
    invalid.colorHex.reset();
    invalid.fontSizeHalfPoints = -1;
    unchangedOnFailure(doc, [&] { doc.applyStyle(0, 1, 3, invalid); });

    const auto directory = std::filesystem::temp_directory_path() /
        ("cppwordkit-edit-" + std::to_string(std::random_device{}()));
    require(std::filesystem::create_directory(directory), "cannot create temporary directory");
    struct Cleanup { Path path; ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); } } cleanup{directory};
    doc.saveAs(directory / "edit.docx");
    auto reopened = WordDocument::open(directory / "edit.docx");
    require(reopened.paragraphText(0) == doc.paragraphText(0), "save roundtrip text mismatch");
    require(reopened.mainDocumentPart().hasXPath("//w:hyperlink[@r:id='rIdLink']"), "save lost hyperlink");
}
void emptyObjectsAndUnsupported() {
    auto empty = document("<w:p/>");
    empty.insertText(0, 0, "abc");
    empty.deleteRange(0, 0, 3);
    empty.insertText(0, 0, "中文");
    require(empty.paragraphText(0) == "中文", "empty paragraph insertion");
    empty.deleteRange(0, 2, 0);
    auto objects = document("<w:p><w:r><w:t>ab</w:t><w:drawing/><w:t>cd</w:t></w:r></w:p>");
    require(objects.paragraphText(0) == "ab\xef\xbf\xbc" "cd", "object must occupy a position");
    TextStyle style;
    style.bold = true;
    // 文本操作不能悄悄删除或格式化内嵌对象。
    unchangedOnFailure(objects, [&] { objects.deleteRange(0, 1, 3); });
    unchangedOnFailure(objects, [&] { objects.applyStyle(0, 1, 3, style); });
    objects.applyStyle(0, 3, 1, style);
    objects.deleteRange(0, 0, 2);
    require(objects.mainDocumentPart().countByXPath("//w:drawing") == 1, "object duplicated or lost");
    require(objects.paragraphText(0) == "\xef\xbf\xbc" "cd", "editing adjacent to object");
    auto lateFailure = document("<w:p><w:r><w:t>abc</w:t></w:r><w:r><w:drawing/></w:r></w:p>");
    // 前一 Run 已在副本中拆分，后一 Run 遇到对象失败时也必须完整回滚。
    unchangedOnFailure(lateFailure, [&] { lateFailure.deleteRange(0, 1, 3); });
    unchangedOnFailure(lateFailure, [&] { lateFailure.applyStyle(0, 1, 3, style); });
    for (const auto body : {"<w:p><w:r><w:instrText>PAGE</w:instrText></w:r></w:p>",
                           "<w:p><w:ins><w:r><w:t>revision</w:t></w:r></w:ins></w:p>",
                           "<w:p><w:r><w:br w:type='page'/></w:r></w:p>"}) {
        auto unsupported = document(body);
        unchangedOnFailure(unsupported, [&] { unsupported.insertText(0, 0, "x"); });
    }
}
void randomizedEdits() {
    // 固定种子，与独立的 Unicode 标量数组对照，覆盖连续拆分及删除后的再次编辑。
    auto doc = WordDocument::create();
    std::vector<std::string> expected;
    const std::vector<std::string> alphabet{"a", "中", "😀", "&", " ", "\t", "\n"};
    std::mt19937 generator(20261010);
    for (int iteration = 0; iteration < 500; ++iteration) {
        const auto index = generator() % (expected.size() + 1);
        if (generator() % 3 || expected.empty()) {
            const auto value = alphabet[generator() % alphabet.size()];
            doc.insertText(0, index, value);
            expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(index), value);
        } else {
            const auto length = generator() % (expected.size() - index + 1);
            doc.deleteRange(0, index, length);
            expected.erase(expected.begin() + static_cast<std::ptrdiff_t>(index),
                           expected.begin() + static_cast<std::ptrdiff_t>(index + length));
        }
        std::string joined;
        for (const auto& scalar : expected) joined += scalar;
        require(doc.paragraphText(0) == joined, "randomized text differs from scalar model");
        TextStyle style;
        style.italic = iteration % 2 == 0;
        const auto styleStart = generator() % (expected.size() + 1);
        const auto styleLength = generator() % (expected.size() - styleStart + 1);
        doc.applyStyle(0, styleStart, styleLength, style);
        require(doc.paragraphText(0) == joined, "randomized formatting changed text");
    }
}
}
int main() {
    try {
        unicodeAndControls();
        formattingAndPreservation();
        emptyObjectsAndUnsupported();
        randomizedEdits();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

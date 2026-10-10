//! 核心完整性回归：局部编辑保真、混合正文索引、当前部件读取和保存往返。
#include "cppwordkit/WordDocument.hpp"
#include "cppwordkit/XmlPart.hpp"
#include <filesystem>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void requireFailure(F action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    require(failed, "invalid operation unexpectedly succeeded");
}
const char* documentXml = R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><w:body>
<w:p><w:bookmarkStart w:id="0" w:name="keep"/><w:hyperlink r:id="rIdLink"><w:r><w:t>Link</w:t></w:r></w:hyperlink><w:bookmarkEnd w:id="0"/></w:p>
<w:tbl><w:tblPr><w:tblStyle w:val="KeepTableStyle"/></w:tblPr><w:tr><w:tc><w:p><w:pPr><w:jc w:val="right"/></w:pPr><w:r><w:rPr><w:i/></w:rPr><w:t>Cell</w:t></w:r></w:p><w:tbl><w:tr><w:tc><w:p><w:r><w:t>Nested</w:t></w:r></w:p></w:tc></w:tr></w:tbl></w:tc></w:tr></w:tbl>
<w:p><w:pPr><w:jc w:val="center"/></w:pPr><w:r><w:rPr><w:b/></w:rPr><w:t>Target</w:t></w:r></w:p>
<w:tbl><w:tr><w:tc><w:p><w:r><w:t>LastTable</w:t></w:r></w:p></w:tc></w:tr></w:tbl>
<w:sdt><w:sdtContent><w:p><w:r><w:t>Control</w:t></w:r></w:p></w:sdtContent></w:sdt>
<w:sectPr><w:headerReference w:type="default" r:id="rIdHeader"/><w:cols w:num="2"/></w:sectPr>
</w:body></w:document>)";

void requirePreserved(const cppwordkit::WordDocument& doc) {
    const auto& xml = doc.mainDocumentPart();
    require(xml.hasXPath("//w:bookmarkStart[@w:name='keep']"), "bookmark lost");
    require(xml.hasXPath("//w:hyperlink[@r:id='rIdLink']"), "hyperlink lost");
    require(xml.hasXPath("//w:tblPr/w:tblStyle[@w:val='KeepTableStyle']"), "table properties lost");
    require(xml.hasXPath("//w:tbl//w:tbl"), "nested table lost");
    require(xml.hasXPath("//w:sdt//w:t[text()='Control']"), "content control lost");
    require(xml.hasXPath("//w:sectPr/w:headerReference[@r:id='rIdHeader']"), "header reference lost");
    require(xml.hasXPath("/w:document/w:body/*[last()][self::w:sectPr]/w:cols[@w:num='2']"), "section properties moved or lost");
}
}

int main() {
    try {
        auto doc = cppwordkit::WordDocument::create();
        doc.mainDocumentPart() = cppwordkit::XmlPart::fromString(documentXml);
        require(doc.paragraphCount() == 2, "top-level paragraph count");
        require(doc.runStyle(1, 0).bold == true, "style read selected table cell");
        require(doc.paragraphStyle(1).alignment == cppwordkit::ParagraphAlignment::Center, "paragraph style index");
        cppwordkit::TextStyle style;
        style.strike = true;
        doc.setRunStyle(1, 0, style);
        cppwordkit::ParagraphStyle paragraphStyle;
        paragraphStyle.alignment = cppwordkit::ParagraphAlignment::Justify;
        doc.setParagraphStyle(1, paragraphStyle);
        require(doc.mainDocumentPart().hasXPath("/w:document/w:body/w:p[2]/w:r/w:rPr/w:strike"), "style write selected wrong run");
        require(!doc.mainDocumentPart().hasXPath("//w:tc//w:strike"), "cell style changed");
        require(doc.model().paragraphs[1].style.alignment == cppwordkit::ParagraphAlignment::Justify, "model style mismatch");
        require(doc.model().tables.size() == 2, "nested table leaked into top-level model");
        require(doc.model().bodyBlocks.back().table.rows[0][0].text == "LastTable", "nested table displaced last table");

        cppwordkit::Paragraph replacement;
        replacement.runs.push_back({.text = "New & text"});
        doc.setParagraph(1, replacement);
        requirePreserved(doc);
        doc.insertParagraph(1, replacement);
        requirePreserved(doc);
        doc.removeParagraph(1);
        requirePreserved(doc);
        doc.appendParagraph(replacement);
        requirePreserved(doc);
        const auto beforeFailure = doc.mainDocumentPart().toString();
        requireFailure([&] { doc.removeParagraph(99); });
        requireFailure([&] { doc.insertParagraph(99, replacement); });
        requireFailure([&] { doc.setParagraph(99, replacement); });
        requireFailure([&] { doc.setRunStyle(99, 0, style); });
        requireFailure([&] { doc.mainDocumentPart().replaceElementXml("/w:document/w:body/w:p[2]", "<broken>"); });
        requireFailure([&] { doc.mainDocumentPart().removeElement("/w:document"); });
        require(doc.mainDocumentPart().toString() == beforeFailure, "failed edit changed document");
        const auto raw = doc.rawPart("word/document.xml");
        require(raw.has_value(), "missing raw XML");
        require(std::string(raw->begin(), raw->end()) == doc.mainDocumentPart().toString(), "raw XML is stale");
        require(!doc.rawPart("missing.bin"), "missing part returned data");

        // 唯一临时目录防止并行测试互相覆盖，异常退出也清理测试文件。
        const auto directory = std::filesystem::temp_directory_path() /
            ("cppwordkit-integrity-" + std::to_string(std::random_device{}()));
        require(std::filesystem::create_directory(directory), "cannot create test directory");
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
        } cleanup{directory};
        doc.saveAs(directory / "roundtrip.docx");
        auto reopened = cppwordkit::WordDocument::open(directory / "roundtrip.docx");
        requirePreserved(reopened);
        require(reopened.paragraph(1).runs[0].text == "New & text", "paragraph roundtrip failed");

        auto empty = cppwordkit::WordDocument::create();
        empty.removeParagraph(0);
        empty.appendParagraph(replacement);
        require(empty.paragraphCount() == 1, "append to empty body failed");
        empty.mainDocumentPart().removeElement("//w:sectPr");
        empty.insertParagraph(0, replacement);
        empty.appendParagraph(replacement);
        require(empty.paragraphCount() == 3, "insert without section properties failed");
        // 重复替换/删除可暴露 XPath 结果集仍引用已释放节点的生命周期问题。
        for (int i = 0; i < 300; ++i) {
            empty.setParagraph(0, replacement);
            empty.insertParagraph(0, replacement);
            empty.removeParagraph(0);
        }
        require(empty.paragraphCount() == 3, "repeated local edits changed paragraph count");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

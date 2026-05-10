#include <cppwordkit/CppWordKit.hpp>

#include <cassert>
#include <string>

int main() {
    auto part = cppwordkit::XmlPart::fromString(
        R"(<root xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:t>Hello Name</w:t><w:t>Second</w:t></root>)"
    );

    assert(!part.empty());
    assert(part.hasXPath("//w:t"));
    assert(part.firstTextByXPath("//w:t").value() == "Hello Name");
    assert(part.textsByXPath("//w:t").size() == 2);
    assert(part.replaceText("Name", "CppWordKit"));
    assert(part.firstTextByXPath("//w:t").value() == "Hello CppWordKit");

    part.setTextByXPath("(//w:t)[2]", "Updated");
    assert(part.textsByXPath("//w:t")[1] == "Updated");

    auto singleNode = cppwordkit::XmlPart::fromString(
        R"(<root xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:t>Hello {{name}}</w:t></root>)"
    );
    assert(singleNode.replaceWordText("{{name}}", "Alice"));
    assert(singleNode.textsByXPath("//w:t")[0] == "Hello Alice");

    auto splitTwoNodes = cppwordkit::XmlPart::fromString(
        R"(<root xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:t>Hello {{na</w:t><w:t>me}}</w:t></root>)"
    );
    assert(splitTwoNodes.replaceWordText("{{name}}", "Alice"));
    auto splitTwoTexts = splitTwoNodes.textsByXPath("//w:t");
    assert(splitTwoTexts[0] == "Hello Alice");
    assert(splitTwoTexts[1].empty());

    auto splitThreeNodes = cppwordkit::XmlPart::fromString(
        R"(<root xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:t>{{</w:t><w:t>name</w:t><w:t>}}</w:t></root>)"
    );
    assert(splitThreeNodes.replaceWordText("{{name}}", "Alice"));
    auto splitThreeTexts = splitThreeNodes.textsByXPath("//w:t");
    assert(splitThreeTexts[0] == "Alice");
    assert(splitThreeTexts[1].empty());
    assert(splitThreeTexts[2].empty());

    auto splitWithSuffix = cppwordkit::XmlPart::fromString(
        R"(<root xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:t>A {{na</w:t><w:t>me}} B</w:t></root>)"
    );
    assert(splitWithSuffix.replaceWordText("{{name}}", "Alice"));
    auto suffixTexts = splitWithSuffix.textsByXPath("//w:t");
    assert(suffixTexts[0] == "A Alice B");
    assert(suffixTexts[1].empty());

    auto tablePart = cppwordkit::XmlPart::fromString(
        R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:tbl><w:tr><w:tc><w:p><w:r><w:t>${table_row}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Role</w:t></w:r></w:p></w:tc></w:tr></w:tbl></w:body></w:document>)"
    );
    const cppwordkit::TableData rows = {
        {{"Alice"}, {"Engineer"}},
        {{"Bob"}, {"Writer"}}
    };
    assert(tablePart.insertTableRowsAtBookmark("table_row", rows) == 2);
    const auto tableRows = tablePart.textsByXPath("//w:tr");
    assert(tableRows.size() == 3);
    assert(tableRows[0] == "Role");
    assert(tableRows[1] == "AliceEngineer");
    assert(tableRows[2] == "BobWriter");

    auto stylePart = cppwordkit::XmlPart::fromString(
        R"(<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:r><w:t>Styled</w:t></w:r></w:p></w:body></w:document>)"
    );
    cppwordkit::TextStyle textStyle;
    textStyle.fontFamily = "Calibri";
    textStyle.colorHex = "ff0000";
    textStyle.fontSizeHalfPoints = 24;
    textStyle.bold = true;
    textStyle.italic = false;
    textStyle.underline = true;
    stylePart.setRunStyle(0, 0, textStyle);
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:rFonts[@w:ascii='Calibri'][@w:hAnsi='Calibri'][@w:eastAsia='Calibri']"));
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:color[@w:val='FF0000']"));
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:sz[@w:val='24']"));
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:b"));
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:i[@w:val='0']"));
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:u[@w:val='single']"));

    auto readTextStyle = stylePart.runStyle(0, 0);
    assert(readTextStyle.fontFamily == "Calibri");
    assert(readTextStyle.colorHex == "FF0000");
    assert(readTextStyle.fontSizeHalfPoints == 24);
    assert(readTextStyle.bold == true);
    assert(readTextStyle.italic == false);
    assert(readTextStyle.underline == true);

    cppwordkit::TextStyle colorOnly;
    colorOnly.colorHex = "00AA11";
    stylePart.setRunStyle(0, 0, colorOnly);
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:rFonts[@w:ascii='Calibri']"));
    assert(stylePart.hasXPath("(//w:r)[1]/w:rPr/w:color[@w:val='00AA11']"));

    cppwordkit::ParagraphStyle paragraphStyle;
    paragraphStyle.alignment = cppwordkit::ParagraphAlignment::Center;
    paragraphStyle.lineSpacingTwips = 360;
    paragraphStyle.lineSpacingRule = cppwordkit::LineSpacingRule::Auto;
    paragraphStyle.firstLineIndentTwips = 420;
    paragraphStyle.leftIndentTwips = 120;
    paragraphStyle.rightIndentTwips = 240;
    stylePart.setParagraphStyle(0, paragraphStyle);
    assert(stylePart.hasXPath("(//w:p)[1]/w:pPr/w:jc[@w:val='center']"));
    assert(stylePart.hasXPath("(//w:p)[1]/w:pPr/w:spacing[@w:line='360'][@w:lineRule='auto']"));
    assert(stylePart.hasXPath("(//w:p)[1]/w:pPr/w:ind[@w:firstLine='420'][@w:left='120'][@w:right='240']"));

    auto readParagraphStyle = stylePart.paragraphStyle(0);
    assert(readParagraphStyle.alignment == cppwordkit::ParagraphAlignment::Center);
    assert(readParagraphStyle.lineSpacingTwips == 360);
    assert(readParagraphStyle.lineSpacingRule == cppwordkit::LineSpacingRule::Auto);
    assert(readParagraphStyle.firstLineIndentTwips == 420);
    assert(readParagraphStyle.leftIndentTwips == 120);
    assert(readParagraphStyle.rightIndentTwips == 240);
}

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
}

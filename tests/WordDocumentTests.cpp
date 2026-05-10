#include <cppwordkit/CppWordKit.hpp>

#include "ZipArchive.hpp"

#include <cassert>
#include <filesystem>
#include <map>

namespace {

constexpr const char* splitDocumentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p>
      <w:r><w:t>Hello {{na</w:t></w:r>
      <w:r><w:t>me}}</w:t></w:r>
    </w:p>
  </w:body>
</w:document>
)";

constexpr const char* multiPlaceholderDocumentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p>
      <w:r><w:t>Hello {{na</w:t></w:r>
      <w:r><w:t>me}}, </w:t></w:r>
      <w:r><w:t>{{title}}</w:t></w:r>
    </w:p>
  </w:body>
</w:document>
)";

constexpr const char* tableDocumentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:tbl>
      <w:tr>
        <w:tc><w:p><w:r><w:t>${table_row}</w:t></w:r></w:p></w:tc>
        <w:tc><w:p><w:r><w:t>Role</w:t></w:r></w:p></w:tc>
      </w:tr>
    </w:tbl>
  </w:body>
</w:document>
)";

} // namespace

int main() {
    auto document = cppwordkit::WordDocument::create();
    assert(document.isOpen());
    assert(document.tableCount() == 0);

    document.mainDocumentPart().setTextByXPath("//w:t", "Hello {{name}}");
    assert(document.text() == "Hello {{name}}");
    assert(document.replaceText("{{name}}", "CppWordKit"));
    assert(document.text() == "Hello CppWordKit");

    const auto output = std::filesystem::temp_directory_path() / "cppwordkit_create_test.docx";
    document.saveAs(output);
    auto reopened = cppwordkit::WordDocument::open(output);
    assert(reopened.text() == "Hello CppWordKit");
    std::filesystem::remove(output);

    const auto splitInput = std::filesystem::temp_directory_path() / "cppwordkit_split_input.docx";
    const auto splitOutput = std::filesystem::temp_directory_path() / "cppwordkit_split_output.docx";

    cppwordkit::ZipArchive archive;
    archive.setText("word/document.xml", splitDocumentXml);
    archive.write(splitInput);

    auto splitDocument = cppwordkit::WordDocument::open(splitInput);
    assert(splitDocument.ReplaceText("name", "Alice"));
    splitDocument.saveAs(splitOutput);

    auto splitReopened = cppwordkit::WordDocument::open(splitOutput);
    assert(splitReopened.text() == "Hello Alice");

    std::filesystem::remove(splitInput);
    std::filesystem::remove(splitOutput);

    const auto multiInput = std::filesystem::temp_directory_path() / "cppwordkit_multi_input.docx";
    const auto multiOutput = std::filesystem::temp_directory_path() / "cppwordkit_multi_output.docx";

    cppwordkit::ZipArchive multiArchive;
    multiArchive.setText("word/document.xml", multiPlaceholderDocumentXml);
    multiArchive.write(multiInput);

    auto multiDocument = cppwordkit::WordDocument::open(multiInput);
    const std::map<std::string, std::string> replacements = {
        {"name", "Alice"},
        {"{{title}}", "Engineer"},
        {"missing", "Ignored"}
    };
    assert(multiDocument.ReplaceText(replacements) == 2);
    multiDocument.saveAs(multiOutput);

    auto multiReopened = cppwordkit::WordDocument::open(multiOutput);
    assert(multiReopened.text() == "Hello Alice, Engineer");

    std::filesystem::remove(multiInput);
    std::filesystem::remove(multiOutput);

    const auto tableInput = std::filesystem::temp_directory_path() / "cppwordkit_table_input.docx";
    const auto tableOutput = std::filesystem::temp_directory_path() / "cppwordkit_table_output.docx";

    cppwordkit::ZipArchive tableArchive;
    tableArchive.setText("word/document.xml", tableDocumentXml);
    tableArchive.write(tableInput);

    auto tableDocument = cppwordkit::WordDocument::open(tableInput);
    const cppwordkit::TableData rows = {
        {{"Alice"}, {"Engineer"}},
        {{"Bob"}, {"Writer"}}
    };
    assert(tableDocument.insertTableRowsAtBookmark("${table_row}", rows) == 2);
    tableDocument.saveAs(tableOutput);

    auto tableReopened = cppwordkit::WordDocument::open(tableOutput);
    assert(tableReopened.mainDocumentPart().textsByXPath("//w:tr").size() == 3);
    assert(tableReopened.text() == "RoleAliceEngineerBobWriter");

    std::filesystem::remove(tableInput);
    std::filesystem::remove(tableOutput);
}

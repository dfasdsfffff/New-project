#include <cppwordkit/CppWordKit.hpp>

#include <cassert>
#include <filesystem>

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
}

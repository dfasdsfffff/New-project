#include <cppwordkit/CppWordKit.hpp>

#include "ZipArchive.hpp"

#include <cassert>
#include <filesystem>
#include <stdexcept>

namespace {

constexpr const char* templatedDocumentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p>
      <w:r><w:t>Hello {{ user.na</w:t></w:r>
      <w:r><w:t>me | upper }}</w:t></w:r>
      <w:r><w:t>, {{ missing | default('friend') }}</w:t></w:r>
    </w:p>
  </w:body>
</w:document>
)";

constexpr const char* headerXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:hdr xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:p><w:r><w:t>{{ title | lower }}</w:t></w:r></w:p>
</w:hdr>
)";

} // namespace

int main() {
    const cppwordkit::TemplateContext context = {
        {"user", cppwordkit::TemplateValue::Object{{"name", "Alice"}}},
        {"title", "MONTHLY REPORT"}
    };

    const auto rendered = cppwordkit::TemplateEngine::renderText(
        "Hello {{ user.name | upper }} {{ missing | default('friend') }}",
        context
    );
    assert(rendered.text == "Hello ALICE friend");
    assert(rendered.usedVariables.contains("user"));
    assert(rendered.usedVariables.contains("missing"));

    const auto resultVariable = cppwordkit::TemplateEngine::renderText("{{ result }}", {{"result", "ok"}});
    assert(resultVariable.text == "ok");

    const auto undeclared = cppwordkit::TemplateEngine::undeclaredVariables("{{ user.name }} {{ other }}", context);
    assert(undeclared.size() == 1);
    assert(undeclared.contains("other"));

    bool threwUnsupported = false;
    try {
        cppwordkit::TemplateEngine::validateText("{% if user %}");
    } catch (const cppwordkit::WordProcessingException&) {
        threwUnsupported = true;
    }
    assert(threwUnsupported);

    const auto input = std::filesystem::temp_directory_path() / "cppwordkit_template_input.docx";
    const auto output = std::filesystem::temp_directory_path() / "cppwordkit_template_output.docx";

    cppwordkit::ZipArchive archive;
    archive.setText("word/document.xml", templatedDocumentXml);
    archive.setText("word/header1.xml", headerXml);
    archive.write(input);

    auto document = cppwordkit::WordDocument::open(input);
    assert(document.getUndeclaredTemplateVariables(context).empty());
    document.render(context);
    document.saveAs(output);

    auto reopened = cppwordkit::WordDocument::open(output);
    assert(reopened.text() == "Hello ALICE, friend");
    auto header = reopened.part("word/header1.xml");
    assert(header.has_value());
    assert((*header)->textsByXPath("//w:t").front() == "monthly report");

    std::filesystem::remove(input);
    std::filesystem::remove(output);
}

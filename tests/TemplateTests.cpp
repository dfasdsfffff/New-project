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

constexpr const char* controlDocumentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p><w:r><w:t>{%p if show_intro %}</w:t></w:r></w:p>
    <w:p><w:r><w:t>Intro {{ user.name | title }}</w:t></w:r></w:p>
    <w:p><w:r><w:t>{%p endif %}</w:t></w:r></w:p>
    <w:tbl>
      <w:tr><w:tc><w:p><w:r><w:t>Name</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>Qty</w:t></w:r></w:p></w:tc></w:tr>
      <w:tr><w:tc><w:p><w:r><w:t>{%tr for row in rows %}</w:t></w:r></w:p></w:tc></w:tr>
      <w:tr><w:tc><w:p><w:r><w:t>{{ loop.index }}. {{ row.name | capitalize }}</w:t></w:r></w:p></w:tc><w:tc><w:p><w:r><w:t>{{ row.qty }}</w:t></w:r></w:p></w:tc></w:tr>
      <w:tr><w:tc><w:p><w:r><w:t>{%tr endfor %}</w:t></w:r></w:p></w:tc></w:tr>
    </w:tbl>
    <w:p><w:r><w:t>{# hidden #}Done {{ words | join(',') | replace('a','A') }}</w:t></w:r></w:p>
  </w:body>
</w:document>
)";

} // namespace

int main() {
    const cppwordkit::TemplateContext context = {
        {"user", cppwordkit::TemplateValue::Object{{"name", "Alice"}}},
        {"title", "MONTHLY REPORT"},
        {"show_intro", true},
        {"rows", cppwordkit::TemplateValue::Array{
            cppwordkit::TemplateValue::Object{{"name", "apple"}, {"qty", std::int64_t{2}}},
            cppwordkit::TemplateValue::Object{{"name", "banana"}, {"qty", std::int64_t{5}}}
        }},
        {"words", cppwordkit::TemplateValue::Array{"alpha", "beta"}}
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

    const auto controlRendered = cppwordkit::TemplateEngine::renderText(
        "{% if show_intro %}{{ user.name | lower }}{% else %}hidden{% endif %}"
        "{% for row in rows %}[{{ loop.index0 }}:{{ row.name | capitalize }}]{% endfor %}"
        "{{ words | join('|') }} {{ ' padded ' | trim }} {{ title | length }}",
        context
    );
    assert(controlRendered.text == "alice[0:Apple][1:Banana]alpha|beta padded 14");

    const auto undeclared = cppwordkit::TemplateEngine::undeclaredVariables("{{ user.name }} {{ other }}", context);
    assert(undeclared.size() == 1);
    assert(undeclared.contains("other"));

    assert(cppwordkit::TemplateEngine::diagnostics("{% if show_intro %}ok{% endif %}", context).ok());

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

    auto tpl = cppwordkit::DocxTemplate::open(input);
    assert(tpl.getUndeclaredTemplateVariables(context).empty());
    tpl.render(context);
    tpl.saveAs(output);

    auto reopened = cppwordkit::WordDocument::open(output);
    assert(reopened.text() == "Hello ALICE, friend");
    auto header = reopened.part("word/header1.xml");
    assert(header.has_value());
    assert((*header)->textsByXPath("//w:t").front() == "monthly report");

    auto compatibilityDocument = cppwordkit::WordDocument::open(input);
    assert(compatibilityDocument.getUndeclaredTemplateVariables(context).empty());
    compatibilityDocument.render(context);
    assert(compatibilityDocument.text() == "Hello ALICE, friend");

    const auto controlInput = std::filesystem::temp_directory_path() / "cppwordkit_template_control_input.docx";
    const auto controlOutput = std::filesystem::temp_directory_path() / "cppwordkit_template_control_output.docx";
    cppwordkit::ZipArchive controlArchive;
    controlArchive.setText("word/document.xml", controlDocumentXml);
    controlArchive.write(controlInput);

    auto controlTpl = cppwordkit::DocxTemplate::open(controlInput);
    assert(controlTpl.getUndeclaredTemplateVariables(context).empty());
    controlTpl.render(context);
    controlTpl.saveAs(controlOutput);

    auto controlReopened = cppwordkit::WordDocument::open(controlOutput);
    const auto controlText = controlReopened.text();
    assert(controlText.find("Intro Alice") != std::string::npos);
    assert(controlText.find("1. Apple2") != std::string::npos);
    assert(controlText.find("2. Banana5") != std::string::npos);
    assert(controlText.find("Done AlphA,betA") != std::string::npos);
    assert(controlText.find("{%") == std::string::npos);
    assert(controlText.find("hidden") == std::string::npos);

    std::filesystem::remove(input);
    std::filesystem::remove(output);
    std::filesystem::remove(controlInput);
    std::filesystem::remove(controlOutput);
}

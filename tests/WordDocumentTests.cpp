#include <cppwordkit/CppWordKit.hpp>

#include "ZipArchive.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

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

constexpr const char* imagePlaceholderDocumentXml = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p>
      <w:r><w:t>{{rep</w:t></w:r>
      <w:r><w:t>lace}}</w:t></w:r>
    </w:p>
  </w:body>
</w:document>
)";

void writeTinyPng(const std::filesystem::path& path) {
    const std::vector<std::uint8_t> png = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
        0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4,
        0x89, 0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9c, 0x63, 0xf8, 0x0f, 0x00, 0x01,
        0x01, 0x01, 0x00, 0x18, 0xdd, 0x8d, 0xb0, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
        0x42, 0x60, 0x82
    };
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

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

    const auto imageInput = std::filesystem::temp_directory_path() / "cppwordkit_image_input.docx";
    const auto imagePath = std::filesystem::temp_directory_path() / "cppwordkit_test_image.png";
    const auto imageOutput = std::filesystem::temp_directory_path() / "cppwordkit_image_output.docx";
    cppwordkit::ZipArchive imageArchive;
    imageArchive.setText("[Content_Types].xml", R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="xml" ContentType="application/xml"/><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/></Types>)");
    imageArchive.setText("word/document.xml", imagePlaceholderDocumentXml);
    imageArchive.write(imageInput);
    writeTinyPng(imagePath);

    auto imageDocument = cppwordkit::WordDocument::open(imageInput);
    const cppwordkit::ImageOptions imageOptions{
        .widthEmu = 100000,
        .heightEmu = 100000,
        .description = "Unit test image"
    };
    assert(imageDocument.insertImageAtPlaceholder("replace", imagePath, imageOptions));
    imageDocument.saveAs(imageOutput);

    auto imageReopened = cppwordkit::WordDocument::open(imageOutput);
    const auto imagePartNames = imageReopened.partNames();
    assert(imagePartNames.end() != std::find(imagePartNames.begin(), imagePartNames.end(), "word/media/image1.png"));
    assert(imageReopened.mainDocumentPart().hasXPath("//w:drawing"));
    assert(imageReopened.part("word/_rels/document.xml.rels").has_value());
    assert((*imageReopened.part("word/_rels/document.xml.rels"))->hasXPath("//rel:Relationship[@Type='http://schemas.openxmlformats.org/officeDocument/2006/relationships/image']"));
    assert(imageReopened.part("[Content_Types].xml").has_value());
    assert((*imageReopened.part("[Content_Types].xml"))->hasXPath("/*[local-name()='Types']/*[local-name()='Default'][@Extension='png']"));

    std::filesystem::remove(imageInput);
    std::filesystem::remove(imagePath);
    std::filesystem::remove(imageOutput);

    auto styledDocument = cppwordkit::WordDocument::create();
    styledDocument.mainDocumentPart().setTextByXPath("//w:t", "Styled");

    cppwordkit::TextStyle runStyle;
    runStyle.fontFamily = "Arial";
    runStyle.colorHex = "336699";
    runStyle.fontSizeHalfPoints = 28;
    runStyle.bold = true;
    styledDocument.setRunStyle(0, 0, runStyle);

    cppwordkit::ParagraphStyle paragraphStyle;
    paragraphStyle.alignment = cppwordkit::ParagraphAlignment::Justify;
    paragraphStyle.lineSpacingTwips = 480;
    paragraphStyle.lineSpacingRule = cppwordkit::LineSpacingRule::Exact;
    paragraphStyle.firstLineIndentTwips = 420;
    styledDocument.setParagraphStyle(0, paragraphStyle);

    const auto styledOutput = std::filesystem::temp_directory_path() / "cppwordkit_styled_output.docx";
    styledDocument.saveAs(styledOutput);
    auto styledReopened = cppwordkit::WordDocument::open(styledOutput);
    auto reopenedRunStyle = styledReopened.runStyle(0, 0);
    assert(reopenedRunStyle.fontFamily == "Arial");
    assert(reopenedRunStyle.colorHex == "336699");
    assert(reopenedRunStyle.fontSizeHalfPoints == 28);
    assert(reopenedRunStyle.bold == true);
    auto reopenedParagraphStyle = styledReopened.paragraphStyle(0);
    assert(reopenedParagraphStyle.alignment == cppwordkit::ParagraphAlignment::Justify);
    assert(reopenedParagraphStyle.lineSpacingTwips == 480);
    assert(reopenedParagraphStyle.lineSpacingRule == cppwordkit::LineSpacingRule::Exact);
    assert(reopenedParagraphStyle.firstLineIndentTwips == 420);
    assert(styledReopened.mainDocumentPart().hasXPath("(//w:p)[1]/w:pPr/w:jc[@w:val='both']"));
    assert(styledReopened.mainDocumentPart().hasXPath("(//w:r)[1]/w:rPr/w:color[@w:val='336699']"));

    auto structuredDocument = cppwordkit::WordDocument::create();
    cppwordkit::DocumentModel structuredModel;

    cppwordkit::Paragraph firstParagraph;
    firstParagraph.style.alignment = cppwordkit::ParagraphAlignment::Center;
    firstParagraph.style.spacingBeforeTwips = 120;
    firstParagraph.style.spacingAfterTwips = 240;
    cppwordkit::Run firstRun;
    firstRun.text = "Hello ";
    firstRun.style.bold = true;
    cppwordkit::Run secondRun;
    secondRun.text = "model";
    secondRun.style.italic = true;
    secondRun.style.backgroundColorHex = "ABCDEF";
    firstParagraph.runs = {firstRun, secondRun};
    structuredModel.paragraphs.push_back(firstParagraph);

    cppwordkit::Paragraph secondParagraph;
    secondParagraph.runs.push_back({.text = "Second paragraph"});
    structuredModel.paragraphs.push_back(secondParagraph);

    structuredModel.tables.push_back({{{{"A1"}, {"B1"}}, {{"A2"}, {"B2"}}}});
    structuredDocument.replaceBody(structuredModel);
    assert(structuredDocument.paragraphCount() == 2);
    assert(structuredDocument.text() == "Hello modelSecond paragraphA1B1A2B2");
    const auto readModel = structuredDocument.model();
    assert(readModel.paragraphs.size() == 2);
    assert(readModel.paragraphs[0].runs.size() == 2);
    assert(readModel.paragraphs[0].runs[0].text == "Hello ");
    assert(readModel.paragraphs[0].runs[0].style.bold == true);
    assert(readModel.paragraphs[0].runs[1].style.italic == true);
    assert(readModel.paragraphs[0].runs[1].style.backgroundColorHex == "ABCDEF");
    assert(readModel.paragraphs[0].style.alignment == cppwordkit::ParagraphAlignment::Center);
    assert(readModel.paragraphs[0].style.spacingBeforeTwips == 120);
    assert(readModel.tables.size() == 1);
    assert(readModel.tables[0].rows[1][1].text == "B2");

    cppwordkit::Paragraph insertedParagraph;
    insertedParagraph.runs.push_back({.text = "Inserted"});
    structuredDocument.insertParagraph(1, insertedParagraph);
    assert(structuredDocument.paragraphCount() == 3);
    assert(structuredDocument.paragraph(1).runs[0].text == "Inserted");
    structuredDocument.removeParagraph(1);
    assert(structuredDocument.paragraphCount() == 2);
    cppwordkit::Paragraph replacementParagraph;
    replacementParagraph.runs.push_back({.text = "Replacement"});
    structuredDocument.setParagraph(1, replacementParagraph);
    assert(structuredDocument.paragraph(1).runs[0].text == "Replacement");
    structuredDocument.appendParagraph(insertedParagraph);
    assert(structuredDocument.paragraphCount() == 3);

    const auto modelOutput = std::filesystem::temp_directory_path() / "cppwordkit_model_output.docx";
    structuredDocument.saveAs(modelOutput);
    auto modelReopened = cppwordkit::WordDocument::open(modelOutput);
    assert(modelReopened.paragraphCount() == 3);
    assert(modelReopened.model().tables[0].rows[0][0].text == "A1");
    std::filesystem::remove(modelOutput);

    auto imageApiDocument = cppwordkit::WordDocument::create();
    cppwordkit::DocumentModel imageModel;
    imageModel.paragraphs.push_back({{{"Image paragraph"}}});
    imageApiDocument.replaceBody(imageModel);
    const auto imageApiPath = std::filesystem::temp_directory_path() / "cppwordkit_model_image.png";
    const auto imageApiOutput = std::filesystem::temp_directory_path() / "cppwordkit_model_image_output.docx";
    writeTinyPng(imageApiPath);
    const auto imageId = imageApiDocument.addImage(imageApiPath);
    imageApiDocument.insertImage(0, 0, imageId, imageOptions);
    auto imageApiModel = imageApiDocument.model();
    const auto imageRun = std::find_if(
        imageApiModel.paragraphs[0].runs.begin(),
        imageApiModel.paragraphs[0].runs.end(),
        [](const cppwordkit::Run& run) { return !run.images.empty(); }
    );
    assert(imageRun != imageApiModel.paragraphs[0].runs.end());
    assert(imageRun->images[0].image.relationshipId == imageId.relationshipId);
    imageApiDocument.saveAs(imageApiOutput);
    auto imageApiReopened = cppwordkit::WordDocument::open(imageApiOutput);
    assert(imageApiReopened.mainDocumentPart().hasXPath("//w:drawing"));
    const auto reopenedImageModel = imageApiReopened.model();
    const auto reopenedImageRun = std::find_if(
        reopenedImageModel.paragraphs[0].runs.begin(),
        reopenedImageModel.paragraphs[0].runs.end(),
        [](const cppwordkit::Run& run) { return !run.images.empty(); }
    );
    assert(reopenedImageRun != reopenedImageModel.paragraphs[0].runs.end());
    std::filesystem::remove(imageApiPath);
    std::filesystem::remove(imageApiOutput);

    const auto unknownInput = std::filesystem::temp_directory_path() / "cppwordkit_unknown_input.docx";
    const auto unknownOutput = std::filesystem::temp_directory_path() / "cppwordkit_unknown_output.docx";
    cppwordkit::ZipArchive unknownArchive;
    unknownArchive.setText("[Content_Types].xml", R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="xml" ContentType="application/xml"/><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/></Types>)");
    unknownArchive.setText("_rels/.rels", R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>)");
    unknownArchive.setText("word/document.xml", R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:r><w:t>Old</w:t></w:r></w:p></w:body></w:document>)");
    unknownArchive.setText("customXml/item1.xml", R"(<root>keep me</root>)");
    unknownArchive.write(unknownInput);
    auto unknownDocument = cppwordkit::WordDocument::open(unknownInput);
    cppwordkit::DocumentModel unknownModel;
    unknownModel.paragraphs.push_back({{{"New"}}});
    unknownDocument.replaceBody(unknownModel);
    unknownDocument.saveAs(unknownOutput);
    const auto unknownReopenedParts = cppwordkit::WordDocument::open(unknownOutput).partNames();
    assert(unknownReopenedParts.end() != std::find(unknownReopenedParts.begin(), unknownReopenedParts.end(), "customXml/item1.xml"));
    std::filesystem::remove(unknownInput);
    std::filesystem::remove(unknownOutput);

    bool threwOutOfRange = false;
    try {
        styledReopened.setRunStyle(99, 0, runStyle);
    } catch (const cppwordkit::WordProcessingException&) {
        threwOutOfRange = true;
    }
    assert(threwOutOfRange);

    bool threwInvalidColor = false;
    try {
        cppwordkit::TextStyle invalidColor;
        invalidColor.colorHex = "red";
        styledReopened.setRunStyle(0, 0, invalidColor);
    } catch (const cppwordkit::WordProcessingException&) {
        threwInvalidColor = true;
    }
    assert(threwInvalidColor);

    std::filesystem::remove(styledOutput);
}

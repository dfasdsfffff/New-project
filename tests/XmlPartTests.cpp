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
}

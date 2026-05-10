#include <cppwordkit/CppWordKit.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: replace_text <input.docx> <output.docx> <search> <replacement>\n";
        return 1;
    }

    auto document = cppwordkit::WordDocument::open(argv[1]);
    document.replaceText(argv[3], argv[4]);
    document.saveAs(argv[2]);
}

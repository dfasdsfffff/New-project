#include <cppwordkit/CppWordKit.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: read_text <input.docx>\n";
        return 1;
    }

    const auto document = cppwordkit::WordDocument::open(argv[1]);
    std::cout << document.text() << '\n';
}

#include <cppwordkit/CppWordKit.hpp>

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: fill_table <input.docx> <output.docx>\n";
        return 1;
    }

    auto document = cppwordkit::WordDocument::open(argv[1]);
    document.fillFirstTable({
        {{"Name"}, {"Role"}},
        {{"CppWordKit"}, {"Library"}}
    });
    document.saveAs(argv[2]);
}

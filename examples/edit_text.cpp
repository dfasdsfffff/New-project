// 仅使用核心库演示 Unicode 位置编辑，无需 Qt 和模板引擎。
#include <cppwordkit/core/CppWordKitCore.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: edit_text <output.docx>\n";
        return 1;
    }
    try {
        auto doc = cppwordkit::WordDocument::create();
        doc.insertText(0, 0, "你好😀\tWorld\n第二行");
        doc.deleteRange(0, 2, 1);
        cppwordkit::TextStyle style;
        style.bold = true;
        style.colorHex = "336699";
        doc.applyStyle(0, 0, 2, style);
        std::cout << doc.paragraphText(0) << '\n';
        doc.saveAs(argv[1]);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

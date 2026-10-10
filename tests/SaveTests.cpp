//! 保存回归：完整替换、失败保留、禁止覆盖、Unicode 路径和暂存清理。
#include "cppwordkit/WordDocument.hpp"
#include "ZipArchive.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void requireFailure(F action) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    require(failed, "save unexpectedly succeeded");
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "cannot read saved file");
    return {std::istreambuf_iterator<char>(stream), {}};
}
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        std::random_device random;
        for (int i = 0; i < 32; ++i) {
            auto candidate = std::filesystem::temp_directory_path() /
                ("cppwordkit-save-test-" + std::to_string(random()));
            if (std::filesystem::create_directory(candidate)) { path = candidate; return; }
        }
        throw std::runtime_error("cannot create test directory");
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
void requireNoStaging(const std::filesystem::path& directory) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        require(!entry.path().filename().string().starts_with(".cppwordkit-save-"),
                "staging directory leaked");
    }
}
#ifdef _WIN32
struct FileLock {
    HANDLE handle;
    explicit FileLock(const std::filesystem::path& path)
        : handle(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        require(handle != INVALID_HANDLE_VALUE, "cannot lock test file");
    }
    ~FileLock() { CloseHandle(handle); }
};
#endif
}

int main() {
    try {
        TemporaryDirectory directory;
        const auto path = directory.path / std::filesystem::path(u"中文报告.docx");
        auto doc = cppwordkit::WordDocument::create();
        cppwordkit::Paragraph paragraph;
        paragraph.runs.push_back({.text = "original"});
        doc.setParagraph(0, paragraph);
        doc.saveAs(path);
        const auto original = bytes(path);
        paragraph.runs[0].text = "updated";
        doc.setParagraph(0, paragraph);

        requireFailure([&] { doc.saveAs(path, {.overwrite = false}); });
        require(bytes(path) == original, "no-overwrite changed original file");

        // 直接验证提交层的禁止覆盖，不能只依赖 WordDocument 的 exists 检查。
        cppwordkit::ZipArchive archive;
        archive.setText("word/document.xml", "replacement");
        requireFailure([&] { archive.write(path, false); });
        require(bytes(path) == original, "commit overwrote existing file");
        requireNoStaging(directory.path);

#ifdef _WIN32
        {
            // 允许写入但禁止删除，旧实现会截断；新实现必须完整保留原文件。
            FileLock lock(path);
            requireFailure([&] { doc.save(); });
            require(bytes(path) == original, "failed replacement damaged original");
            require(doc.path() == path && doc.text() == "updated", "failed save lost document state");
            requireNoStaging(directory.path);
        }
#endif
        const auto blocked = directory.path / "blocked.docx";
        std::filesystem::create_directory(blocked);
        std::ofstream(blocked / "marker") << "keep";
        requireFailure([&] { doc.saveAs(blocked); });
        require(bytes(blocked / "marker") == "keep", "failed save changed destination directory");
        require(doc.path() == path, "failed save changed source path");
        requireFailure([&] { doc.saveAs(directory.path / "missing" / "new.docx"); });
        requireNoStaging(directory.path);

        doc.save();
        require(cppwordkit::WordDocument::open(path).text() == "updated", "replacement not readable");
        const auto copy = directory.path / "copy.docx";
        doc.saveAs(copy, {.overwrite = false});
        require(cppwordkit::WordDocument::open(copy).text() == "updated", "new save not readable");
        requireNoStaging(directory.path);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

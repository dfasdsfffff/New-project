#pragma once
//! @file ZipArchive.hpp ZIP 归档的读取与写入，支持 OOXML 包格式

#include "cppwordkit/Types.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

//! ZIP 归档文件的封装，用于读取和写入 OOXML 规范中的 ZIP 包
//! 支持 Store 和 Deflate 两种压缩方式，不处理加密条目
class ZipArchive {
public:
    //! ZIP 中的单个条目：文件名 + 原始数据
    struct Entry {
        std::string name;
        std::vector<std::uint8_t> data;
    };

    static ZipArchive read(const Path& path);

    void write(const Path& path) const;

    [[nodiscard]] bool contains(const std::string& name) const;
    [[nodiscard]] std::string text(const std::string& name) const;
    [[nodiscard]] std::vector<std::uint8_t> bytes(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> names() const;

    void setText(const std::string& name, const std::string& content);
    void setBytes(const std::string& name, std::vector<std::uint8_t> content);

private:
    std::vector<Entry> entries_;                              //!< 所有条目数据，按添加顺序存储
    std::unordered_map<std::string, std::size_t> index_;      //!< 条目名到 entries_ 索引的哈希映射



    void addOrReplace(std::string name, std::vector<std::uint8_t> data);
};

} // namespace cppwordkit
//! @namespace cppwordkit OOXML Word 文档处理的顶级命名空间

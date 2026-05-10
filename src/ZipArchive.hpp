#pragma once

#include "cppwordkit/Types.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cppwordkit {

class ZipArchive {
public:
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
    std::vector<Entry> entries_;
    std::unordered_map<std::string, std::size_t> index_;

    void addOrReplace(std::string name, std::vector<std::uint8_t> data);
};

} // namespace cppwordkit

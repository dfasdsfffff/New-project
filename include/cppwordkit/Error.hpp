//! @file Error.hpp CppWordKit 异常类型层次定义
//!
//! 异常继承体系：
//!   std::runtime_error
//!     └── WordException                —— 通用 Word 操作异常（基类）
//!           ├── WordProcessingException —— WordprocessingML 处理异常
//!           │     └── XmlException       —— XML 解析/操作异常
//!           └── PackageException         —— OOXML 包（ZIP）异常

#pragma once

#include <stdexcept>
#include <string>

namespace cppwordkit {

/// @brief 通用 Word 操作异常，所有库异常的基类
class WordException : public std::runtime_error {
public:
    explicit WordException(const std::string& message)
        : std::runtime_error(message) {}
};

/// @brief WordprocessingML 文档处理异常（段落、文本、样式等）
class WordProcessingException : public WordException {
public:
    using WordException::WordException;
};

/// @brief OOXML 包（ZIP）读写异常
class PackageException : public WordException {
public:
    using WordException::WordException;
};

/// @brief XML 解析或 XPath 操作异常
class XmlException : public WordProcessingException {
public:
    using WordProcessingException::WordProcessingException;
};

} // namespace cppwordkit

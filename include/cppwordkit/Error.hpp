#pragma once

#include <stdexcept>
#include <string>

namespace cppwordkit {

class WordException : public std::runtime_error {
public:
    explicit WordException(const std::string& message)
        : std::runtime_error(message) {}
};

class WordProcessingException : public WordException {
public:
    using WordException::WordException;
};

class PackageException : public WordException {
public:
    using WordException::WordException;
};

class XmlException : public WordProcessingException {
public:
    using WordProcessingException::WordProcessingException;
};

} // namespace cppwordkit

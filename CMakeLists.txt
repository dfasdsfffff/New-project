cmake_minimum_required(VERSION 3.20)

project(CppWordKit
    VERSION 0.1.0
    DESCRIPTION "A lightweight C++20 library for reading and editing docx files"
    LANGUAGES CXX
)

option(CPPWORDKIT_BUILD_TESTS "Build CppWordKit tests" ON)
option(CPPWORDKIT_BUILD_EXAMPLES "Build CppWordKit examples" ON)
option(CPPWORDKIT_USE_LOCAL_DEPS "Use locally downloaded MinGW dependencies" OFF)

set(CPPWORDKIT_LOCAL_LIBXML2_ROOT "D:/libraries/MinGW/libxml2-v2.12.10"
    CACHE PATH "Local libxml2 installation root")
set(CPPWORDKIT_LOCAL_ZLIB_ROOT "D:/libraries/MinGW/zlib"
    CACHE PATH "Local zlib installation root")

if(CPPWORDKIT_USE_LOCAL_DEPS)
    list(PREPEND CMAKE_PREFIX_PATH
        "${CPPWORDKIT_LOCAL_LIBXML2_ROOT}"
        "${CPPWORDKIT_LOCAL_ZLIB_ROOT}"
    )
    set(ZLIB_ROOT "${CPPWORDKIT_LOCAL_ZLIB_ROOT}" CACHE PATH "zlib root" FORCE)
endif()

find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

if(CPPWORDKIT_USE_LOCAL_DEPS AND NOT TARGET LibXml2::LibXml2)
    find_path(LIBXML2_INCLUDE_DIR
        NAMES libxml/parser.h
        PATHS "${CPPWORDKIT_LOCAL_LIBXML2_ROOT}/include/libxml2"
        NO_DEFAULT_PATH
    )
    find_library(LIBXML2_LIBRARY
        NAMES xml2 libxml2
        PATHS "${CPPWORDKIT_LOCAL_LIBXML2_ROOT}/lib"
        NO_DEFAULT_PATH
    )
    if(NOT LIBXML2_INCLUDE_DIR OR NOT LIBXML2_LIBRARY)
        message(FATAL_ERROR "Could not find local libxml2 under ${CPPWORDKIT_LOCAL_LIBXML2_ROOT}")
    endif()
    add_library(LibXml2::LibXml2 UNKNOWN IMPORTED)
    set_target_properties(LibXml2::LibXml2 PROPERTIES
        IMPORTED_LOCATION "${LIBXML2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBXML2_INCLUDE_DIR}"
    )
endif()

add_library(CppWordKit
    src/OpenXmlPackage.cpp
    src/Template.cpp
    src/WordDocument.cpp
    src/XmlPart.cpp
    src/ZipArchive.cpp
)

add_library(CppWordKit::CppWordKit ALIAS CppWordKit)

target_compile_features(CppWordKit PUBLIC cxx_std_20)

target_include_directories(CppWordKit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(CppWordKit
    PUBLIC
        LibXml2::LibXml2
        ZLIB::ZLIB
)

if(MSVC)
    target_compile_options(CppWordKit PRIVATE /W4)
else()
    target_compile_options(CppWordKit PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)

install(TARGETS CppWordKit
    EXPORT CppWordKitTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT CppWordKitTargets
    FILE CppWordKitTargets.cmake
    NAMESPACE CppWordKit::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CppWordKit
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
    cmake/CppWordKitConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CppWordKitConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CppWordKit
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CppWordKitConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CppWordKitConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CppWordKitConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CppWordKit
)

if(CPPWORDKIT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CPPWORDKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

cmake_minimum_required(VERSION 3.20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(EDITOR_TARGET CppWordKitWordEditor)

set(EDITOR_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/DocumentController.cpp
    src/QtDocumentMapper.cpp
    src/ImageStore.cpp
    src/FormatToolbar.cpp
)

set(EDITOR_HEADERS
    src/MainWindow.hpp
    src/DocumentController.hpp
    src/QtDocumentMapper.hpp
    src/ImageStore.hpp
    src/FormatToolbar.hpp
)

add_executable(${EDITOR_TARGET} ${EDITOR_SOURCES} ${EDITOR_HEADERS})

target_compile_features(${EDITOR_TARGET} PRIVATE cxx_std_20)

target_include_directories(${EDITOR_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${EDITOR_TARGET}
    PRIVATE
        CppWordKit::CppWordKit
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
)

set_target_properties(${EDITOR_TARGET} PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(MSVC)
    target_compile_options(${EDITOR_TARGET} PRIVATE /W4)
else()
    target_compile_options(${EDITOR_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
endif()

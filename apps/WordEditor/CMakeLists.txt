cmake_minimum_required(VERSION 3.20)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(EDITOR_TARGET CppWordKitWordEditor)

# ── SARibbon 库（合辑版） ──
add_library(SARibbon STATIC
    ${CPPWORDKIT_3THLIB_DIR}/SARibbon/SARibbon.cpp
)
target_compile_features(SARibbon PRIVATE cxx_std_17)
target_include_directories(SARibbon PUBLIC ${CPPWORDKIT_3THLIB_DIR}/SARibbon)
target_link_libraries(SARibbon PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

set(EDITOR_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/DocumentController.cpp
    src/QtDocumentMapper.cpp
    src/ImageStore.cpp
    src/PageView.cpp
    src/PageSetupDialog.cpp
)

set(EDITOR_HEADERS
    src/MainWindow.hpp
    src/DocumentController.hpp
    src/QtDocumentMapper.hpp
    src/ImageStore.hpp
    src/PageView.hpp
    src/PageSetupDialog.hpp
)

add_executable(${EDITOR_TARGET} ${EDITOR_SOURCES} ${EDITOR_HEADERS})

target_compile_features(${EDITOR_TARGET} PRIVATE cxx_std_20)

target_include_directories(${EDITOR_TARGET}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CPPWORDKIT_3THLIB_DIR}/SARibbon
)

target_link_libraries(${EDITOR_TARGET}
    PRIVATE
        CppWordKit::CppWordKit
        SARibbon
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::PrintSupport
)

if(CPPWORDKIT_BUILD_TESTS)
    # 复用真实窗口实现，以无界面模式回归保存和退出流程。
    set(EDITOR_TEST_SOURCES ${EDITOR_SOURCES})
    list(REMOVE_ITEM EDITOR_TEST_SOURCES src/main.cpp)
    add_executable(cppwordkit_editor_tests
        ../../tests/EditorLifecycleTests.cpp ${EDITOR_TEST_SOURCES} ${EDITOR_HEADERS})
    target_include_directories(cppwordkit_editor_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(cppwordkit_editor_tests PRIVATE
        CppWordKit::CppWordKit SARibbon Qt6::Widgets Qt6::PrintSupport)
    add_test(NAME cppwordkit_editor_tests COMMAND cppwordkit_editor_tests)
    get_filename_component(_editor_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
    get_filename_component(_editor_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_tests_properties(cppwordkit_editor_tests PROPERTIES
        TIMEOUT 30
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_editor_qt_prefix}/plugins"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_editor_qt_prefix}/bin;PATH=path_list_prepend:${_editor_compiler_bin}")
    if(CPPWORDKIT_USE_LOCAL_DEPS)
        set_property(TEST cppwordkit_editor_tests APPEND PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CPPWORDKIT_LOCAL_LIBXML2_ROOT}/bin"
            "PATH=path_list_prepend:${CPPWORDKIT_LOCAL_ZLIB_ROOT}/bin")
    endif()
endif()

set_target_properties(${EDITOR_TARGET} PROPERTIES
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out/$<CONFIG>"
    DEBUG_POSTFIX "_d"
)

if(MSVC)
    target_compile_options(${EDITOR_TARGET} PRIVATE /W4)
else()
    target_compile_options(${EDITOR_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
endif()

get_filename_component(_qt6_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
set(_qt6_bin_dir "${_qt6_prefix}/bin")
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt6_bin_dir}" REQUIRED)

set(_out_dir "${CMAKE_SOURCE_DIR}/out/$<CONFIG>")

add_custom_command(TARGET ${EDITOR_TARGET} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_out_dir}"
    COMMAND "${WINDEPLOYQT_EXECUTABLE}"
        --compiler-runtime
        "$<TARGET_FILE:${EDITOR_TARGET}>"
    COMMENT "Deploying Qt dependencies..."
)

if(CPPWORDKIT_USE_LOCAL_DEPS)
    add_custom_command(TARGET ${EDITOR_TARGET} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CPPWORDKIT_LOCAL_LIBXML2_ROOT}/bin/libxml2.dll"
            "${_out_dir}/libxml2.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CPPWORDKIT_LOCAL_ZLIB_ROOT}/bin/libzlib.dll"
            "${_out_dir}/libzlib.dll"
        COMMENT "Copying dependency DLLs..."
    )
endif()

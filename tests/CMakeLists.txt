function(cppwordkit_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE CppWordKit::CppWordKit)
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ${name} COMMAND ${name})

    if(CPPWORDKIT_USE_LOCAL_DEPS)
        get_filename_component(CPPWORDKIT_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${CPPWORDKIT_LOCAL_LIBXML2_ROOT}/bin;PATH=path_list_prepend:${CPPWORDKIT_LOCAL_ZLIB_ROOT}/bin;PATH=path_list_prepend:${CPPWORDKIT_COMPILER_BIN}"
        )
    endif()
endfunction()

cppwordkit_add_test(cppwordkit_xml_tests XmlPartTests.cpp)
cppwordkit_add_test(cppwordkit_word_tests WordDocumentTests.cpp)

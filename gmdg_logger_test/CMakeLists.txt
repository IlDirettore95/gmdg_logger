# test application
add_executable(gmdg_logger_test)

target_sources(gmdg_logger_test
    PRIVATE
        src/main.cpp
)

target_include_directories(gmdg_logger_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../gmdg_logger/include
)

target_link_libraries(gmdg_logger_test 
    PRIVATE
        gmdg_logger
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(gmdg_logger_gui
        PRIVATE
            stdc++exp
    )
endif()

install(TARGETS gmdg_logger_test
    RUNTIME DESTINATION ${PROJECT_NAME}/bin
)

# CTests
add_executable(gmdg_logger_tests)

target_sources(gmdg_logger_tests
    PRIVATE
        src/tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../gmdg_logger_gui/src/mvc/view/table_layout.cpp
)

target_include_directories(gmdg_logger_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../gmdg_logger_gui/src
)

target_link_libraries(gmdg_logger_tests
    PRIVATE
        gmdg_logger
        gmdg_asserting
)

if (GMDG_ASSERT_ENABLE)
    target_compile_definitions(gmdg_logger_tests PRIVATE GMDG_ASSERT_ENABLE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(gmdg_logger_gui
        PRIVATE
            stdc++exp
    )
endif()

add_test(NAME gmdg_logger_tests
         COMMAND gmdg_logger_tests)
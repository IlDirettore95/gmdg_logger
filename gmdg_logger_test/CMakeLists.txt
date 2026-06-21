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
        stdc++exp
)

install(TARGETS gmdg_logger_test
    RUNTIME DESTINATION ${PROJECT_NAME}/bin
)

# CTests
add_executable(gmdg_logger_tests)

target_sources(gmdg_logger_tests
    PRIVATE
        src/tests.cpp
)

target_link_libraries(gmdg_logger_tests
    PRIVATE 
        gmdg_logger
        stdc++exp
)

add_test(NAME gmdg_logger_tests
         COMMAND gmdg_logger_tests)
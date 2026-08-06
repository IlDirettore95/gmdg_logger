# benchmark application
add_executable(gmdg_logger_bench)

target_sources(gmdg_logger_bench
    PRIVATE
        src/main.cpp
        src/backends/backend.hpp
        src/backends/async_backend.hpp
        src/backends/async_backend.cpp
        src/backends/sync_backend.hpp
        src/backends/sync_backend.cpp
        src/backends/fprintf_backend.hpp
        src/backends/fprintf_backend.cpp
)

target_include_directories(gmdg_logger_bench
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../gmdg_logger/include
)

target_link_libraries(gmdg_logger_bench
    PRIVATE
        gmdg_logger
        gmdg_asserting
)

if (GMDG_ASSERT_ENABLE)
    target_compile_definitions(gmdg_logger_bench PRIVATE GMDG_ASSERT_ENABLE)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(gmdg_logger_bench
        PRIVATE
            stdc++exp
    )
endif()

install(TARGETS gmdg_logger_bench
    RUNTIME DESTINATION ${PROJECT_NAME}/bin
)

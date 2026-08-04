cmake_minimum_required(VERSION 4.1.1)

project(gmdg_logger LANGUAGES CXX VERSION 0.1.0)

#Compile options
if (MSVC)

    add_compile_options(
        /W4
        /EHsc
    )

else()

    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )

    add_link_options(
        -static
        -static-libgcc
        -static-libstdc++
    )

endif()

enable_testing()

option(GMDG_ASSERT_ENABLE "Enable GMDG_ASSERT/GMDG_ASSERT_WITH_MESSAGE checks" OFF)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../gmdg_asserting" "${CMAKE_BINARY_DIR}/gmdg_asserting")

add_subdirectory("gmdg_logger")
add_subdirectory("gmdg_logger_gui")
add_subdirectory("gmdg_logger_test")
add_subdirectory("gmdg_logger_bench")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")

include(CPack)
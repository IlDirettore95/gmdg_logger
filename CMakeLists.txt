cmake_minimum_required(VERSION 4.1.1)

project(gmdg_logger LANGUAGES CXX VERSION 0.1.0)

enable_testing()

add_subdirectory("gmdg_logger")
add_subdirectory("gmdg_logger_gui")
add_subdirectory("gmdg_logger_test")

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_INSTALL_PREFIX}")

include(CPack)
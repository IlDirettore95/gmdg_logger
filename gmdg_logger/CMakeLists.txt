# static

add_library(gmdg_logger STATIC)

target_sources(gmdg_logger
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
        include/gmdg_logger.h 
        include/gmdg_logger.hpp  
    PRIVATE
        src/gmdg_logger.cpp
)

target_include_directories(gmdg_logger
    PUBLIC
        include
)

target_link_libraries(gmdg_logger
    PRIVATE
        gmdg_asserting
)

if (GMDG_ASSERT_ENABLE)
    target_compile_definitions(gmdg_logger PRIVATE GMDG_ASSERT_ENABLE)
endif()

install(
    TARGETS gmdg_logger
    FILE_SET HEADERS
    DESTINATION ${PROJECT_NAME}/include

    RUNTIME DESTINATION ${PROJECT_NAME}/lib
    LIBRARY DESTINATION ${PROJECT_NAME}/lib
    ARCHIVE DESTINATION ${PROJECT_NAME}/lib
)

# dll
add_library(gmdg_logger_c SHARED)

target_sources(gmdg_logger_c
    PRIVATE
        include/gmdg_logger.h
    PRIVATE
        src/gmdg_logger.cpp
)

target_include_directories(gmdg_logger_c
    PRIVATE
        include
)

target_link_libraries(gmdg_logger_c
    PRIVATE
        gmdg_asserting
)

if (GMDG_ASSERT_ENABLE)
    target_compile_definitions(gmdg_logger_c PRIVATE GMDG_ASSERT_ENABLE)
endif()

install(
    TARGETS gmdg_logger_c
        RUNTIME DESTINATION ${PROJECT_NAME}/bin
        LIBRARY DESTINATION ${PROJECT_NAME}/lib
        ARCHIVE DESTINATION ${PROJECT_NAME}/lib
)
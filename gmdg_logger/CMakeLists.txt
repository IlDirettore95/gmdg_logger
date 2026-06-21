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
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

install(
    TARGETS gmdg_logger
    FILE_SET HEADERS
    DESTINATION ${PROJECT_NAME}/include
)
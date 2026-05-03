cmake_minimum_required(VERSION 4.1.1)

get_filename_component(PROJECT_FOLDER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_FOLDER_NAME} LANGUAGES CXX)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        main.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        $ENV{VENDORS_DIR}/glfw/include
)

target_link_directories(${PROJECT_NAME} 
    PRIVATE
        $ENV{VENDORS_DIR}/glfw/lib
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE
        opengl32
        glfw3
        stdc++exp
)
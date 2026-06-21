#add_executable(gmdg_logger_gui WIN32) # WIN32 to compile it as a win app (no terminal)
add_executable(gmdg_logger_gui)

target_sources(gmdg_logger_gui
    PRIVATE
        src/mvc/controller/actions/action.hpp
        src/mvc/controller/actions/shutdown_action.hpp
        src/mvc/controller/actions/shutdown_action.cpp
        src/mvc/controller/control_handler.hpp
        src/mvc/controller/control_handler.cpp

        src/mvc/model/model_handler.hpp
        src/mvc/model/model_handler.cpp

        src/mvc/view/view_handler.hpp
        src/mvc/view/view_handler.cpp

        src/application.hpp
        src/application.cpp
        src/main.cpp

        #utilities
        ${CMAKE_BINARY_DIR}/generated/version.hpp

    PRIVATE
        $ENV{VENDORS_DIR}/imgui/imgui.cpp
        $ENV{VENDORS_DIR}/imgui/imgui_draw.cpp
        $ENV{VENDORS_DIR}/imgui/imgui_tables.cpp
        $ENV{VENDORS_DIR}/imgui/imgui_widgets.cpp
        $ENV{VENDORS_DIR}/imgui/backends/imgui_impl_glfw.cpp
        $ENV{VENDORS_DIR}/imgui/backends/imgui_impl_opengl3.cpp
        $ENV{VENDORS_DIR}/imgui/imgui_demo.cpp
)

target_include_directories(gmdg_logger_gui
    PRIVATE
        $ENV{VENDORS_DIR}/glfw/include
        $ENV{VENDORS_DIR}/imgui
        $ENV{VENDORS_DIR}/imgui/backends
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
)

target_link_directories(gmdg_logger_gui
    PRIVATE
        $ENV{VENDORS_DIR}/glfw/lib
)

target_link_libraries(gmdg_logger_gui
    PRIVATE
        opengl32
        glfw3
        stdc++exp
    PRIVATE
        gmdg_logger
)

target_precompile_headers(gmdg_logger_gui
    PRIVATE 
        src/pch.h
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.hpp.in"
    "${CMAKE_BINARY_DIR}/generated/version.hpp"
    @ONLY
)

install(
    TARGETS gmdg_logger_gui
    RUNTIME DESTINATION ${PROJECT_NAME}/bin
)
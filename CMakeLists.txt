cmake_minimum_required(VERSION 3.15)
project(yuvdiff_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_GUI "Build Qt6-based GUI application" ON)
option(ENABLE_AVX2 "Enable AVX2 SIMD optimizations" ON)
option(STATIC_CLI "Statically link CLI for portable server deployment" OFF)

if (ENABLE_AVX2 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(-mavx2 -mfma)
    endif()
endif()

# Core Library (No Qt dependency)
add_library(yuvdiff_core STATIC
    cpp/src/formats.cpp
    cpp/src/parser.cpp
    cpp/src/diff.cpp
    cpp/src/metrics.cpp
    cpp/src/renderer.cpp
    cpp/src/tools.cpp
)

target_include_directories(yuvdiff_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
)

# CLI Executable
add_executable(yuvdiff-cli cpp/src/main_cli.cpp)
target_link_libraries(yuvdiff-cli PRIVATE yuvdiff_core)

if (STATIC_CLI)
    target_link_options(yuvdiff-cli PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Tests
enable_testing()
add_executable(yuvdiff_test
    cpp/tests/test_main.cpp
    cpp/tests/test_formats.cpp
    cpp/tests/test_parser.cpp
    cpp/tests/test_diff.cpp
    cpp/tests/test_metrics.cpp
    cpp/tests/test_renderer.cpp
    cpp/tests/test_tools.cpp
)
target_include_directories(yuvdiff_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests
)
target_link_libraries(yuvdiff_test PRIVATE yuvdiff_core)
add_test(NAME AllTests COMMAND yuvdiff_test)

# GUI (Qt6 Widgets & OpenGL)
if (BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets OpenGL OpenGLWidgets QUIET)
    if (Qt6_FOUND AND Qt6OpenGLWidgets_FOUND)
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)
        add_executable(yuvdiff-gui
            cpp/include/yuvdiff/gui.hpp
            cpp/include/yuvdiff/gl_widget.hpp
            cpp/include/yuvdiff/worker.hpp
            cpp/include/yuvdiff/cache.hpp
            cpp/include/yuvdiff/theme.hpp
            cpp/src/gui.cpp
            cpp/src/gl_widget.cpp
            cpp/src/worker.cpp
            cpp/src/theme.cpp
            cpp/src/main_gui.cpp
        )
        target_link_libraries(yuvdiff-gui PRIVATE yuvdiff_core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
        message(STATUS "Qt6 OpenGLWidgets found: yuvdiff-gui with GPU acceleration will be built.")
    else()
        message(STATUS "Qt6 or Qt6OpenGLWidgets not found: yuvdiff-gui skipped.")
    endif()
else()
    message(STATUS "BUILD_GUI is OFF: skipping GUI build (Headless CLI mode).")
endif()

# Installation Rules
include(GNUInstallDirs)

install(TARGETS yuvdiff-cli yuvdiff_core
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY cpp/include/yuvdiff
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

if (TARGET yuvdiff-gui)
    install(TARGETS yuvdiff-gui
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()


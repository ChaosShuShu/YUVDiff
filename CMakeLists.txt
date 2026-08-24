cmake_minimum_required(VERSION 3.15)
project(yuvdiff_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Core Library (No Qt dependency)
add_library(yuvdiff_core STATIC
    cpp/src/formats.cpp
    cpp/src/parser.cpp
    cpp/src/diff.cpp
    cpp/src/metrics.cpp
    cpp/src/renderer.cpp
)

target_include_directories(yuvdiff_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/include
)

# CLI Executable
add_executable(yuvdiff-cli cpp/src/main_cli.cpp)
target_link_libraries(yuvdiff-cli PRIVATE yuvdiff_core)

# Tests
enable_testing()
add_executable(yuvdiff_test
    cpp/tests/test_main.cpp
    cpp/tests/test_formats.cpp
    cpp/tests/test_parser.cpp
    cpp/tests/test_diff.cpp
    cpp/tests/test_metrics.cpp
    cpp/tests/test_renderer.cpp
)
target_include_directories(yuvdiff_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/tests
)
target_link_libraries(yuvdiff_test PRIVATE yuvdiff_core)
add_test(NAME AllTests COMMAND yuvdiff_test)

# GUI (Qt6 Widgets)
find_package(Qt6 COMPONENTS Widgets QUIET)
if (Qt6_FOUND)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)
    add_executable(yuvdiff-gui
        cpp/include/yuvdiff/gui.hpp
        cpp/src/gui.cpp
        cpp/src/main_gui.cpp
    )
    target_link_libraries(yuvdiff-gui PRIVATE yuvdiff_core Qt6::Widgets)
    message(STATUS "Qt6 found: yuvdiff-gui will be built.")
else()
    message(WARNING "Qt6 not found: yuvdiff-gui skipped.")
endif()

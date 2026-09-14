cmake_minimum_required(VERSION 3.21)

project(PlanVizQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if (WIN32)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(planviz-qt
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/PlanData.cpp
    src/PlanData.h
    src/MapView.cpp
    src/MapView.h
)

target_link_libraries(planviz-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

qt_add_executable(highway-editor-qt
    src/highway_editor_main.cpp
    src/HighwayEditorData.cpp
    src/HighwayEditorData.h
    src/HighwayEditorView.cpp
    src/HighwayEditorView.h
    src/HighwayEditorWindow.cpp
    src/HighwayEditorWindow.h
)

target_link_libraries(highway-editor-qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if (MSVC)
    target_compile_options(planviz-qt PRIVATE /W4 /permissive-)
    target_compile_options(highway-editor-qt PRIVATE /W4 /permissive-)
else()
    target_compile_options(planviz-qt PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(highway-editor-qt PRIVATE -Wall -Wextra -Wpedantic)
endif()

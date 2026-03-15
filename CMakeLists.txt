cmake_minimum_required(VERSION 3.16)
project(plotjuggler-signalview VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(PJ_INSTALL_DIR "" CACHE PATH "PlotJuggler install prefix")
if(NOT PJ_INSTALL_DIR)
    # Default: look for it relative to CMAKE_PREFIX_PATH
    find_path(PJ_INSTALL_DIR
        NAMES include/PlotJuggler/plotdata.h
        PATHS ${CMAKE_PREFIX_PATH}
        NO_DEFAULT_PATH
    )
endif()

if(NOT PJ_INSTALL_DIR)
    message(FATAL_ERROR "Set PJ_INSTALL_DIR or CMAKE_PREFIX_PATH to the PlotJuggler install prefix")
endif()

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Concurrent Xml Test)
find_package(fmt REQUIRED)

add_library(SignalViewPlugin SHARED
    src/signal_view_plugin.h
    src/signal_view_plugin.cpp
    src/signal_view_widget.h
    src/signal_view_widget.cpp
    src/plot_canvas.h
    src/plot_canvas.cpp
    src/y_axis_panel.h
    src/y_axis_panel.cpp
    src/overlay_manager.h
    src/overlay_manager.cpp
    src/data_sets_panel.h
    src/data_sets_panel.cpp
)

target_include_directories(SignalViewPlugin PRIVATE
    ${PJ_INSTALL_DIR}/include
)

find_library(PJ_BASE_LIB
    NAMES plotjuggler_base
    PATHS ${PJ_INSTALL_DIR}/lib
    NO_DEFAULT_PATH
    REQUIRED
)

target_link_libraries(SignalViewPlugin PRIVATE
    ${PJ_BASE_LIB}
    fmt::fmt
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    Qt5::Concurrent
)

target_compile_definitions(SignalViewPlugin PRIVATE
    QT_PLUGIN
    PLUGIN_VERSION="${PROJECT_VERSION}"
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
)

install(TARGETS SignalViewPlugin
    DESTINATION ${PJ_INSTALL_DIR}/lib/plotjuggler/plugins
)

option(BUILD_TESTING "Build unit tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

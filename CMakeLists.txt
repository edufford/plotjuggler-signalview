cmake_minimum_required(VERSION 3.16)
project(plotjuggler-signalview LANGUAGES CXX)

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

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Svg Concurrent Xml)
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
)

target_include_directories(SignalViewPlugin PRIVATE
    ${PJ_INSTALL_DIR}/include
)

target_link_libraries(SignalViewPlugin PRIVATE
    ${PJ_INSTALL_DIR}/lib/libplotjuggler_base.a
    fmt::fmt
    Qt5::Core
    Qt5::Widgets
    Qt5::Xml
    Qt5::Svg
    Qt5::Concurrent
)

target_compile_definitions(SignalViewPlugin PRIVATE QT_PLUGIN)

install(TARGETS SignalViewPlugin
    DESTINATION ${PJ_INSTALL_DIR}/lib/plotjuggler/plugins
)

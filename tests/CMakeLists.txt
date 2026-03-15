include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_compile_definitions($<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>)

set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${PJ_INSTALL_DIR}/include
)
set(TEST_LINK_LIBS
    ${PJ_BASE_LIB}
    fmt::fmt
    Qt5::Core
    Qt5::Widgets
    GTest::gtest_main
)

# Test 1: Name parsing (pure string logic)
add_executable(test_name_parsing
    test_name_parsing.cpp
    ${CMAKE_SOURCE_DIR}/src/overlay_manager.cpp
)
target_include_directories(test_name_parsing PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_name_parsing PRIVATE ${TEST_LINK_LIBS})
add_test(NAME NameParsing COMMAND test_name_parsing)

# Test 2: OverlayManager instance methods
add_executable(test_overlay_manager
    test_overlay_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/overlay_manager.cpp
)
target_include_directories(test_overlay_manager PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_overlay_manager PRIVATE ${TEST_LINK_LIBS})
target_compile_definitions(test_overlay_manager PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
)
add_test(NAME OverlayManager COMMAND test_overlay_manager)

# Test 3: AxisLayout namespace (band geometry)
add_executable(test_axis_layout
    test_axis_layout.cpp
)
target_include_directories(test_axis_layout PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_axis_layout PRIVATE ${TEST_LINK_LIBS})
add_test(NAME AxisLayout COMMAND test_axis_layout)

# Test 4: CSV parsing and detectDelimiter
add_executable(test_csv_parser
    test_csv_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/overlay_manager.cpp
)
target_include_directories(test_csv_parser PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_csv_parser PRIVATE ${TEST_LINK_LIBS})
target_compile_definitions(test_csv_parser PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
)
add_test(NAME CsvParser COMMAND test_csv_parser)

file(COPY fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# --- UI interaction tests (QTest + gtest, custom main for QApplication) ---

set(UI_WIDGET_SOURCES
    ${CMAKE_SOURCE_DIR}/src/plot_canvas.cpp
    ${CMAKE_SOURCE_DIR}/src/y_axis_panel.cpp
    ${CMAKE_SOURCE_DIR}/src/overlay_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/data_sets_panel.cpp
)
set(UI_TEST_LINK_LIBS
    ${PJ_BASE_LIB}
    fmt::fmt
    Qt5::Core
    Qt5::Widgets
    Qt5::Test
    GTest::gtest
)

# Test 5: PlotCanvas mouse interaction
add_executable(test_plot_canvas_ui
    test_plot_canvas_ui.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_plot_canvas_ui PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_plot_canvas_ui PRIVATE ${UI_TEST_LINK_LIBS})
add_test(NAME PlotCanvasUI COMMAND test_plot_canvas_ui)

# Test 6: YAxisBarColumn mouse interaction
add_executable(test_yaxis_bar_ui
    test_yaxis_bar_ui.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_yaxis_bar_ui PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_yaxis_bar_ui PRIVATE ${UI_TEST_LINK_LIBS})
add_test(NAME YAxisBarUI COMMAND test_yaxis_bar_ui)

# Test 7: SignalNameColumn mouse interaction
add_executable(test_signal_name_ui
    test_signal_name_ui.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_signal_name_ui PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_signal_name_ui PRIVATE ${UI_TEST_LINK_LIBS})
add_test(NAME SignalNameUI COMMAND test_signal_name_ui)

# Test 8: PlotCanvas::buildSignalPath (full-res and downsampled path building)
add_executable(test_signal_path
    test_signal_path.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_signal_path PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_signal_path PRIVATE ${UI_TEST_LINK_LIBS})
add_test(NAME SignalPath COMMAND test_signal_path)

# Test 9: Dark/Light theme (mirrorLightness math, PlotCanvas, YAxisBar)
add_executable(test_theme
    test_theme.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_theme PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_theme PRIVATE ${UI_TEST_LINK_LIBS})
add_test(NAME Theme COMMAND test_theme)

# Test 10: DataSetsPanel context menu
add_executable(test_data_sets_panel_ui
    test_data_sets_panel_ui.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_data_sets_panel_ui PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_data_sets_panel_ui PRIVATE ${UI_TEST_LINK_LIBS})
target_compile_definitions(test_data_sets_panel_ui PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/fixtures"
)
add_test(NAME DataSetsPanelUI COMMAND test_data_sets_panel_ui)

# Test 11: Auto Scale margin calculation
add_executable(test_autoscale
    test_autoscale.cpp
    ${CMAKE_SOURCE_DIR}/src/signal_view_widget.cpp
    ${UI_WIDGET_SOURCES}
)
target_include_directories(test_autoscale PRIVATE ${TEST_INCLUDE_DIRS})
target_link_libraries(test_autoscale PRIVATE ${UI_TEST_LINK_LIBS})
target_compile_definitions(test_autoscale PRIVATE
    PLUGIN_VERSION="${PROJECT_VERSION}"
)
add_test(NAME AutoScale COMMAND test_autoscale)

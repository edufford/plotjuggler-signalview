include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${PJ_INSTALL_DIR}/include
)
set(TEST_LINK_LIBS
    ${PJ_INSTALL_DIR}/lib/libplotjuggler_base.a
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

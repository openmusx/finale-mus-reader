include(FetchContent)

set(CATCH_BUILD_TESTING OFF CACHE BOOL "Do not build Catch2 tests" FORCE)
set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "Do not build Catch2 examples" FORCE)
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "Do not install Catch2 documentation" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "Do not install Catch2 extras" FORCE)
FetchContent_Declare(Catch2
    URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.15.3.tar.gz
    URL_HASH SHA256=b0299ae552918220a7a6e21e7de5b714777f4e8c883fb70c4bb23fe01df8c6e3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(Catch2)

# Catch2 is intentionally consumed as a normal library, but its implementation is split
# across many translation units. Unity compilation reduces the cost of building those
# upstream files just as it does for this project's test target below.
#
# This is a deliberate choice about one dependency's build time. Its companion target has no
# such reason, so it follows the general rule for code this repository does not own.
set_target_properties(Catch2 PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 15)
finale_mus_reader_keep_out_of_unity(Catch2WithMain)

add_executable(finale_mus_reader_tests
    classes/common/legacy_record_tests.cpp
    classes/common/mapping_framework_tests.cpp
    classes/details/articulation_assign_tests.cpp
    classes/details/baseline_tests.cpp
    classes/details/measure_number_individual_positioning_tests.cpp
    classes/details/center_shape_tests.cpp
    classes/details/chord_assign_tests.cpp
    classes/details/gframe_hold_tests.cpp
    classes/details/independent_staff_details_tests.cpp
    classes/details/lyric_details_tests.cpp
    classes/details/measure_graphic_assign_tests.cpp
    classes/details/measure_text_assign_tests.cpp
    classes/details/notehead_details_tests.cpp
    classes/details/percussion_note_code_tests.cpp
    classes/details/shape_note_tests.cpp
    classes/details/smart_shape_entry_assign_tests.cpp
    classes/details/special_tools_tests.cpp
    classes/details/staff_group_tests.cpp
    classes/details/staff_size_tests.cpp
    classes/details/tie_alter_tests.cpp
    classes/details/tuplet_def_tests.cpp
    classes/entries/entry_tests.cpp
    classes/options/accidental_options_tests.cpp
    classes/options/alternate_notation_options_tests.cpp
    classes/options/augmentation_dot_options_tests.cpp
    classes/options/barline_options_tests.cpp
    classes/options/beam_options_tests.cpp
    classes/options/chord_options_tests.cpp
    classes/options/clef_options_tests.cpp
    classes/options/flag_options_tests.cpp
    classes/options/font_options_tests.cpp
    classes/options/grace_note_options_tests.cpp
    classes/options/key_signature_options_tests.cpp
    classes/options/line_curve_options_tests.cpp
    classes/options/lyric_options_tests.cpp
    classes/options/misc_options_tests.cpp
    classes/options/multimeasure_rest_options_tests.cpp
    classes/options/music_symbol_options_tests.cpp
    classes/options/note_rest_options_tests.cpp
    classes/options/page_format_options_tests.cpp
    classes/options/piano_brace_bracket_options_tests.cpp
    classes/options/repeat_options_tests.cpp
    classes/options/smart_shape_options_tests.cpp
    classes/options/staff_options_tests.cpp
    classes/options/stem_options_tests.cpp
    classes/options/text_options_tests.cpp
    classes/options/tie_options_tests.cpp
    classes/options/time_signature_options_tests.cpp
    classes/options/tuplet_options_tests.cpp
    classes/others/articulation_def_tests.cpp
    classes/others/beat_chart_element_tests.cpp
    classes/others/chord_suffix_tests.cpp
    classes/others/clef_list_tests.cpp
    classes/others/custom_keys_tests.cpp
    classes/others/drum_staff_tests.cpp
    classes/others/file_path_tests.cpp
    classes/others/frame_tests.cpp
    classes/others/fretboard_tests.cpp
    classes/others/graphic_assignments_tests.cpp
    classes/others/individual_positioning_tests.cpp
    classes/others/marking_category_tests.cpp
    classes/others/measure_expr_assign_tests.cpp
    classes/others/measure_number_region_tests.cpp
    classes/others/measure_tests.cpp
    classes/others/multimeasure_rest_tests.cpp
    classes/others/multistaff_instrument_group_tests.cpp
    classes/others/name_position_tests.cpp
    classes/others/page_tests.cpp
    classes/others/page_text_assign_tests.cpp
    classes/others/part_definitions_tests.cpp
    classes/others/part_globals_tests.cpp
    classes/others/part_voicing_tests.cpp
    classes/others/percussion_note_info_tests.cpp
    classes/others/playback_tests.cpp
    classes/others/repeat_back_tests.cpp
    classes/others/repeat_ending_start_tests.cpp
    classes/others/shape_definitions_tests.cpp
    classes/others/shape_expression_defs_tests.cpp
    classes/others/smart_shape_custom_lines_tests.cpp
    classes/others/smart_shape_measure_assign_tests.cpp
    classes/others/smart_shape_tests.cpp
    classes/others/split_measure_tests.cpp
    classes/others/staff_list_tests.cpp
    classes/others/staff_style_assign_tests.cpp
    classes/others/staff_system_tests.cpp
    classes/others/staff_tests.cpp
    classes/others/staff_used_tests.cpp
    classes/others/system_lock_tests.cpp
    classes/others/tempo_change_tests.cpp
    classes/others/text_blocks_tests.cpp
    classes/others/text_expression_defs_tests.cpp
    classes/others/text_repeat_assign_tests.cpp
    classes/others/text_repeat_def_tests.cpp
    classes/others/time_composite_tests.cpp
    classes/texts/text_pool_tests.cpp
    reader_tests.cpp
    text_encoding_tests.cpp)
finale_mus_reader_enable_unity(finale_mus_reader_tests)

# Test support interface: the shared class-test helpers, the fixture root, and the test
# framework. The class import framework is internal, so tests reach into the private source
# tree (through finale_mus_reader::internal) rather than driving every case through a
# synthetic MUS file. A parent project's own test target can link this to reuse the helpers
# and the tracked fixtures.
add_library(finale_mus_reader_test_support INTERFACE)
add_library(finale_mus_reader::test_support ALIAS finale_mus_reader_test_support)
target_include_directories(finale_mus_reader_test_support INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/classes/common")
target_compile_definitions(finale_mus_reader_test_support INTERFACE
    FINALE_MUS_READER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(finale_mus_reader_test_support INTERFACE
    finale_mus_reader::internal
    Catch2::Catch2WithMain)

target_link_libraries(finale_mus_reader_tests PRIVATE finale_mus_reader::test_support)

if(MSVC)
    target_compile_options(finale_mus_reader_tests PRIVATE /W4 /WX)
else()
    target_compile_options(finale_mus_reader_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Cached so a parent project's own Catch2 test target can find the discovery module too.
set(FINALE_MUS_READER_CATCH2_EXTRAS_DIR "${catch2_SOURCE_DIR}/extras" CACHE INTERNAL
    "Catch2 CMake integration modules")
list(APPEND CMAKE_MODULE_PATH "${FINALE_MUS_READER_CATCH2_EXTRAS_DIR}")
include(Catch)
catch_discover_tests(finale_mus_reader_tests)

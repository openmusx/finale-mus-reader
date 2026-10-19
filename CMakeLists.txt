cmake_minimum_required(VERSION 3.20)

project(finale_mus_reader VERSION 0.1.0 LANGUAGES C CXX)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
endif()
if(CMAKE_CXX_STANDARD LESS 20)
    message(FATAL_ERROR "Legacy Finale MUS Reader requires at least C++20")
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # musxdom's template-heavy factories can exceed the default COFF section
    # limit in any translation unit that instantiates them. Apply this to the
    # entire directory tree so no MSVC object is built without it.
    add_compile_options(/bigobj)
    # Sources carry UTF-8 string literals and no byte-order mark: the legacy text
    # tests compare against real decoded names such as "Célino" and "ヒラギノ明朝".
    # Without this MSVC reads them in the machine's active code page and silently
    # produces different bytes, so this is a directory-wide invariant too.
    add_compile_options(/utf-8)
endif()

set(FINALE_MUS_READER_DEFAULT_TESTING OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND NOT EMSCRIPTEN)
    set(FINALE_MUS_READER_DEFAULT_TESTING ON)
endif()
option(FINALE_MUS_READER_BUILD_TESTING "Build Legacy Finale MUS Reader tests"
    ${FINALE_MUS_READER_DEFAULT_TESTING})
unset(FINALE_MUS_READER_DEFAULT_TESTING)
option(FINALE_MUS_READER_BUILD_REPORTING_TESTING
    "Build the standalone reporting test in either instrumentation mode"
    ${FINALE_MUS_READER_BUILD_TESTING})
option(FINALE_MUS_READER_USE_SYSTEM_ZLIB
    "Use an installed zlib when a parent project has not already provided one" OFF)
set(FINALE_MUS_READER_INSTRUMENTATION "AUTO" CACHE STRING
    "Reader diagnostics and timing instrumentation: AUTO, ON, or OFF")
set_property(CACHE FINALE_MUS_READER_INSTRUMENTATION PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${FINALE_MUS_READER_INSTRUMENTATION}"
    FINALE_MUS_READER_INSTRUMENTATION)
if(NOT FINALE_MUS_READER_INSTRUMENTATION MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR
        "FINALE_MUS_READER_INSTRUMENTATION must be AUTO, ON, or OFF")
endif()
set(FINALE_MUS_READER_MUSXDOM_SOURCE_DIR "" CACHE PATH
    "Use an existing musxdom source tree instead of fetching the pinned revision")

include(cmake/ReaderDependencies.cmake)

# Unity compilation is this project's normal build, not a dependency's. Each project-owned
# target opts in explicitly below so the setting cannot impose an amalgamation policy on code
# this repository does not own. zlib is the standing example -- its inftrees.h is included by
# several of its sources and is not idempotent -- but the rule is about ownership rather than
# about zlib.
#
# Stated once here and applied at each dependency, so a target added later is opted out by
# calling this rather than by remembering a policy written somewhere else.
function(finale_mus_reader_keep_out_of_unity)
    foreach(finale_mus_reader_foreign_target IN LISTS ARGN)
        if(TARGET ${finale_mus_reader_foreign_target})
            set_target_properties(${finale_mus_reader_foreign_target} PROPERTIES
                UNITY_BUILD OFF)
        endif()
    endforeach()
endfunction()

function(finale_mus_reader_enable_unity target)
    set_target_properties(${target} PROPERTIES UNITY_BUILD ON)
endfunction()

if(TARGET ZLIB::ZLIB)
    set(FINALE_MUS_READER_ZLIB_TARGET ZLIB::ZLIB)
elseif(TARGET zlibstatic)
    set(FINALE_MUS_READER_ZLIB_TARGET zlibstatic)
elseif(TARGET zlib)
    set(FINALE_MUS_READER_ZLIB_TARGET zlib)
elseif(FINALE_MUS_READER_USE_SYSTEM_ZLIB)
    find_package(ZLIB REQUIRED)
    set(FINALE_MUS_READER_ZLIB_TARGET ZLIB::ZLIB)
else()
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "Do not build zlib examples")
    finale_mus_reader_declare_zlib()
    set(FINALE_MUS_READER_SAVED_DEV_WARNINGS
        "${CMAKE_SUPPRESS_DEVELOPER_WARNINGS}")
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "")
    FetchContent_MakeAvailable(zlib)
    set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS
        "${FINALE_MUS_READER_SAVED_DEV_WARNINGS}" CACHE INTERNAL "")

    if(TARGET zlib)
        set_target_properties(zlib PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE)
        if(EMSCRIPTEN)
            # Both targets otherwise produce libz.a on platforms without shared libraries.
            set_target_properties(zlib PROPERTIES OUTPUT_NAME z_shared)
        endif()
    endif()

    finale_mus_reader_keep_out_of_unity(zlib zlibstatic)

    if(TARGET zlibstatic)
        set(FINALE_MUS_READER_ZLIB_TARGET zlibstatic)
    elseif(TARGET zlib)
        set(FINALE_MUS_READER_ZLIB_TARGET zlib)
    elseif(TARGET ZLIB::ZLIB)
        set(FINALE_MUS_READER_ZLIB_TARGET ZLIB::ZLIB)
    else()
        message(FATAL_ERROR "Unable to determine the zlib target")
    endif()
endif()

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS ${FINALE_MUS_READER_ZLIB_TARGET})
endif()
message(STATUS "Legacy Finale MUS Reader zlib target: ${FINALE_MUS_READER_ZLIB_TARGET}")

if(TARGET musx)
    get_target_property(FINALE_MUS_READER_MUSXDOM_SOURCE musx SOURCE_DIR)
    set(FINALE_MUS_READER_MUSXDOM_ORIGIN "existing musx target")
else()
    set(musx_BUILD_TESTING OFF CACHE BOOL "Do not build musxdom tests")

    if(FINALE_MUS_READER_MUSXDOM_SOURCE_DIR)
        get_filename_component(FINALE_MUS_READER_MUSXDOM_SOURCE_DIR
            "${FINALE_MUS_READER_MUSXDOM_SOURCE_DIR}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(NOT EXISTS "${FINALE_MUS_READER_MUSXDOM_SOURCE_DIR}/CMakeLists.txt")
            message(FATAL_ERROR
                "FINALE_MUS_READER_MUSXDOM_SOURCE_DIR is not a musxdom source tree")
        endif()
        add_subdirectory("${FINALE_MUS_READER_MUSXDOM_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}/_deps/musxdom-build")
        set(FINALE_MUS_READER_MUSXDOM_SOURCE
            "${FINALE_MUS_READER_MUSXDOM_SOURCE_DIR}")
        set(FINALE_MUS_READER_MUSXDOM_ORIGIN "local source tree")
    else()
        finale_mus_reader_declare_musxdom()
        FetchContent_MakeAvailable(musxdom)
        set(FINALE_MUS_READER_MUSXDOM_SOURCE "${musxdom_SOURCE_DIR}")
        set(FINALE_MUS_READER_MUSXDOM_ORIGIN "fetched pinned revision")
    endif()
endif()
message(STATUS
    "Legacy Finale MUS Reader musxdom: ${FINALE_MUS_READER_MUSXDOM_ORIGIN} at ${FINALE_MUS_READER_MUSXDOM_SOURCE}")

finale_mus_reader_keep_out_of_unity(musx)

add_library(finale_mus_reader_blast OBJECT
    third_party/blast/blast.c)

# Pinned upstream source rather than this project's, so it follows the same rule as the
# fetched dependencies even though the target is declared here.
finale_mus_reader_keep_out_of_unity(finale_mus_reader_blast)

if(MSVC)
    # C4244 remains enabled at /W3. Suppress it for the unmodified upstream
    # source while retaining useful third-party diagnostics below /W4.
    target_compile_options(finale_mus_reader_blast PRIVATE /W3 /WX- /wd4244)
else()
    target_compile_options(finale_mus_reader_blast PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(finale_mus_reader STATIC
    src/container/mus_container.cpp
    src/container/product_banner.cpp
    src/defaults/default_document.cpp
    src/defaults/embedded_default.cpp
    src/import/details/articulation_assign.cpp
    src/import/details/baseline.cpp
    src/import/details/center_shape.cpp
    src/import/details/chord_assign.cpp
    src/import/details/custom_key_octaves.cpp
    src/import/details/fretboard_diagrams.cpp
    src/import/details/gframe_hold.cpp
    src/import/details/independent_staff_details.cpp
    src/import/details/key_symbol_list.cpp
    src/import/details/lyric_details.cpp
    src/import/details/measure_graphic_assign.cpp
    src/import/details/measure_number_individual_positioning.cpp
    src/import/details/measure_text_assign.cpp
    src/import/details/notehead_details.cpp
    src/import/details/shape_note.cpp
    src/import/details/smart_shape_entry_assign.cpp
    src/import/details/special_tools.cpp
    src/import/details/staff_group.cpp
    src/import/details/staff_size.cpp
    src/import/details/tie_alter.cpp
    src/import/details/tuplet_def.cpp
    src/import/entries/entry.cpp
    src/import/header/header.cpp
    src/import/options/accidental_options.cpp
    src/import/options/alternate_notation_options.cpp
    src/import/options/augmentation_dot_options.cpp
    src/import/options/barline_options.cpp
    src/import/options/beam_options.cpp
    src/import/options/chord_options.cpp
    src/import/options/clef_options.cpp
    src/import/options/flag_options.cpp
    src/import/options/font_options.cpp
    src/import/options/grace_note_options.cpp
    src/import/options/key_signature_options.cpp
    src/import/options/line_curve_options.cpp
    src/import/options/lyric_options.cpp
    src/import/options/misc_options.cpp
    src/import/options/multimeasure_rest_options.cpp
    src/import/options/music_spacing_options.cpp
    src/import/options/music_symbol_options.cpp
    src/import/options/note_rest_options.cpp
    src/import/options/page_format_options.cpp
    src/import/options/piano_brace_bracket_options.cpp
    src/import/options/repeat_options.cpp
    src/import/options/smart_shape_options.cpp
    src/import/options/staff_options.cpp
    src/import/options/stem_options.cpp
    src/import/options/text_options.cpp
    src/import/options/tie_options.cpp
    src/import/options/time_signature_options.cpp
    src/import/options/tuplet_options.cpp
    src/import/others/articulation_def.cpp
    src/import/others/beat_chart_element.cpp
    src/import/others/bookmark.cpp
    src/import/others/chord_suffix_elements.cpp
    src/import/others/chord_suffix_playback.cpp
    src/import/others/clef_list.cpp
    src/import/others/custom_keys.cpp
    src/import/others/drum_staff.cpp
    src/import/others/enclosure.cpp
    src/import/others/file_path.cpp
    src/import/others/font_definitions.cpp
    src/import/others/frame.cpp
    src/import/others/fret_instruments.cpp
    src/import/others/fretboard_groups.cpp
    src/import/others/fretboard_styles.cpp
    src/import/others/graphic_assignments.cpp
    src/import/others/individual_positioning.cpp
    src/import/others/layer_attributes.cpp
    src/import/others/marking_category.cpp
    src/import/others/measure.cpp
    src/import/others/measure_expr_assign.cpp
    src/import/others/measure_number_region.cpp
    src/import/others/multimeasure_rest.cpp
    src/import/others/multistaff_instrument_group.cpp
    src/import/others/name_position.cpp
    src/import/others/page.cpp
    src/import/others/page_text_assign.cpp
    src/import/others/part_definitions.cpp
    src/import/others/part_globals.cpp
    src/import/others/part_voicing.cpp
    src/import/others/percussion_note_info.cpp
    src/import/others/playback.cpp
    src/import/others/repeat_back.cpp
    src/import/others/repeat_ending_start.cpp
    src/import/others/shape_definitions.cpp
    src/import/others/shape_expression_defs.cpp
    src/import/others/smart_shape.cpp
    src/import/others/smart_shape_custom_lines.cpp
    src/import/others/smart_shape_measure_assign.cpp
    src/import/others/split_measure.cpp
    src/import/others/staff.cpp
    src/import/others/staff_list.cpp
    src/import/others/staff_style_assign.cpp
    src/import/others/staff_system.cpp
    src/import/others/staff_used.cpp
    src/import/others/system_lock.cpp
    src/import/others/tempo_change.cpp
    src/import/others/text_blocks.cpp
    src/import/others/text_expression_defs.cpp
    src/import/others/text_repeat_assign.cpp
    src/import/others/text_repeat_def.cpp
    src/import/others/time_composite.cpp
    src/import/support/embedded_graphics.cpp
    src/import/support/enigma_text.cpp
    src/import/support/legacy_mapping.cpp
    src/import/support/legacy_percussion.cpp
    src/import/support/percussion_mappings.cpp
    src/import/support/playback_plugin_names.cpp
    src/import/support/reporting.cpp
    src/import/support/text_encoding.cpp
    src/import/texts/coda_texts.cpp
    src/import/texts/file_info_text.cpp
    src/import/texts/text_pool.cpp
    src/reader/document_factory.cpp
    src/reader/reader.cpp
    src/reader/resources.cpp
    src/records/legacy_others.cpp
    src/records/legacy_record_index.cpp
    $<TARGET_OBJECTS:finale_mus_reader_blast>)
# The two text importers declare different Target aliases in the same anonymous namespace.
set_source_files_properties(src/import/others/text_repeat_assign.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
finale_mus_reader_enable_unity(finale_mus_reader)
add_library(finale_mus_reader::finale_mus_reader ALIAS finale_mus_reader)
target_compile_features(finale_mus_reader PUBLIC cxx_std_20)

# The same warning policy as the tests: every diagnostic is a build break.
if(MSVC)
    target_compile_options(finale_mus_reader PRIVATE /W4 /WX)
else()
    target_compile_options(finale_mus_reader PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set(FINALE_MUS_READER_INSTRUMENTATION_ENABLED OFF)
if(FINALE_MUS_READER_INSTRUMENTATION STREQUAL "ON"
        OR (FINALE_MUS_READER_INSTRUMENTATION STREQUAL "AUTO" AND FINALE_MUS_READER_BUILD_TESTING))
    set(FINALE_MUS_READER_INSTRUMENTATION_ENABLED ON)
endif()
if(FINALE_MUS_READER_INSTRUMENTATION_ENABLED)
    target_sources(finale_mus_reader PRIVATE src/reader/timing.cpp)
endif()

target_include_directories(finale_mus_reader
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/blast")

target_link_libraries(finale_mus_reader
    PUBLIC musx
    PRIVATE
        ${FINALE_MUS_READER_ZLIB_TARGET}
)
if(FINALE_MUS_READER_INSTRUMENTATION_ENABLED)
    target_compile_definitions(finale_mus_reader PUBLIC
        FINALE_MUS_READER_ENABLE_INSTRUMENTATION=1)
endif()

# Legacy MUS stores text in whatever encoding the saving machine used, so the reader has to
# convert it to the UTF-8 that musxdom expects. Each platform gets the most accurate
# converter it has rather than a common subset: Windows uses its own APIs, macOS uses
# CoreFoundation, which is the only one of the three that resolves the classic Mac encodings
# natively, and everything else uses iconv. CoreFoundation is pure C, so no Objective-C
# toolchain is involved.
#
# iconv is not linked, probed for, or compiled on Apple: macOS ships its own libiconv that
# would otherwise be found and silently linked, and the source guards the iconv path behind
# the same condition. Linux and Emscripten exercise the iconv path.
if(WIN32)
    # MultiByteToWideChar and WideCharToMultiByte are in the standard Win32 libraries.
elseif(APPLE)
    target_link_libraries(finale_mus_reader PRIVATE "-framework CoreFoundation")
else()
    find_package(Iconv)
    if(Iconv_FOUND)
        target_link_libraries(finale_mus_reader PRIVATE Iconv::Iconv)
    else()
        message(FATAL_ERROR
            "iconv is required to convert legacy MUS text to UTF-8 on this platform.")
    endif()
endif()

# Internal build interface: the private src/ headers plus the XML backend the tests use. Not
# part of the library's API and never installed. It keeps test-only include paths and the
# pugixml selection in one target.
if(FINALE_MUS_READER_BUILD_TESTING OR FINALE_MUS_READER_BUILD_REPORTING_TESTING)
    if(TARGET pugixml)
        set(FINALE_MUS_READER_PUGIXML_TARGET pugixml)
    elseif(TARGET pugixml::pugixml)
        set(FINALE_MUS_READER_PUGIXML_TARGET pugixml::pugixml)
    else()
        set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "Do not build pugixml tests")
        finale_mus_reader_declare_pugixml()
        FetchContent_MakeAvailable(pugixml)
        finale_mus_reader_keep_out_of_unity(pugixml pugixml-static)
        set(FINALE_MUS_READER_PUGIXML_TARGET pugixml)
    endif()

    add_library(finale_mus_reader_internal INTERFACE)
    add_library(finale_mus_reader::internal ALIAS finale_mus_reader_internal)
    target_include_directories(finale_mus_reader_internal INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(finale_mus_reader_internal INTERFACE
        finale_mus_reader::finale_mus_reader
        ${FINALE_MUS_READER_PUGIXML_TARGET}
        ${FINALE_MUS_READER_ZLIB_TARGET})
endif()

if(FINALE_MUS_READER_BUILD_TESTING OR FINALE_MUS_READER_BUILD_REPORTING_TESTING)
    enable_testing()
endif()

# Built in either instrumentation mode: the reporting boundary checks, plus fixture reads that
# catch document values an importer assigns only inside a reporting callback.
if(FINALE_MUS_READER_BUILD_REPORTING_TESTING)
    add_executable(finale_mus_reader_reporting_tests
        tests/reporting_tests.cpp
        tests/disabled_reporting_construction_tests.cpp)
    finale_mus_reader_enable_unity(finale_mus_reader_reporting_tests)
    target_compile_definitions(finale_mus_reader_reporting_tests PRIVATE
        FINALE_MUS_READER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(finale_mus_reader_reporting_tests PRIVATE finale_mus_reader::internal)
    add_test(NAME reporting COMMAND finale_mus_reader_reporting_tests)
endif()

if(FINALE_MUS_READER_BUILD_TESTING)
    add_subdirectory(tests)
endif()

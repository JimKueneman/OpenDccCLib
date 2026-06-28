cmake_minimum_required(VERSION 3.14)

# Setup directory paths.
set(ROOT_DIR ${CMAKE_SOURCE_DIR}/..)

# Specify project.
project(opendccclib-test)

# Enable the required language standards.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Uncomment out the following line to get verbose build output.
set(CMAKE_VERBOSE_MAKEFILE ON)

# Get a copy of GoogleTest.
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)

# Compile flags.
add_compile_options(
    -fPIC -g -O0 -Wall -Werror --coverage -DGTEST
    -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -Wno-attributes -D_GNU_SOURCE
)

# GTest and GMock includes + typical user config
include_directories(
    ${CMAKE_SOURCE_DIR}/user_config/typical ${CMAKE_SOURCE_DIR}/ ${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include
)

# Add libraries that have tests disabled.

# Add libraries that have test enabled.
set(GTEST ON)
add_subdirectory(${ROOT_DIR}/src/dcc dcc)


set(LINK_LIBS ${LINK_LIBS}
    dcc
)

if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
# Mac OS X specific configuration
set(START_GROUP)
set(END_GROUP)
add_link_options(--coverage)
else ()
# Linux specific configuration
set(START_GROUP "-Wl,--start-group")
set(END_GROUP "-Wl,--end-group")
set(LINK_LIBS ${LINK_LIBS}
    -lgcov
)
endif ()

# Setup gcov target.
add_custom_target(TARGET_GCOV ALL
    DEPENDS gcovr/coverage.html
)

# Build an executable for each test file.
foreach(testsourcefile ${TESTS})
    # Manipulate path to result in unique test names.
    get_filename_component(testname ${testsourcefile} NAME_WE)
    get_filename_component(testdir ${testsourcefile} DIRECTORY)
    get_filename_component(testdir ${testdir} NAME)
    set(testname "${testdir}_${testname}")

    add_executable(${testname} ${testsourcefile})
    target_include_directories(${testname}
        PUBLIC
            ${ROOT_DIR}/src
    )
    target_link_libraries(${testname}
        GTest::gtest_main
        GTest::gmock_main

        -fPIC
        ${START_GROUP}
        ${LINK_LIBS}
        ${END_GROUP}
    )
    add_custom_command(TARGET ${testname}
        POST_BUILD
        COMMAND ./${testname}
        # Forces the coverage report to be regenerated if tests are run.
        COMMAND rm -rf gcovr
    )
    add_dependencies(TARGET_GCOV ${testname})
endforeach(testsourcefile ${TESTS})

# =============================================================================
# Compile-gate matrix — prove each real-world single-role config compiles+links.
#
# Roles (command station / mobile decoder / accessory decoder) are MUTUALLY
# EXCLUSIVE in real firmware, so each config enables exactly one role (plus an
# optional RailCom axis). The all-on "typical" config used by the behavioral
# suite can never catch a gating bug because every gate is on. Here a gating bug
# surfaces as:
#   * a COMPILE error  — a type/enum/decl used when its gate is off (-Werror), or
#   * a LINK error     — a symbol USED when its gate is off. The smoke exe links
#                        the per-config library whole-archive, so every object
#                        must resolve, exactly like a real single-role firmware.
#   * an nm LEAK       — a gated-off module that still emitted text symbols
#                        (define-when-off), caught by check_symbol_leakage.sh.
# =============================================================================
file(GLOB DCC_GATE_SOURCES ${ROOT_DIR}/src/dcc/*.c)

function(dcc_gate_target cfg)
    set(lib gate_${cfg})
    set(exe gate_${cfg}_link)

    add_library(${lib} STATIC ${DCC_GATE_SOURCES})
    target_include_directories(${lib} BEFORE PUBLIC
        ${CMAKE_SOURCE_DIR}/user_config/${cfg}
        ${ROOT_DIR}/src
    )
    # Smoke-only: keep the guarded-out TUs out of the coverage report.
    target_compile_options(${lib} PRIVATE -fno-profile-arcs -fno-test-coverage -Wno-unused-command-line-argument)

    add_executable(${exe} ${CMAKE_SOURCE_DIR}/dcc_gate_smoke.cxx)
    target_compile_options(${exe} PRIVATE -fno-profile-arcs -fno-test-coverage -Wno-unused-command-line-argument)
    if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(${exe} -Wl,-force_load ${lib})
    else ()
        target_link_libraries(${exe} -Wl,--whole-archive ${lib} -Wl,--no-whole-archive)
    endif ()

    # Remaining args = symbol prefixes of gated-off modules that must not leak.
    set(forbidden ${ARGN})
    if (forbidden)
        add_custom_command(TARGET ${exe} POST_BUILD
            COMMAND ./${exe}
            COMMAND ${CMAKE_SOURCE_DIR}/check_symbol_leakage.sh $<TARGET_FILE:${lib}> ${forbidden}
            VERBATIM
        )
    else ()
        add_custom_command(TARGET ${exe} POST_BUILD
            COMMAND ./${exe}
            VERBATIM
        )
    endif ()
endfunction()

# The trailing prefixes are the modules that MUST be gated out of each config;
# check_symbol_leakage.sh fails the build if any still export text symbols.
# Verified against nm output (e.g. DccPacketDecoder vs DccPacketEncoder, and the
# shared DccApplicationDecoderRailcom which is allowed in decoder + accessory).

set(DEC_MODULES
    DccApplicationDecoderCv DccBitDecoder DccCvStorage DccFailsafe DccPacketDecoder)
set(CS_MODULES
    DccApplicationCommandStationMainTrack DccApplicationCommandStationPacket
    DccApplicationCommandStationServiceTrack DccApplicationMainTrack
    DccApplicationServiceTrack DccBitEncoder DccScheduler DccPacketEncoder
    DccServiceMode)
set(CS_RAILCOM    DccRailcomCutout DccRailcomCommandStation)
set(DEC_RAILCOM   DccRailcomDecoder)
set(APP_RAILCOM   DccApplicationDecoderRailcom)
set(ACC_RAILCOM   DccApplicationAccessoryDecoderRailcom)

# command station: no decoder/accessory modules at all.
dcc_gate_target(command_station
    ${DEC_MODULES} ${DEC_RAILCOM} ${APP_RAILCOM} ${ACC_RAILCOM})
dcc_gate_target(command_station_no_railcom
    ${DEC_MODULES} ${DEC_RAILCOM} ${APP_RAILCOM} ${ACC_RAILCOM} ${CS_RAILCOM})

# mobile decoder: no command-station / accessory-specific / CS-railcom modules.
dcc_gate_target(decoder
    ${CS_MODULES} ${CS_RAILCOM} ${ACC_RAILCOM})
dcc_gate_target(decoder_no_railcom
    ${CS_MODULES} ${CS_RAILCOM} ${ACC_RAILCOM} ${DEC_RAILCOM} ${APP_RAILCOM})

# accessory decoder: no command-station / mobile-decoder modules.
dcc_gate_target(accessory
    ${CS_MODULES} ${CS_RAILCOM} ${DEC_MODULES} ${DEC_RAILCOM})
dcc_gate_target(accessory_no_railcom
    ${CS_MODULES} ${CS_RAILCOM} ${DEC_MODULES} ${DEC_RAILCOM} ${APP_RAILCOM} ${ACC_RAILCOM})

# Generate the HTML coverage report
add_custom_command(OUTPUT gcovr/coverage.html
    COMMAND mkdir -p gcovr
    # --exclude-directories drops the compile-gate build trees (gate_*.dir): on
    # Apple clang the -fno-*coverage override is ignored, so those guarded-out
    # builds would otherwise fold into the typical-build coverage numbers.
    COMMAND gcovr --gcov-ignore-parse-errors=all --html-details gcovr/coverage.html --exclude=_deps.* --exclude=.*_Test.* --exclude-directories=.*gate_.* -r ${ROOT_DIR} .
    COMMAND ln -sf ${CMAKE_SOURCE_DIR}/build/gcovr/coverage.html ${CMAKE_SOURCE_DIR}/
    VERBATIM
)

# MTD-Benchmark: NS-3 module CMake build configuration

# Check for minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Module name
set(module_name mtd-benchmark)

# Set source files
set(source_files
    model/mtd-event-bus.cc
    model/mtd-event-stream.cc
    model/mtd-detector.cc
    model/mtd-score-manager.cc
    model/mtd-domain-manager.cc
    model/mtd-shuffle-controller.cc
    model/mtd-attack-generator.cc
    model/mtd-export-api.cc
    model/mtd-security-tag.cc
    helper/mtd-network-helper.cc
    helper/mtd-traffic-helper.cc
    helper/mtd-analysis-helper.cc
    helper/mtd-telemetry-helper.cc
)

# Set header files (for installation)
set(header_files
    model/mtd-common.h
    model/mtd-event-bus.h
    model/mtd-event-stream.h
    model/mtd-detector.h
    model/mtd-score-manager.h
    model/mtd-domain-manager.h
    model/mtd-shuffle-controller.h
    model/mtd-attack-generator.h
    model/mtd-export-api.h
    model/mtd-security-tag.h
    helper/mtd-network-helper.h
    helper/mtd-traffic-helper.h
    helper/mtd-analysis-helper.h
    helper/mtd-telemetry-helper.h
)

# Set test source files
set(test_sources
    test/mtd-benchmark-test-suite.cc
)

# Dependencies
set(libraries_to_link
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libpoint-to-point}
    ${libcsma}
    ${libapplications}
    ${libflow-monitor}
)

# Build the module
build_lib(
    LIBNAME ${module_name}
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libraries_to_link}
    TEST_SOURCES ${test_sources}
)

# Stage pure-Python bridge package into build/bindings/python for source-tree development.
file(
    GLOB_RECURSE
    mtd_bridge_files
    ${CMAKE_CURRENT_SOURCE_DIR}/python/mtd_bridge/*
)
foreach(
    file
    ${mtd_bridge_files}
)
    string(
        REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/python"
                        "${CMAKE_OUTPUT_DIRECTORY}/bindings/python"
                        destination
                        ${file}
    )
    configure_file(
        ${file}
        ${destination}
        COPYONLY
    )
endforeach()

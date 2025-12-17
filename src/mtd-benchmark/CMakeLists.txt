# MTD-Benchmark: NS-3 module CMake build configuration

# Check for minimum CMake version
cmake_minimum_required(VERSION 3.16)

# Module name
set(module_name mtd-benchmark)

# Set source files
set(source_files
    model/mtd-event-bus.cc
    model/mtd-detector.cc
    model/mtd-score-manager.cc
    model/mtd-domain-manager.cc
    model/mtd-shuffle-controller.cc
    model/mtd-attack-generator.cc
    model/mtd-export-api.cc
    helper/mtd-network-helper.cc
)

# Set header files (for installation)
set(header_files
    model/mtd-common.h
    model/mtd-event-bus.h
    model/mtd-detector.h
    model/mtd-score-manager.h
    model/mtd-domain-manager.h
    model/mtd-shuffle-controller.h
    model/mtd-attack-generator.h
    model/mtd-export-api.h
    helper/mtd-network-helper.h
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
    ${libapplications}
)

# Build the module
build_lib(
    LIBNAME ${module_name}
    SOURCE_FILES ${source_files}
    HEADER_FILES ${header_files}
    LIBRARIES_TO_LINK ${libraries_to_link}
    TEST_SOURCES ${test_sources}
)

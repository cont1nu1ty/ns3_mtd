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
    model/mtd-python-interface.cc
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
    model/mtd-python-interface.h
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

# ==================== Python Bindings (Pybind11) ====================

# Find Python and pybind11
find_package(Python3 COMPONENTS Interpreter Development QUIET)
find_package(pybind11 QUIET)

if(Python3_FOUND AND pybind11_FOUND)
    message(STATUS "Building MTD-Benchmark Python bindings")
    
    # Create Python module
    pybind11_add_module(mtd_benchmark
        bindings/mtd-benchmark-bindings.cc
    )
    
    # Link with the main module library
    target_link_libraries(mtd_benchmark PRIVATE
        ${lib${module_name}}
        ${libcore}
        ${libnetwork}
    )
    
    # Set output directory
    set_target_properties(mtd_benchmark PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/python"
    )
    
    # Install Python module
    install(TARGETS mtd_benchmark
        LIBRARY DESTINATION lib/python
        COMPONENT python
    )
    
else()
    message(STATUS "Python3 or pybind11 not found, skipping Python bindings")
    message(STATUS "  To enable Python bindings, install pybind11:")
    message(STATUS "    pip install pybind11")
    message(STATUS "    # or on Ubuntu/Debian:")
    message(STATUS "    sudo apt install pybind11-dev python3-pybind11")
endif()

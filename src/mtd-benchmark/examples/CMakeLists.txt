build_lib_example(
  NAME mtd-benchmark-example
  SOURCE_FILES mtd-benchmark-example.cc
  LIBRARIES_TO_LINK ${libmtd-benchmark}
)

build_lib_example(
  NAME mtd-basic-simulation
  SOURCE_FILES mtd-basic-simulation.cc
  LIBRARIES_TO_LINK ${libmtd-benchmark}
)

build_lib_example(
  NAME mtd-python-integration
  SOURCE_FILES mtd-python-integration.cc
  LIBRARIES_TO_LINK ${libmtd-benchmark}
)

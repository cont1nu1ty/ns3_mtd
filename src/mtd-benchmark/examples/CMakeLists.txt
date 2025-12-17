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

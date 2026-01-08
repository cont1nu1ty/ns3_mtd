# MTD-Benchmark Examples
#
# mtd-full-defense-test: Complete DDoS attack-defense demonstration

build_lib_example(
  NAME mtd-full-defense-test
  SOURCE_FILES mtd-full-defense-test.cc
  LIBRARIES_TO_LINK ${libmtd-benchmark}
)

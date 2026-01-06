# MTD-Benchmark Examples
#
# 1. mtd-full-defense-test: Complete DDoS attack-defense demonstration
# 2. mtd-python-integration: Python external algorithm integration example

build_lib_example(
  NAME mtd-full-defense-test
  SOURCE_FILES mtd-full-defense-test.cc
  LIBRARIES_TO_LINK ${libmtd-benchmark}
)

build_lib_example(
  NAME mtd-python-integration
  SOURCE_FILES mtd-python-integration.cc
  LIBRARIES_TO_LINK ${libmtd-benchmark}
)

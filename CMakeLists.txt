cmake_minimum_required(VERSION 3.16)
project(machxl_native VERSION 0.2 LANGUAGES C CXX)
include(GNUInstallDirs)

# Version and release date of the program, printed by --version and at the
# top of every report. A version tag (vX.Y) in the checked-out history wins
# over the project version above. The date is that of the last commit, or of
# the build when the sources are not a git checkout. Both are read when CMake
# configures the build.
set(MACHXL_VERSION "${PROJECT_VERSION}")
string(TIMESTAMP MACHXL_SOURCE_DATE "%Y-%m-%d" UTC)
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --match "v[0-9]*" --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} RESULT_VARIABLE MACHXL_GIT_TAG_RESULT
    OUTPUT_VARIABLE MACHXL_GIT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(MACHXL_GIT_TAG_RESULT EQUAL 0 AND MACHXL_GIT_TAG MATCHES "^v([0-9].*)$")
    set(MACHXL_VERSION "${CMAKE_MATCH_1}")
  endif()
  execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%cs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} RESULT_VARIABLE MACHXL_GIT_DATE_RESULT
    OUTPUT_VARIABLE MACHXL_GIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(MACHXL_GIT_DATE_RESULT EQUAL 0 AND MACHXL_GIT_DATE MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
    set(MACHXL_SOURCE_DATE "${MACHXL_GIT_DATE}")
  endif()
endif()
string(REGEX MATCH "^([0-9][0-9])([0-9][0-9])-0?([0-9]+)-0?([0-9]+)$" MACHXL_DATE_PARTS "${MACHXL_SOURCE_DATE}")
set(MACHXL_RELEASE_DATE "${CMAKE_MATCH_3}-${CMAKE_MATCH_4}-${CMAKE_MATCH_2}")
set(MACHXL_RELEASE_YEAR "${CMAKE_MATCH_1}${CMAKE_MATCH_2}")
configure_file(src/machxl_version.hpp.in generated/machxl_version.hpp @ONLY)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(MACHXL_CLANG_FORMAT NAMES clang-format
  HINTS /opt/homebrew/opt/llvm/bin
        /Library/Developer/CommandLineTools/usr/bin)
if(MACHXL_CLANG_FORMAT)
  file(GLOB MACHXL_FORMAT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
  add_custom_target(format
    COMMAND ${MACHXL_CLANG_FORMAT} -i ${MACHXL_FORMAT_SOURCES}
    COMMENT "Formatting MachXL source files")
  add_custom_target(format-check
    COMMAND ${MACHXL_CLANG_FORMAT} --dry-run --Werror ${MACHXL_FORMAT_SOURCES}
    COMMENT "Checking MachXL source formatting")
endif()

add_subdirectory(third_party/espresso)
add_library(machxl_frontend src/logic.cpp src/frontend.cpp src/jedec.cpp src/signature.cpp src/simulator.cpp src/simulation_reports.cpp src/simulation_placement.cpp src/settings.cpp src/synthesis.cpp src/fitter_legacy_failure_report.cpp src/fitter_failure_report.cpp src/power_reports.cpp src/fitter_report_data.cpp src/fitter_reports.cpp src/fitter_place_report.cpp src/timing_report.cpp
  src/device_mach111.cpp src/fitter.cpp src/fitter_metadata.cpp src/fitter_budget.cpp src/fitter_route.cpp src/fitter_inputs.cpp src/fitter_order.cpp
  src/fitter_partition.cpp src/fitter_place.cpp src/device_family.cpp src/fitter_family.cpp src/fitter_family_place.cpp src/fitter_modern_place.cpp src/fitter_modern.cpp
  src/optimizer.cpp src/optimizer_bridge.c)
target_link_libraries(machxl_frontend PRIVATE machxl_espresso)
target_include_directories(machxl_frontend PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(machxl_frontend PRIVATE -Wall -Wextra -Wpedantic)
add_executable(machxl src/main.cpp)
target_link_libraries(machxl PRIVATE machxl_frontend)
install(TARGETS machxl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES third_party/espresso/COPYRIGHT
  DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/machxl
  RENAME espresso-COPYRIGHT)
include(CTest)
if(BUILD_TESTING)
  add_executable(espresso_set_test tests/espresso_set_test.c)
  target_link_libraries(espresso_set_test PRIVATE machxl_espresso)
  add_test(NAME espresso_packed_sets COMMAND espresso_set_test)
  add_executable(logic_test tests/logic_test.cpp)
  target_link_libraries(logic_test PRIVATE machxl_frontend)
  add_test(NAME boolean_logic COMMAND logic_test)
  add_executable(frontend_test tests/frontend_test.cpp)
  target_link_libraries(frontend_test PRIVATE machxl_frontend)
  add_test(NAME frontend COMMAND frontend_test)
  add_executable(jedec_test tests/jedec_test.cpp)
  target_link_libraries(jedec_test PRIVATE machxl_frontend)
  add_test(NAME jedec_writer COMMAND jedec_test)
  add_executable(settings_test tests/settings_test.cpp)
  target_link_libraries(settings_test PRIVATE machxl_frontend)
  add_test(NAME settings COMMAND settings_test)
  add_executable(fitter_device_test tests/fitter_device_test.cpp)
  target_link_libraries(fitter_device_test PRIVATE machxl_frontend)
  add_test(NAME mach111_physical_codec COMMAND fitter_device_test)
  add_executable(fitter_route_test tests/fitter_route_test.cpp)
  target_link_libraries(fitter_route_test PRIVATE machxl_frontend)
  add_test(NAME ordered_csm_router COMMAND fitter_route_test)
  add_executable(fitter_budget_test tests/fitter_budget_test.cpp)
  target_link_libraries(fitter_budget_test PRIVATE machxl_frontend)
  add_test(NAME fitting_runtime_budget COMMAND fitter_budget_test)
  add_executable(fitter_inputs_test tests/fitter_inputs_test.cpp)
  target_link_libraries(fitter_inputs_test PRIVATE machxl_frontend)
  add_test(NAME original_input_pin_retries COMMAND fitter_inputs_test)
  add_executable(fitter_order_test tests/fitter_order_test.cpp)
  target_link_libraries(fitter_order_test PRIVATE machxl_frontend)
  add_test(NAME original_fitter_order COMMAND fitter_order_test)
  add_executable(fitter_partition_test tests/fitter_partition_test.cpp)
  target_link_libraries(fitter_partition_test PRIVATE machxl_frontend)
  add_test(NAME original_fitter_partition COMMAND fitter_partition_test)
  add_executable(fitter_legacy_failure_report_test tests/fitter_legacy_failure_report_test.cpp)
  target_link_libraries(fitter_legacy_failure_report_test PRIVATE machxl_frontend)
  add_test(NAME legacy_failure_metadata COMMAND fitter_legacy_failure_report_test)
  add_executable(power_reports_test tests/power_reports_test.cpp)
  target_link_libraries(power_reports_test PRIVATE machxl_frontend)
  add_test(NAME native_power_reports COMMAND power_reports_test)
  add_executable(fitter_place_test tests/fitter_place_test.cpp)
  target_link_libraries(fitter_place_test PRIVATE machxl_frontend)
  add_test(NAME original_fitter_placement COMMAND fitter_place_test)
  add_executable(fitter_modern_place_test tests/fitter_modern_place_test.cpp)
  target_link_libraries(fitter_modern_place_test PRIVATE machxl_frontend)
  add_test(NAME original_modern_placement COMMAND fitter_modern_place_test)
  add_executable(device_family_test tests/device_family_test.cpp)
  target_link_libraries(device_family_test PRIVATE machxl_frontend)
  add_test(NAME device_family_physical_codecs COMMAND device_family_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/device-family-reference)
  add_executable(fitter_family_test tests/fitter_family_test.cpp)
  target_link_libraries(fitter_family_test PRIVATE machxl_frontend)
  add_test(NAME native_family_fitter COMMAND fitter_family_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter-family-reference)
  add_executable(signature_test tests/signature_test.cpp)
  target_link_libraries(signature_test PRIVATE machxl_frontend)
  add_test(NAME original_jtag_signatures COMMAND signature_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/signature-reference)
  add_executable(optimizer_test tests/optimizer_test.cpp)
  target_link_libraries(optimizer_test PRIVATE machxl_frontend)
  add_test(NAME espresso_optimizer COMMAND optimizer_test)
  # Regression assertions must execute in Release builds as well.
  get_property(machxl_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(machxl_test_target IN LISTS machxl_test_targets)
    if(machxl_test_target MATCHES "_test$")
      target_compile_options(${machxl_test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_executable(jedec_fixture tests/jedec_fixture.cpp)
    target_link_libraries(jedec_fixture PRIVATE machxl_frontend)
    add_executable(simulator_reference_driver tests/simulator_driver.cpp)
    target_link_libraries(simulator_reference_driver PRIVATE machxl_frontend)
    add_executable(simulation_reports_driver tests/simulation_reports_driver.cpp)
    target_link_libraries(simulation_reports_driver PRIVATE machxl_frontend)
    add_executable(fitter_reports_driver tests/fitter_reports_driver.cpp)
    target_link_libraries(fitter_reports_driver PRIVATE machxl_frontend)
    add_executable(fitter_place_report_driver tests/fitter_place_report_driver.cpp)
    target_link_libraries(fitter_place_report_driver PRIVATE machxl_frontend)
    add_executable(timing_reports_driver tests/timing_reports_driver.cpp)
    target_link_libraries(timing_reports_driver PRIVATE machxl_frontend)
    add_test(NAME original_simulation_vectors
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/simulator_reference.py $<TARGET_FILE:simulator_reference_driver> $<TARGET_FILE:machxl>)
    add_test(NAME original_simulation_relations
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/simulation_relations.py $<TARGET_FILE:machxl>)
    add_test(NAME original_simulation_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/simulation_reports_reference.py $<TARGET_FILE:simulation_reports_driver> $<TARGET_FILE:machxl>)
    add_test(NAME original_fitter_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_reports.py $<TARGET_FILE:fitter_reports_driver> $<TARGET_FILE:machxl>)
    add_test(NAME native_fitter_failure_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_failure_reports.py $<TARGET_FILE:machxl>)
    add_test(NAME original_legacy_failure_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy_failure_reports.py $<TARGET_FILE:machxl>)
    add_test(NAME original_legacy_registered_failure_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy_registered_failure_reports.py $<TARGET_FILE:machxl>)
    add_test(NAME original_legacy_multiblock_failure_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy_multiblock_failure_reports.py $<TARGET_FILE:machxl>)
    add_test(NAME native_partition_failure_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/partition_failure_reports.py $<TARGET_FILE:machxl>)
    add_test(NAME original_power_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/power_reports.py $<TARGET_FILE:machxl> $<TARGET_FILE:power_reports_test>)
    add_test(NAME original_place_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_place_reports_reference.py $<TARGET_FILE:fitter_place_report_driver> $<TARGET_FILE:machxl>)
    add_test(NAME original_fitter_input_auto
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_input_auto.py $<TARGET_FILE:machxl>)
    add_test(NAME original_timing_reports
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/timing_reports.py $<TARGET_FILE:timing_reports_driver> $<TARGET_FILE:machxl>)
    add_test(NAME original_jedec_roundtrip
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/jedec_roundtrip.py $<TARGET_FILE:jedec_fixture>)
    add_test(NAME original_pla_equivalence
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME original_corpus_functions
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend_corpus.py $<TARGET_FILE:machxl>)
    add_test(NAME original_blizzard_functions
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/blizzard_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME original_synthesis_options
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/synthesis_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME original_language_probes
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend_probes.py $<TARGET_FILE:machxl>)
    add_test(NAME native_fixed_fitter
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME native_automatic_fitter
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/automatic_fitter_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME complete_archive_compatibility
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/archive_compatibility.py $<TARGET_FILE:machxl>)
    add_test(NAME native_modern_fitter
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/modern_fitter_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME original_blizzard_helpers
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/blizzard_helpers.py $<TARGET_FILE:machxl>)
    add_test(NAME original_reserved_names
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend_reserved.py $<TARGET_FILE:machxl>)
    add_test(NAME original_xor_descriptor_compatibility
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_xor_descriptor.py $<TARGET_FILE:machxl>)
    add_test(NAME original_settings_compatibility
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/settings_compatibility.py $<TARGET_FILE:machxl>)
    add_test(NAME original_fitting_settings
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/modern_fitter_reference.py $<TARGET_FILE:machxl> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitting-settings-reference)
    add_test(NAME original_legacy_fitting_options
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/modern_fitter_reference.py $<TARGET_FILE:machxl> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter-legacy-options-reference)
    add_test(NAME original_family_stress
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/modern_fitter_reference.py $<TARGET_FILE:machxl> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter-family-stress-reference)
    add_test(NAME reference_file_validation
      COMMAND ${Python3_EXECUTABLE} -m unittest discover -s ${CMAKE_CURRENT_SOURCE_DIR}/tests -p test_*.py)
    set_tests_properties(reference_file_validation PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME original_simulation_placement
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/simulation_placement_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME original_state_defaults
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/state_default_reference.py $<TARGET_FILE:machxl>)
    add_test(NAME native_input_robustness
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/input_robustness.py $<TARGET_FILE:machxl>)
    add_test(NAME original_fitter_rejections
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/fitter_rejections.py $<TARGET_FILE:machxl>)
    add_test(NAME original_saved_placement
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/placement_reference.py $<TARGET_FILE:machxl>)
  endif()
endif()

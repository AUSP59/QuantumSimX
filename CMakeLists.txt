
cmake_minimum_required(VERSION 3.20)
project(QUANTUM_SIMX VERSION 1.2.0 LANGUAGES CXX)

include(cmake/Options.cmake)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(quantum_simx STATIC
  src/state_vector.cpp
  src/gates.cpp
  src/circuit.cpp
  src/random.cpp
)
target_compile_definitions(quantum_simx PUBLIC QSX_VERSION=\"${PROJECT_VERSION}\" )

# Density matrix backend
target_sources(quantum_simx PRIVATE src/density_matrix.cpp src/qasm.cpp src/optimize.cpp src/grad.cpp)

# MPI distributed (optional)
if(ENABLE_MPI)
  target_sources(quantum_simx PRIVATE mpi/distributed_state.cpp)
  target_compile_definitions(quantum_simx PUBLIC QSX_MPI)
endif()

# Reproducible build flags suggestion
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.)
endif()

target_include_directories(quantum_simx PUBLIC include)
target_compile_definitions(quantum_simx PRIVATE $<$<BOOL:${ENABLE_OPENMP}>:QSX_OPENMP>)
if(ENABLE_OPENMP)
  find_package(OpenMP REQUIRED)
  target_link_libraries(quantum_simx PUBLIC OpenMP::OpenMP_CXX)
endif()

# CLI
add_executable(quantum-simx cli/main.cpp)
target_link_libraries(quantum-simx PRIVATE quantum_simx)

# Optional GUI
if(BUILD_GUI)
  find_package(Qt5 COMPONENTS Widgets QUIET)
  if(Qt5Widgets_FOUND)
    add_executable(quantum-simx-gui gui/main_qt.cpp gui/main_window.cpp)
    target_link_libraries(quantum-simx-gui PRIVATE quantum_simx Qt5::Widgets)
    target_compile_definitions(quantum-simx-gui PRIVATE QSX_GUI_QT)
  else()
    message(WARNING "Qt5 not found; GUI will not be built.")
  endif()
endif()

# Optional MPI
if(ENABLE_MPI)
  find_package(MPI REQUIRED)
  target_link_libraries(quantum_simx PUBLIC MPI::MPI_CXX)
  target_compile_definitions(quantum_simx PUBLIC QSX_MPI)
endif()

# Tests
if(BUILD_TESTS)
  enable_testing()
  add_executable(tests tests/test_main.cpp tests/test_parser.cpp)
  target_link_libraries(tests PRIVATE quantum_simx)
  add_test(NAME unit COMMAND tests)
endif()

# Benchmarks
if(BUILD_BENCHMARKS)
  add_executable(bench benchmarks/bench_apply.cpp)
  target_link_libraries(bench PRIVATE quantum_simx)
endif()

# Install & package
include(GNUInstallDirs)
install(TARGETS quantum_simx quantum-simx
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "quantum-simx")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
include(CPack)


# Fuzzing (optional): -DBUILD_FUZZ=ON
option(BUILD_FUZZ "Build fuzz targets (requires Clang/LibFuzzer)" OFF)
if(BUILD_FUZZ AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
  add_executable(fuzz_parse tests/fuzz_parse.cpp)
  target_link_libraries(fuzz_parse PRIVATE quantum_simx)
  target_compile_options(fuzz_parse PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
  target_link_options(fuzz_parse PRIVATE -fsanitize=fuzzer,address,undefined)
endif()


# Optional Python bindings (requires pybind11)
option(BUILD_PY "Build Python bindings" OFF)
if(BUILD_PY)
  find_package(pybind11 CONFIG QUIET)
  if(pybind11_FOUND)
    pybind11_add_module(qsx_python bindings/python/module.cpp)
    target_link_libraries(qsx_python PRIVATE quantum_simx)
  else()
    message(WARNING "pybind11 not found; Python bindings skipped")
  endif()
endif()

# Structure-of-Arrays optimization toggle
option(QSX_SOA "Use SoA memory layout for amplitudes (experimental fast path)" OFF)
if(QSX_SOA)
  target_compile_definitions(quantum_simx PUBLIC QSX_SOA)
endif()

# Extra CPack generators
set(CPACK_GENERATOR "ZIP;TGZ")

install(FILES docs/man/quantum-simx.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1 OPTIONAL)


include(CMakePackageConfigHelpers)
install(TARGETS quantum_simx
        EXPORT quantum-simx-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/QuantumSimXConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/QuantumSimXConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/QuantumSimXConfig.cmake"
  INSTALL_DESTINATION lib/cmake/QuantumSimX)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/QuantumSimXConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/QuantumSimXConfigVersion.cmake"
  DESTINATION lib/cmake/QuantumSimX)

install(EXPORT quantum-simx-targets
  FILE QuantumSimXTargets.cmake
  NAMESPACE QuantumSimX::
  DESTINATION lib/cmake/QuantumSimX)


# pkg-config file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/quantum-simx.pc.in ${CMAKE_CURRENT_BINARY_DIR}/quantum-simx.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/quantum-simx.pc DESTINATION lib/pkgconfig)


add_library(quantum_simx_c src/c_api.cpp)
target_include_directories(quantum_simx_c PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(quantum_simx_c PRIVATE quantum_simx)

install(TARGETS quantum_simx_c
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include)
install(FILES include/quantum/c_api.h DESTINATION include/quantum)


# Install zsh & fish completions
install(FILES extras/completion/_quantum-simx DESTINATION share/zsh/site-functions)
install(FILES extras/completion/quantum-simx.fish DESTINATION share/fish/vendor_completions.d)

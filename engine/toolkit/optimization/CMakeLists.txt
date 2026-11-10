# The optional optimization toolkit: its manifest lets the engine discover this folder; it is
# a separate target because it depends on OSQP. See include/paperboat/toolkit/optimization.hpp.

find_package(osqp QUIET CONFIG)
option(PAPERBOAT_WITH_OPTIMIZATION "Build the optimization toolkit package (paperboat::toolkit_optimization)" ON)
if(NOT PAPERBOAT_WITH_OPTIMIZATION)
  return()
endif()

if(osqp_FOUND)
  set(PAPERBOAT_OSQP_TARGET osqp::osqpstatic)
else()
  include(FetchContent)
  set(OSQP_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
  set(OSQP_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
  set(OSQP_BUILD_DEMO_EXE OFF CACHE BOOL "" FORCE)
  set(OSQP_BUILD_UNITTESTS OFF CACHE BOOL "" FORCE)
  set(OSQP_ENABLE_PRINTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(osqp
    URL https://github.com/osqp/osqp/archive/refs/tags/v1.0.0.tar.gz
    URL_HASH SHA256=dd6a1c2e7e921485697d5e7cdeeb043c712526c395b3700601f51d472a7d8e48
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
  FetchContent_MakeAvailable(osqp)
  set(PAPERBOAT_OSQP_TARGET osqpstatic)
endif()

# Ipopt is optional: without it the NLP block still registers and reports "needs Ipopt"; the
# QP/LP blocks and the control MPC keep working.
find_path(PAPERBOAT_IPOPT_INCLUDE_DIR IpIpoptApplication.hpp PATH_SUFFIXES coin)
find_library(PAPERBOAT_IPOPT_LIBRARY ipopt)

add_library(paperboat_toolkit_optimization src/blocks.cpp src/solver.cpp src/nlp.cpp)
add_library(paperboat::toolkit_optimization ALIAS paperboat_toolkit_optimization)
target_include_directories(paperboat_toolkit_optimization PUBLIC include)
target_link_libraries(paperboat_toolkit_optimization
  PUBLIC paperboat::toolkit
  PRIVATE ${PAPERBOAT_OSQP_TARGET} paperboat_warnings)
if(PAPERBOAT_IPOPT_INCLUDE_DIR AND PAPERBOAT_IPOPT_LIBRARY)
  target_compile_definitions(paperboat_toolkit_optimization PRIVATE PAPERBOAT_HAS_IPOPT)
  target_include_directories(paperboat_toolkit_optimization PRIVATE ${PAPERBOAT_IPOPT_INCLUDE_DIR})
  target_link_libraries(paperboat_toolkit_optimization PRIVATE ${PAPERBOAT_IPOPT_LIBRARY})
endif()

if(PAPERBOAT_BUILD_TESTS)
  add_executable(paperboat_optimization_tests tests/solver_test.cpp)
  target_include_directories(paperboat_optimization_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(paperboat_optimization_tests PRIVATE paperboat_toolkit_optimization Catch2::Catch2WithMain)
  add_test(NAME paperboat_optimization_tests COMMAND paperboat_optimization_tests)
endif()

# opt-01-beam-sizing: a QP step with the optimization toolkit.
cmake_minimum_required(VERSION 3.25)
project(paperboat_opt_beam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT PAPERBOAT_ENGINE_DIR)
  foreach(dir
      "${CMAKE_CURRENT_LIST_DIR}/../../../../"          # installed: <paperboat>/engine
      "${CMAKE_CURRENT_LIST_DIR}/../../../../paperboat/engine")
    if(EXISTS "${dir}/CMakeLists.txt")
      set(PAPERBOAT_ENGINE_DIR "${dir}")
      break()
    endif()
  endforeach()
endif()
if(NOT PAPERBOAT_ENGINE_DIR)
  message(FATAL_ERROR "No Paperboat engine found: set PAPERBOAT_ENGINE_DIR")
endif()
set(PAPERBOAT_ENGINE_DIR "${PAPERBOAT_ENGINE_DIR}" CACHE PATH "Paperboat engine sources")
add_subdirectory(${PAPERBOAT_ENGINE_DIR} paperboat EXCLUDE_FROM_ALL)

# The toolkit: the engine's installed copy, or this repository's folder.
if(NOT TARGET paperboat::toolkit_optimization)
  foreach(dir
      "${PAPERBOAT_ENGINE_DIR}/toolkit/optimization"
      "${CMAKE_CURRENT_LIST_DIR}/../.."
      "${CMAKE_CURRENT_LIST_DIR}/../../engine/toolkit/optimization")
    if(EXISTS "${dir}/toolkit.toml")
      add_subdirectory("${dir}" opt-toolkit)
      break()
    endif()
  endforeach()
endif()
if(NOT TARGET paperboat::toolkit_optimization)
  message(FATAL_ERROR "The optimization toolkit is not installed (paperboat toolkit install optimization)")
endif()

add_executable(paperboat-opt-beam main.cpp)
target_link_libraries(paperboat-opt-beam PRIVATE paperboat::core paperboat::toolkit_optimization paperboat_warnings)
target_compile_definitions(paperboat-opt-beam PRIVATE
  PAPERBOAT_DESCRIPTOR="${CMAKE_CURRENT_SOURCE_DIR}/descriptor.toml")

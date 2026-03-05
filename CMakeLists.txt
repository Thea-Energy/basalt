cmake_minimum_required(VERSION 3.15)

project(phnx)

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(CMAKE_POSITION_INDEPENDENT_CODE ON) # Resolves -fPIC errors

include(FetchContent)
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/_deps)
set(FETCHCONTENT_QUIET TRUE)

find_package(
        Python 3.10
        COMPONENTS Interpreter Development.Module
        REQUIRED)


# ===============================================================================
# Nanobind
# ===============================================================================
execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)


# ===============================================================================
# Simmetrix SimModSuite
# ===============================================================================
add_library(sms INTERFACE)

target_include_directories(sms INTERFACE include/gmcore)
target_link_directories(sms INTERFACE lib/gmcore)

target_include_directories(sms INTERFACE include/mscore)
target_link_directories(sms INTERFACE lib/mscore)

target_include_directories(sms INTERFACE include/pskrnl)
target_link_directories(sms INTERFACE lib/pskrnl lib/pskrnl/psKrnl)

target_include_directories(sms INTERFACE include/license)
target_link_directories(sms INTERFACE lib/license)

target_link_libraries(sms INTERFACE SimParasolid380 SimPartitionedMesh SimPartitionWrapper SimMeshing SimMeshTools SimModel pskernel tirpc SimLicense)

# ===============================================================================
# Gmsh
# ===============================================================================
find_library(GMSH_LIB gmsh)
if(NOT GMSH_LIB)
  message(FATAL_ERROR "Could not find libgmsh")
endif()

find_path(GMSH_INC gmsh.h)
if(NOT GMSH_INC)
  message(FATAL_ERROR "Could not find gmsh.h")
endif()

include_directories(${GMSH_INC})


# ===============================================================================
# Logging
# ===============================================================================
set(SPDLOG_VERSION v1.12.0)
FetchContent_Declare(
        spdlog URL https://github.com/gabime/spdlog/archive/${SPDLOG_VERSION}.tar.gz)

FetchContent_MakeAvailable(spdlog)

# ===============================================================================
# Main Library
# ===============================================================================
add_library(
    main
    src/init.cpp
    src/phnx.cpp
)
target_include_directories(main PUBLIC ${SRC_DIR})
target_link_libraries(main PUBLIC 
nanobind 
sms 
spdlog::spdlog 
${GMSH_LIB}
)

# ===============================================================================
# Examples
# ===============================================================================
file(GLOB EXAMPLE_SOURCES examples/*)
foreach(example_file ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${example_file} NAME_WE)
    add_executable(${example_name} ${example_file})
    target_link_libraries(${example_name} PRIVATE main)
endforeach()

# ===============================================================================
# Python Bindings
# ===============================================================================
file(GLOB PYTHON_SOURCES ${SRC_DIR}/python/*.cpp)
nanobind_add_module(_core NB_SHARED # STATIC is the default
        ${PYTHON_SOURCES})

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/phnx/_core)

# Inspect RPATH with readelf -d phnx/_core/_core.cpython-312-x86_64-linux-gnu.so | grep RUNPATH
set_property(
        TARGET _core
        APPEND
        PROPERTY INSTALL_RPATH 
        "${CMAKE_INSTALL_PREFIX}:${CMAKE_CURRENT_SOURCE_DIR}/.pixi/envs/default/lib:${CMAKE_CURRENT_SOURCE_DIR}/lib/pskrnl/psKrnl")

nanobind_add_stub(
        _core_stub
        MODULE
        _core
        OUTPUT
        _core.pyi
        PYTHON_PATH
        "."
        DEPENDS
        _core)

target_link_libraries(_core PRIVATE main)
target_include_directories(_core PRIVATE ${SRC_DIR}/python)
target_include_directories(_core PRIVATE ${SRC_DIR}/autogen)

set(GEN_FILES
phnx.h
)
foreach (SOURCE_FILE ${GEN_FILES})
    get_filename_component(FILE_NAME ${SOURCE_FILE} NAME_WE)
    set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/${SOURCE_FILE}")
    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/autogen")
    set (OUTPUT_FILE "${OUTPUT_DIR}/bind_${FILE_NAME}.h")
    list(APPEND GENERATED_FILES ${OUTPUT_FILE})

    add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND python -m nanobindgen -o ${OUTPUT_DIR}
            ${INPUT_FILE}
            DEPENDS ${INPUT_FILE} ${GEN_SCRIPT}
            COMMENT "Generating bindings for ${SOURCE_FILE}")
endforeach ()

add_custom_target(generate_bindings DEPENDS ${GENERATED_FILES})
add_dependencies(_core generate_bindings)

# ===============================================================================
# Install
# ===============================================================================
install(TARGETS _core nanobind DESTINATION ".")
install(FILES build/_core.pyi DESTINATION ".")

# ===============================================================================
# Post-build commands
# ===============================================================================
add_custom_command(
        TARGET main
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/TheaEnergyEval2025"
        ${CMAKE_BINARY_DIR}
        COMMENT
        "Copying License file to build directory")

# TODO(akoen): We should copy libpskernel to _core directory
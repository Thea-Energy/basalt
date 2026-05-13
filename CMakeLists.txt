cmake_minimum_required(VERSION 3.15)

project(basalt)

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
# Simmetrix SimModSuite (bring-your-own)
# ===============================================================================
if(NOT DEFINED ENV{SIMMODSUITE_ROOT})
    message(FATAL_ERROR
        "SIMMODSUITE_ROOT environment variable is not set.\n"
        "basalt does not bundle Simmetrix SimModSuite. Download the gmcore, mscore, pskrnl, "
        "and simlicense tarballs from Simmetrix's customer portal (all at the same "
        "version-datestamp), extract them into one parent directory, and set "
        "SIMMODSUITE_ROOT to the resulting <version>-<datestamp>/ directory.\n"
        "See docs/install.rst for full instructions.")
endif()

set(SIMMODSUITE_ROOT "$ENV{SIMMODSUITE_ROOT}" CACHE PATH "Simmetrix SimModSuite install root")

if(DEFINED ENV{SIMMODSUITE_ABI})
    set(SIMMODSUITE_ABI "$ENV{SIMMODSUITE_ABI}" CACHE STRING "Simmetrix ABI subdirectory")
else()
    set(SIMMODSUITE_ABI "x64_rhel9_gcc11" CACHE STRING "Simmetrix ABI subdirectory (default)")
endif()

set(SIMMODSUITE_INCLUDE "${SIMMODSUITE_ROOT}/include")
set(SIMMODSUITE_LIB "${SIMMODSUITE_ROOT}/lib/${SIMMODSUITE_ABI}")

if(NOT EXISTS "${SIMMODSUITE_INCLUDE}/SimUtil.h")
    message(FATAL_ERROR
        "Simmetrix headers not found at ${SIMMODSUITE_INCLUDE}/SimUtil.h. "
        "Check SIMMODSUITE_ROOT and that all four module tarballs are extracted "
        "into a unified install tree.")
endif()

if(NOT IS_DIRECTORY "${SIMMODSUITE_LIB}")
    message(FATAL_ERROR
        "Simmetrix library directory not found at ${SIMMODSUITE_LIB}. "
        "Set SIMMODSUITE_ABI to your install's ABI subdirectory "
        "(default: x64_rhel9_gcc11; alternatives: x64_rhel8_gcc83).")
endif()

# Glob-detect SimParasolid so version bumps (SimParasolid380 -> SimParasolid390 -> ...)
# do not require build changes.
file(GLOB SIM_PARASOLID_LIBS "${SIMMODSUITE_LIB}/libSimParasolid*.a")
list(LENGTH SIM_PARASOLID_LIBS SIM_PARASOLID_COUNT)
if(NOT SIM_PARASOLID_COUNT EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one libSimParasolid*.a in ${SIMMODSUITE_LIB}, "
        "found ${SIM_PARASOLID_COUNT}: ${SIM_PARASOLID_LIBS}")
endif()
get_filename_component(SIM_PARASOLID_NAME "${SIM_PARASOLID_LIBS}" NAME_WE)
string(REGEX REPLACE "^lib" "" SIM_PARASOLID_LIBNAME "${SIM_PARASOLID_NAME}")

add_library(sms INTERFACE)
target_include_directories(sms INTERFACE "${SIMMODSUITE_INCLUDE}")
target_link_directories(sms INTERFACE
    "${SIMMODSUITE_LIB}"
    "${SIMMODSUITE_LIB}/psKrnl")

set(SMS_LINK_LIBS
    ${SIM_PARASOLID_LIBNAME}
    SimPartitionedMesh
    SimPartitionWrapper
    SimMeshing
    SimMeshTools
    SimModel
    pskernel
    SimLicense)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SMS_LINK_LIBS tirpc)
endif()

target_link_libraries(sms INTERFACE ${SMS_LINK_LIBS})

# Compile-time default schema dir; init.cpp setenvs P_SCHEMA from this with
# overwrite=0 (preserves customer override).
target_compile_definitions(sms INTERFACE
    PSKRNL_SCHEMA_DIR="${SIMMODSUITE_LIB}/psKrnl/schema")

# ===============================================================================
# Gmsh
# ===============================================================================
find_library(GMSH_LIB gmsh)
if(NOT GMSH_LIB)
  message(FATAL_ERROR "Could not find libgmsh")
endif()
get_filename_component(GMSH_LIBDIR "${GMSH_LIB}" DIRECTORY)

find_path(GMSH_INC gmsh.h)
if(NOT GMSH_INC)
  message(FATAL_ERROR "Could not find gmsh.h")
endif()

include_directories(${GMSH_INC})


# ===============================================================================
# JSON
# ===============================================================================
find_package(nlohmann_json REQUIRED)

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
    src/basalt.cpp
)
target_include_directories(main PUBLIC ${SRC_DIR})
target_link_libraries(main PUBLIC
nanobind
sms
spdlog::spdlog
${GMSH_LIB}
nlohmann_json::nlohmann_json
)

# ===============================================================================
# Python Bindings
# ===============================================================================
file(GLOB PYTHON_SOURCES ${SRC_DIR}/python/*.cpp)
nanobind_add_module(_core NB_SHARED # STATIC is the default
        ${PYTHON_SOURCES})

# Each consumer sets CMAKE_INSTALL_PREFIX itself:
#   - dev pixi `build` task installs with --prefix .
#   - scikit-build-core installs into its temporary prefix and collects from there
#   - pixi-build-python (which routes through scikit-build-core) likewise
# $ORIGIN keeps _core.so finding sibling .so files inside basalt/_core/ regardless
# of where the wheel is installed.

# Inspect RPATH with: readelf -d basalt/_core/_core.cpython-*.so | grep RUNPATH
set_property(
        TARGET _core
        APPEND
        PROPERTY INSTALL_RPATH
        "$ORIGIN:${GMSH_LIBDIR}:${SIMMODSUITE_LIB}/psKrnl")

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
basalt.h
)
foreach (SOURCE_FILE ${GEN_FILES})
    get_filename_component(FILE_NAME ${SOURCE_FILE} NAME_WE)
    set(INPUT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/${SOURCE_FILE}")
    set(OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/autogen")
    set (OUTPUT_FILE "${OUTPUT_DIR}/bind_${FILE_NAME}.h")
    list(APPEND GENERATED_FILES ${OUTPUT_FILE})

    # nanobindgen does not create its output directory itself, and the sdist
    # excludes the gitignored `src/autogen/` tree, so make sure the directory
    # exists before invoking the generator.
    file(MAKE_DIRECTORY ${OUTPUT_DIR})

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
install(TARGETS _core nanobind DESTINATION "basalt/_core")
install(FILES ${CMAKE_BINARY_DIR}/_core.pyi DESTINATION "basalt/_core" OPTIONAL)

cmake_minimum_required(VERSION 3.15)

project(simmetrix)

add_library(sms INTERFACE)

# find_package(VTK REQUIRED)
find_package(OpenCASCADE REQUIRED)

find_package(Python COMPONENTS Interpreter Development)
execute_process(
        COMMAND "${Python_EXECUTABLE}" -m pybind11 --cmakedir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        OUTPUT_VARIABLE PYBIND11_DIR)
list(APPEND CMAKE_PREFIX_PATH "${PYBIND11_DIR}")
find_package(pybind11 CONFIG REQUIRED)

 set(OCCT_LIBS
        TKXDESTEP
        TKSTEP
        TKSTEP209
        TKSTEPAttr
        TKSTEPBase
        TKXCAF
        TKXSBase
        TKVCAF
        TKCAF
        TKLCAF
        TKCDF
        TKV3d
        TKService
        TKMesh
        TKBO
        TKPrim
        TKHLR
        TKShHealing
        TKTopAlgo
        TKGeomAlgo
        TKBRep
        TKGeomBase
        TKG3d
        TKG2d
        TKMath
        TKernel

        TKOffset
        TKIGES
        TKXml
        TKXmlXCAF
    )


target_include_directories(sms INTERFACE include/gmcore)
target_link_directories(sms INTERFACE lib/gmcore)

target_include_directories(sms INTERFACE include/mscore)
target_link_directories(sms INTERFACE lib/mscore)

target_include_directories(sms INTERFACE include/pskrnl)
target_link_directories(sms INTERFACE lib/pskrnl lib/pskrnl/psKrnl)

target_include_directories(sms INTERFACE include/occ)
target_link_directories(sms INTERFACE lib/occ)

# Manually add the include directories from the provided variable.
target_include_directories(sms INTERFACE ${OpenCASCADE_INCLUDE_DIRS})

target_link_libraries(sms INTERFACE SimOpenCascade77 SimParasolid371 SimPartitionedMesh SimPartitionWrapper SimMeshing SimMeshTools SimModel pskernel tirpc)

target_link_libraries(sms INTERFACE ${OCCT_LIBS})






add_library(
    main
    src/Mesh.cpp
)
target_link_libraries(main PUBLIC sms)


pybind11_add_module(smspy bindings/sms.cpp)
target_link_libraries(smspy PUBLIC sms)
target_include_directories(smspy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB EXAMPLE_SOURCES examples/*)
foreach(example_file ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${example_file} NAME_WE)
    add_executable(${example_name} ${example_file})
    target_link_libraries(${example_name} PRIVATE main)
endforeach()
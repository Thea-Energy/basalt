cmake_minimum_required(VERSION 3.15)

project(simmetrix)

add_library(sms INTERFACE)


target_include_directories(sms INTERFACE include/gmcore)
target_link_directories(sms INTERFACE lib/gmcore)

target_include_directories(sms INTERFACE include/mscore)
target_link_directories(sms INTERFACE lib/mscore)

target_include_directories(sms INTERFACE include/pskrnl)
target_link_directories(sms INTERFACE lib/pskrnl lib/pskrnl/psKrnl)

target_link_libraries(sms INTERFACE SimParasolid371 SimPartitionedMesh SimPartitionWrapper SimMeshing SimMeshTools SimModel pskernel tirpc)

add_library(
    main
    src/Mesh.cpp
)
target_link_libraries(main PUBLIC sms)

file(GLOB EXAMPLE_SOURCES examples/*)
foreach(example_file ${EXAMPLE_SOURCES})
    get_filename_component(example_name ${example_file} NAME_WE)
    add_executable(${example_name} ${example_file})
    target_link_libraries(${example_name} PRIVATE main)
endforeach()
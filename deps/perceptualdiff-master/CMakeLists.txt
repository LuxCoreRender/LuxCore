project(perceptualdiff)

cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT MSVC)
    add_compile_options(
        -Wall -Wextra -pedantic -Wcast-qual -Wpointer-arith -Winit-self
        -Wswitch-default -Wmissing-include-dirs -Wold-style-cast
        -Wnon-virtual-dtor -Wshadow -Wno-unknown-pragmas)
endif()

find_package(OpenMP)
if(OPENMP_FOUND)
    add_compile_options(${OpenMP_CXX_FLAGS})
    if(NOT MSVC)
        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
    endif()
endif()

option(SANITIZERS "Enable sanitizers" FALSE)
if(SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR})
set(FREEIMAGE_FIND_REQUIRED TRUE)
find_package(FreeImage)

add_library(pdiff lpyramid.cpp rgba_image.cpp metric.cpp)
target_include_directories(pdiff SYSTEM PRIVATE ${FREEIMAGE_INCLUDE_DIRS})
target_link_libraries(pdiff PRIVATE ${FREEIMAGE_LIBRARIES})

add_executable(perceptualdiff compare_args.cpp perceptualdiff.cpp)
target_link_libraries(perceptualdiff PRIVATE pdiff)

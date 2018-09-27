###########################################################################
#
# Configuration ( Jens Verwiebe )
#
###########################################################################

MESSAGE(STATUS "Using OSX Configuration settings")

# Allow for the location of OSX_DEPENDENCY_ROOT to be set from the command line
IF( NOT OSX_DEPENDENCY_ROOT )
  set(OSX_DEPENDENCY_ROOT ${CMAKE_SOURCE_DIR}/../macos) # can be macos or usr/local for example
ENDIF()

MESSAGE(STATUS "OSX_DEPENDENCY_ROOT : " ${OSX_DEPENDENCY_ROOT})
set(OSX_SEARCH_PATH     ${OSX_DEPENDENCY_ROOT})

# Libs present in system ( /usr )
SET(SYS_LIBRARIES z )

find_package(PythonLibs 3.5 REQUIRED)

#Find pyside2-uic so that .ui files rebuild pyside deps
find_program(PYSIDE_UIC
             NAME pyside2-uic
             HINTS /usr/local/bin/)

# Libs that have find_package modules
set(OPENIMAGEIO_ROOT_DIR "${OSX_SEARCH_PATH}")

set(BOOST_SEARCH_PATH         "${OSX_SEARCH_PATH}")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/lib")

set(OPENCL_SEARCH_PATH        "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/opencl.framework")
set(OPENCL_INCLUDE_DIR       "${OSX_SEARCH_PATH}/include")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}")

set(EMBREE_SEARCH_PATH			"${OSX_SEARCH_PATH}")

set(TBB_SEARCH_PATH "${OSX_SEARCH_PATH}")
set(BLOSC_SEARCH_PATH "${OSX_SEARCH_PATH}")

find_library(OPENMP_LIB libiomp5.dylib HINTS ${OSX_SEARCH_PATH}/lib)

if(NOT OPENMP_LIB)
    message(FATAL_ERROR "OpenMP library not found")
else()
    set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I/${OSX_SEARCH_PATH}/include")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${OPENMP_LIB} -liomp5")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${OPENMP_LIB} -liomp5")
endif()

set(GLEW_SEARCH_PATH          "${OSX_SEARCH_PATH}")
find_path(GLEW_INCLUDE_DIR glew.h PATHS ${OSX_SEARCH_PATH}/include/GL )
find_library(GLEW_LIBRARY libGLEW.a PATHS ${OSX_SEARCH_PATH}/lib )
set(GLEW_FOUND 1)

set(GLUT_SEARCH_PATH          "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/glut.framework")
set(GLUT_INCLUDE_PATH 		"${GLUT_SEARCH_PATH}/Headers")

SET(OPENEXR_ROOT "${OSX_SEARCH_PATH}")

# Libs with hardcoded pathes ( macos repo )
SET(TIFF_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/libtiff.a ${OSX_DEPENDENCY_ROOT}/lib/liblzma.a)
SET(TIFF_INCLUDE_DIR ${OSX_DEPENDENCY_ROOT}/include/tiff)
SET(TIFF_FOUND ON)
SET(JPEG_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/libjpeg.a)
SET(JPEG_INCLUDE_DIR ${OSX_DEPENDENCY_ROOT}/include/jpeg)
SET(JPEG_FOUND ON)
SET(PNG_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/libpng.a ${SYS_LIBRARIES})
SET(PNG_INCLUDE_DIR ${OSX_DEPENDENCY_ROOT}/include/png)
SET(PNG_FOUND ON)
SET(EMBREE_LIBRARY ${OSX_DEPENDENCY_ROOT}/lib/libembree3.dylib)
SET(EMBREE_INCLUDE_PATH ${OSX_DEPENDENCY_ROOT}/include/embree3)
SET(EMBREE_FOUND ON)

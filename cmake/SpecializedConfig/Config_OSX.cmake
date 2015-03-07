
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

# Libs that have find_package modules
set(OPENIMAGEIO_ROOT_DIR "${OSX_SEARCH_PATH}")

set(BOOST_SEARCH_PATH         "${OSX_SEARCH_PATH}")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/lib")

set(OPENCL_SEARCH_PATH        "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/opencl.framework")
set(OPENCL_INCLUDE_PATH       "${OPENCL_SEARCH_PATH}")
#set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}")

set(GLEW_SEARCH_PATH          "${OSX_SEARCH_PATH}")
find_path(GLEW_INCLUDE_DIR glew.h PATHS ${OSX_SEARCH_PATH}/include/GL )
find_library(GLEW_LIBRARY libGLEW.a PATHS ${OSX_SEARCH_PATH}/lib )
set(GLEW_FOUND 1)

set(GLUT_SEARCH_PATH          "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/glut.framework")
set(GLUT_INCLUDE_PATH 		"${GLUT_SEARCH_PATH}/Headers")
#set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}")

SET(OPENEXR_ROOT "${OSX_SEARCH_PATH}")

# Libs with hardcoded pathes ( macos repo )
SET(TIFF_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/libtiff.a)
SET(TIFF_INCLUDE_DIR ${OSX_DEPENDENCY_ROOT}/include/tiff)
SET(TIFF_FOUND ON)
SET(JPEG_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/libjpeg.a)
SET(JPEG_INCLUDE_DIR ${OSX_DEPENDENCY_ROOT}/include/jpeg)
SET(JPEG_FOUND ON)
SET(PNG_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/libpng14.a ${SYS_LIBRARIES})
SET(PNG_INCLUDE_DIR ${OSX_DEPENDENCY_ROOT}/include/png)
SET(PNG_FOUND ON)
SET(EMBREE_LIBRARY ${OSX_DEPENDENCY_ROOT}/lib/embree2/libembree.2.4.0.dylib)
SET(EMBREE_INCLUDE_PATH ${OSX_DEPENDENCY_ROOT}/include/embree2)
SET(EMBREE_FOUND ON)

# use Blender python libs for static compiling !
SET(PYTHON_LIBRARIES ${OSX_DEPENDENCY_ROOT}/lib/BF_pythonlibs/py34_intel64/libbf_python_ext.a ${OSX_DEPENDENCY_ROOT}/lib/BF_pythonlibs/py34_intel64/libbf_python.a)
SET(PYTHON_INCLUDE_DIRS ${OSX_DEPENDENCY_ROOT}/include/Python3.4m)
SET(PYTHONLIBS_FOUND ON)
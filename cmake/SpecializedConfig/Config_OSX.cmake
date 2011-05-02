
###########################################################################
#
# Configuration ( Jens Verwiebe )
#
###########################################################################

MESSAGE(STATUS "Using OSX Configuration settings")

set(OSX_SEARCH_PATH     "../macos")

set(FREEIMAGE_SEARCH_PATH "${OSX_SEARCH_PATH}")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/include")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/lib")
set(FREEIMAGE_INCLUDE_PATH "${FREEIMAGE_SEARCH_PATH}/include")
set(FREEIMAGE_LIBRARY "${FREEIMAGE_SEARCH_PATH}/lib")

set(BOOST_SEARCH_PATH         "${OSX_SEARCH_PATH}")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/lib")

set(OPENCL_SEARCH_PATH        "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/opencl.framework")
set(OPENCL_INCLUDE_PATH       "${OPENCL_SEARCH_PATH}")
#set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}")

set(GLEW_SEARCH_PATH          "${OSX_SEARCH_PATH}")
find_path(GLEW_INCLUDE_DIR glew.h PATHS ${OSX_SEARCH_PATH}//include/GL )
find_library(GLEW_LIBRARY libGLEW.a PATHS ${OSX_SEARCH_PATH}//lib )
set(GLEW_FOUND 1)

set(GLUT_SEARCH_PATH          "${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/glut.framework")
set(GLUT_INCLUDE_PATH 		"${GLUT_SEARCH_PATH}/Headers")
#set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}")


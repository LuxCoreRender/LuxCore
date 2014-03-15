
###########################################################################
#
# Configuration
#
###########################################################################

MESSAGE(STATUS "Using Karoly Zsolnai's Configuration settings")


SET ( DEPS_HOME               "D:/lux/deps" )
SET ( DEPS_BITS               "x86" )
SET ( DEPS_ROOT               "${DEPS_HOME}/${DEPS_BITS}")

SET ( ENV{QTDIR}              "${DEPS_ROOT}/qt-everywhere-opensource-src-4.7.2")
SET ( ENV{LuxRays_HOME}       "${lux_SOURCE_DIR}/../luxrays")

set(FREEIMAGE_SEARCH_PATH     "${DEPS_ROOT}/FreeImage3141/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/source")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/release"
                              "${FREEIMAGE_SEARCH_PATH}/debug"
                              "${FREEIMAGE_SEARCH_PATH}/dist")
ADD_DEFINITIONS(-DFREEIMAGE_LIB)

SET ( ENV{OpenEXR_HOME}       "${FREEIMAGE_SEARCH_PATH}/Source/OpenEXR")
SET ( OpenEXR_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Source/OpenEXR")


SET ( ENV{PNG_HOME}           "${FREEIMAGE_SEARCH_PATH}/Source/LibPNG")
SET ( PNG_INC_SEARCH_PATH     "${FREEIMAGE_SEARCH_PATH}/Source/LibPNG")

set(BOOST_SEARCH_PATH         "${DEPS_ROOT}/boost_1_47_0")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/boost/lib")

#set(OPENCL_SEARCH_PATH        "$ENV{AMDAPPSDKROOT}")
set(OPENCL_SEARCH_PATH        "c:/CUDA/v4.1")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/Win32")
#set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86_64")

set(GLUT_SEARCH_PATH          "${DEPS_ROOT}/freeglut-2.6.0")
#set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/VisualStudio2008Static/x64/Release")
set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/VisualStudio2008Static/Win32/Release")
ADD_DEFINITIONS(-DFREEGLUT_STATIC)

set(GLEW_ROOT                 "${DEPS_ROOT}/glew-1.5.5")
set(GLEW_SEARCH_PATH          "${DEPS_ROOT}/glew-1.5.5")
ADD_DEFINITIONS(-DGLEW_STATIC)

INCLUDE_DIRECTORIES ( SYSTEM "${FREEIMAGE_SEARCH_PATH}/Source/Zlib" )
#INCLUDE_DIRECTORIES ( SYSTEM "${DEPS_HOME}/include" ) # for unistd.h
INCLUDE_DIRECTORIES ( SYSTEM "d:/lux/windows/include" ) # for unistd.h

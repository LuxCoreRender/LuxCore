
###########################################################################
#
# Configuration
#
###########################################################################

MESSAGE(STATUS "Using LordCrc's Configuration settings")

set(BUILD_LUXMARK FALSE) # This will require QT

set(ENV{QTDIR} "E:/Dev/Lux2010/deps_2010/x64/qt-everywhere-opensource-src-4.7.2")

set(FREEIMAGE_SEARCH_PATH     "E:/Dev/Lux2010/deps_2010/x64/FreeImage3150/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/source")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/release"
                              "${FREEIMAGE_SEARCH_PATH}/debug"
                              "${FREEIMAGE_SEARCH_PATH}/dist")
ADD_DEFINITIONS(-DFREEIMAGE_LIB)

set(BOOST_SEARCH_PATH         "E:/Dev/Lux2010/deps_2010/x64/boost_1_43_0")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/boost/lib")

set(OPENCL_SEARCH_PATH        "$ENV{ATISTREAMSDKROOT}")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86_64")

set(GLUT_SEARCH_PATH          "E:/Dev/Lux2010/deps_2010/x64/freeglut-2.6.0")
set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/VisualStudio2008Static/x64/Release")
ADD_DEFINITIONS(-DFREEGLUT_STATIC)

set(GLEW_SEARCH_PATH          "E:/Dev/Lux2010/deps_2010/x64/glew-1.5.5")
ADD_DEFINITIONS(-DGLEW_STATIC)

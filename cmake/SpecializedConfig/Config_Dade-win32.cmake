
###########################################################################
#
# Configuration
#
###########################################################################

# LUXRAYS_CUSTOM_CONFIG
# C:/Users/David/Dati/OpenCL/LuxRays-32bit/luxrays/cmake/SpecializedConfig/Config_Dade-win32.cmake
# 
# Boost:
#  bjam --toolset=msvc --stagedir=stage stage -j 8


MESSAGE(STATUS "Using Dade's Win32 Configuration settings")

set(CMAKE_BUILD_TYPE "Release")

set(BUILD_LUXMARK TRUE) # This will require QT

set(ENV{QTDIR} "C:/Qt/4.7.1")

set(FREEIMAGE_SEARCH_PATH "C:/Users/David/Dati/OpenCL/LuxRays-32bit/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FREEIMAGE_ROOT_DIR "${FREEIMAGE_SEARCH_PATH}/Dist")

set(BOOST_SEARCH_PATH         "C:/Users/David/Dati/OpenCL/LuxRays-32bit/boost_1_43_0")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/boost/lib")

set(OPENCL_SEARCH_PATH        "C:/Program Files (x86)/AMD APP")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86")

set(GLUT_SEARCH_PATH          "C:/Users/David/Dati/OpenCL/LuxRays-32bit/freeglut-2.6.0")
set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/VisualStudio2008Static/Release")
ADD_DEFINITIONS(-DFREEGLUT_STATIC)

set(GLEW_SEARCH_PATH          "C:/Users/David/Dati/OpenCL/LuxRays-32bit/glew-1.6.0")
ADD_DEFINITIONS(-DGLEW_BUILD)





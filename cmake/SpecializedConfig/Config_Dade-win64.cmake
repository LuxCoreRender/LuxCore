
###########################################################################
#
# Configuration
#
###########################################################################

# LUXRAYS_CUSTOM_CONFIG
# C:/Users/David/Dati/OpenCL/LuxRays-64bit/luxrays/cmake/SpecializedConfig/Config_Dade-win64.cmake
# 
# Boost:
#  "C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\bin\vcvars64.bat"
#  bjam --toolset=msvc address-model=64 --stagedir=stage stage -j 8


MESSAGE(STATUS "Using Dade's Win64 Configuration settings")

set(CMAKE_BUILD_TYPE "Release")

set(BUILD_LUXMARK TRUE) # This will require QT

set(ENV{QTDIR} "C:/Qt/4.7.1")

set(FREEIMAGE_SEARCH_PATH "C:/Users/David/Dati/OpenCL/LuxRays-64bit/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")

set(BOOST_SEARCH_PATH         "C:/Users/David/Dati/OpenCL/LuxRays-64bit/boost_1_43_0")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/boost/lib")

set(OPENCL_SEARCH_PATH        "C:/Program Files (x86)/AMD APP")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86_64")

set(GLUT_SEARCH_PATH          "C:/Users/David/Dati/OpenCL/LuxRays-64bit/freeglut-2.6.0")
set(GLUT_LIBRARYDIR           "${GLUT_SEARCH_PATH}/VisualStudio2008Static/x64/Release")
ADD_DEFINITIONS(-DFREEGLUT_STATIC)

set(GLEW_SEARCH_PATH          "C:/Users/David/Dati/OpenCL/LuxRays-64bit/glew-1.6.0")
ADD_DEFINITIONS(-DGLEW_BUILD)


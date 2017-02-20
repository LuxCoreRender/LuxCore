###########################################################################
#
# Configuration
#
###########################################################################

# LUXRAYS_CUSTOM_CONFIG
# E:/projects/luxrays-32bit/luxrays/cmake/SpecializedConfig/Config_Dade-win32.cmake
# 
# Boost:
#  bjam --toolset=msvc --stagedir=stage stage -j 8
#
# To disable OpenMP support (VCOMP.lib error) in Visula Studio 2010 go
# properties => C/C++ => languages and disable OpenMP

MESSAGE(STATUS "Using Dade's Win32 Configuration settings")

set(CMAKE_BUILD_TYPE "Release")

set(ENV{QTDIR} "C:/Qt/4.7.1")

set(FREEIMAGE_SEARCH_PATH "E:/projects/luxrays-32bit/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FREEIMAGE_ROOT_DIR "${FREEIMAGE_SEARCH_PATH}/Dist")

set(BOOST_SEARCH_PATH         "E:/projects/luxrays-32bit/boost_1_48_0")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/boost/lib")

set(OPENCL_SEARCH_PATH        "C:/Program Files (x86)/AMD APP")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86")

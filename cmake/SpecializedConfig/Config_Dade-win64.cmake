###########################################################################
#
# Configuration
#
###########################################################################

# LUXRAYS_CUSTOM_CONFIG
# E:/projects/luxrays-64bit/luxrays/cmake/SpecializedConfig/Config_Dade-win64.cmake
# 
# Boost:
#  CALL "C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x64
#  bootstrap.bat
#  (with Python) path %PATH%;c:\Python27
#  (with Python) bjam --with-python --toolset=msvc address-model=64 --stagedir=stage stage -j 12
#  (without Python) bjam --toolset=msvc address-model=64 --stagedir=stage stage -j 12
#
# To disable OpenMP support (VCOMP.lib error) in Visula Studio 2010 go
# properties => C/C++ => languages and disable OpenMP

MESSAGE(STATUS "Using Dade's Win64 Configuration settings")

set(CMAKE_BUILD_TYPE "Release")

set(ENV{QTDIR} "C:/Qt/4.7.1")

set(FREEIMAGE_SEARCH_PATH "E:/projects/luxrays-64bit/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FREEIMAGE_ROOT_DIR "${FREEIMAGE_SEARCH_PATH}/Dist")

#set(Boost_DEBUG 1)
set(BOOST_SEARCH_PATH         "E:/projects/luxrays-64bit/boost_1_48_0-python26")
set(BOOST_LIBRARYDIR          "${BOOST_SEARCH_PATH}/stage/lib")

set(OPENCL_SEARCH_PATH        "C:/Program Files (x86)/AMD APP")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86")

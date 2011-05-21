
###########################################################################
#
# Configuration
#
###########################################################################

# LUXRAYS_CUSTOM_CONFIG
# C:/Users/David/Dati/OpenCL/luxrays/cmake/SpecializedConfig/Config_Dade-win32.cmake


MESSAGE(STATUS "Using Dade's Win32 Configuration settings")

set(CMAKE_BUILD_TYPE "Release")

set(BUILD_LUXMARK TRUE) # This will require QT

set(ENV{QTDIR} "C:/Qt/4.7.1")

set(FREEIMAGE_SEARCH_PATH "C:/Users/David/Dati/OpenCL/VCProjects/FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/Dist")

set(BOOST_SEARCH_PATH         "C:/Program Files (x86)/boost/boost_1_43")

set(OPENCL_SEARCH_PATH        "C:/Program Files (x86)/AMD APP")
set(OPENCL_LIBRARYDIR         "${OPENCL_SEARCH_PATH}/lib/x86")

set(GLUT_SEARCH_PATH          "C:/Users/David/Dati/OpenCL/glut-3.7.6-bin")

set(GLEW_SEARCH_PATH          "C:/Users/David/Dati/OpenCL/VCProjects/glew-1.5.5")
ADD_DEFINITIONS(-DGLEW_BUILD)




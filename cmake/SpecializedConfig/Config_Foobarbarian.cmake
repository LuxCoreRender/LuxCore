
###########################################################################
#
# Configuration
#
###########################################################################

MESSAGE(STATUS "Using Foobarbarian Configuration settings")

set(BUILD_LUXMARK TRUE) # This will require QT

set(ENV{QTDIR} "c:/qt/4.7.2")

set(FREEIMAGE_SEARCH_PATH     "${LuxRays_SOURCE_DIR}/../FreeImage")
set(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/source")
set(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/release"
                              "${FREEIMAGE_SEARCH_PATH}/debug"
                              "${FREEIMAGE_SEARCH_PATH}/dist")

set(BOOST_SEARCH_PATH         "${LuxRays_SOURCE_DIR}/../boost")
set(OPENCL_SEARCH_PATH        "${LuxRays_SOURCE_DIR}/../opencl")
set(GLUT_SEARCH_PATH          "${LuxRays_SOURCE_DIR}/../freeglut")
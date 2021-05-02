###########################################################################
#
# Configuration
#
###########################################################################

MESSAGE(STATUS "Using Windows Configuration settings")

IF(MSVC)

  STRING(REGEX MATCH "(Win64)" _carch_x64 ${CMAKE_GENERATOR})
  IF(_carch_x64)
    SET(WINDOWS_ARCH "x64")
  ELSE(_carch_x64)
    SET(WINDOWS_ARCH "x86")
  ENDIF(_carch_x64)
  MESSAGE(STATUS "Building for target ${WINDOWS_ARCH}")

  IF(DEFINED ENV{LUX_WINDOWS_BUILD_ROOT} AND DEFINED ENV{LIB_DIR})
    MESSAGE(STATUS "Lux build environment variables found")
    MESSAGE(STATUS "  LUX_WINDOWS_BUILD_ROOT = $ENV{LUX_WINDOWS_BUILD_ROOT}")
    MESSAGE(STATUS "  INCLUDE_DIR = $ENV{INCLUDE_DIR}")
    MESSAGE(STATUS "  LIB_DIR = $ENV{LIB_DIR}")

    SET(BISON_EXECUTABLE        "$ENV{LUX_WINDOWS_BUILD_ROOT}/support/bin/win_bison.exe")
    SET(FLEX_EXECUTABLE         "$ENV{LUX_WINDOWS_BUILD_ROOT}/support/bin/win_flex.exe")
    SET(OPENIMAGEIO_INCLUDE_DIR "$ENV{INCLUDE_DIR}/OpenImageIO")
    SET(OPENEXR_ROOT            "$ENV{INCLUDE_DIR}/OpenEXR")
    SET(BOOST_SEARCH_PATH       "$ENV{INCLUDE_DIR}/Boost")
    SET(Boost_USE_STATIC_LIBS   ON)
    SET(BOOST_LIBRARYDIR        "$ENV{LIB_DIR}")

    ADD_DEFINITIONS(-DOIIO_STATIC_DEFINE)
    #ADD_DEFINITIONS(-DFREEIMAGE_LIB)

  ENDIF()

ELSE(MSVC)

  SET(ENV{QTDIR} "c:/qt/")

  SET(FREEIMAGE_SEARCH_PATH     "${LuxCoreRender_SOURCE_DIR}/../FreeImage")
  SET(BOOST_SEARCH_PATH         "${LuxCoreRender_SOURCE_DIR}/../boost")

ENDIF(MSVC)

SET(FreeImage_INC_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/source")
SET(FreeImage_LIB_SEARCH_PATH "${FREEIMAGE_SEARCH_PATH}/release"
                              "${FREEIMAGE_SEARCH_PATH}/debug"
                              "${FREEIMAGE_SEARCH_PATH}/dist")

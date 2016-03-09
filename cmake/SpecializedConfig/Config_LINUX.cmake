
###########################################################################
#
# Configuration ( Jens Verwiebe )
#
###########################################################################

#cmake -DLUX_CUSTOM_CONFIG=cmake/SpecializedConfig/Config_LINUX.cmake

MESSAGE(STATUS "Using LINUX Configuration settings")

# Allow for the location of LINUX_DEPENDENCY_ROOT to be set from the command line
IF( NOT LINUX_DEPENDENCY_ROOT )
  set(LINUX_DEPENDENCY_ROOT ${CMAKE_SOURCE_DIR}/../linux_deps)
ENDIF()

MESSAGE(STATUS "LINUX_DEPENDENCY_ROOT : " ${LINUX_DEPENDENCY_ROOT})
set(LINUX_SEARCH_PATH     ${LINUX_DEPENDENCY_ROOT})

# Libs that have find_package modules
set(OPENIMAGEIO_ROOT_DIR "${LINUX_SEARCH_PATH}")
set(OPENEXR_ROOT "${LINUX_SEARCH_PATH}")
set(BOOST_SEARCH_PATH         "${LINUX_SEARCH_PATH}")




#SET(PYTHON_LIBRARY "${LINUX_SEARCH_PATH}/lib/libpython3.4a")
#SET(PYTHON_INCUDE_DIR "/usr/include/python3.4m")

# Libs with hardcoded pathes ( linux repo )

SET(TIFF_LIBRARY ${LINUX_DEPENDENCY_ROOT}/lib/libtiff.a)
SET(TIFF_INCLUDE_DIR ${LINUX_DEPENDENCY_ROOT}/include)
SET(TIFF_FOUND ON)
SET(JPEG_LIBRARY ${LINUX_DEPENDENCY_ROOT}/lib/libjpeg.a)
SET(JPEG_INCLUDE_DIR ${LINUX_DEPENDENCY_ROOT}/include)
SET(JPEG_FOUND ON)
SET(ZLIB_INCLUDE_DIR ${LINUX_DEPENDENCY_ROOT}/include)
SET(ZLIB_LIBRARY ${LINUX_DEPENDENCY_ROOT}/lib/libz.a)
SET(ZLIB_FOUND ON)
SET(PNG_LIBRARY_RELEASE ${LINUX_DEPENDENCY_ROOT}/lib/libpng.a)
SET(PNG_PNG_INCLUDE_DIR ${LINUX_DEPENDENCY_ROOT}/include/png)
SET(PNG_FOUND ON)


# - Find JPEG library


# If JPEG_ROOT was defined in the environment, use it.
IF(NOT JPEG_ROOT AND NOT $ENV{JPEG_ROOT} STREQUAL "")
  SET(JPEG_ROOT $ENV{JPEG_ROOT})
ENDIF()

SET(_jpeg_SEARCH_DIRS
  ${JPEG_ROOT}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(JPEG_INCLUDE_DIR
  NAMES
    jconfig.h
  HINTS
    ${_jpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    include
    include/jpeg-1.5
)

FIND_LIBRARY(JPEG_LIBRARY
  NAMES
    jpeg
  HINTS
    ${_jpeg_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set JPEG_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JPEG DEFAULT_MSG
    JPEG_LIBRARY JPEG_INCLUDE_DIR)

IF(JPEG_FOUND)
  SET(JPEG_LIBRARIES ${JPEG_LIBRARY})
  SET(JPEG_INCLUDE_DIRS ${JPEG_INCLUDE_DIR})
ENDIF(JPEG_FOUND)

MARK_AS_ADVANCED(
  JPEG_INCLUDE_DIR
  JPEG_LIBRARY
)

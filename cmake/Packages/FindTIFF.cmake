# - Find TIFF library


# If TIFF_ROOT was defined in the environment, use it.
IF(NOT TIFF_ROOT AND NOT $ENV{TIFF_ROOT} STREQUAL "")
  SET(TIFF_ROOT $ENV{TIFF_ROOT})
ENDIF()

SET(_tiff_SEARCH_DIRS
  ${TIFF_ROOT}
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
)

FIND_PATH(TIFF_INCLUDE_DIR
  NAMES
    tiff.h
  HINTS
    ${_tiff_SEARCH_DIRS}
  PATH_SUFFIXES
    include
    include/tiff
)

FIND_LIBRARY(TIFF_LIBRARY
  NAMES
    tiff
  HINTS
    ${_tiff_SEARCH_DIRS}
  PATH_SUFFIXES
    lib64 lib
  )

# handle the QUIETLY and REQUIRED arguments and set TIFF_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TIFF DEFAULT_MSG
    TIFF_LIBRARY TIFF_INCLUDE_DIR)

IF(TIFF_FOUND)
  SET(TIFF_LIBRARIES ${TIFF_LIBRARY})
  SET(TIFF_INCLUDE_DIRS ${TIFF_INCLUDE_DIR})
ENDIF(TIFF_FOUND)

MARK_AS_ADVANCED(
  TIFF_INCLUDE_DIR
  TIFF_LIBRARY
)

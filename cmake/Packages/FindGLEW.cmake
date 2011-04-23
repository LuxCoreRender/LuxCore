#
# From: http://code.google.com/p/nvidia-texture-tools/source/browse/trunk/cmake/FindGLEW.cmake?r=96 
#
# Try to find GLEW library and include path.
# Once done this will define
#
# GLEW_FOUND
# GLEW_INCLUDE_PATH
# GLEW_LIBRARY
# 

IF (WIN32)
	FIND_PATH( GLEW_INCLUDE_PATH GL/glew.h
		${GLEW_ROOT}/include
		$ENV{PROGRAMFILES}/GLEW/include
		${LuxRays_SOURCE_DIR}/../glew/include
		${PROJECT_SOURCE_DIR}/src/nvgl/glew/include
		DOC "The directory where GL/glew.h resides")
	FIND_LIBRARY( GLEW_LIBRARY
		NAMES glew GLEW glew32 glew32s
		PATHS
		${GLEW_LIBRARYDIR}
		${GLEW_ROOT}/lib
		$ENV{PROGRAMFILES}/GLEW/lib
		${LuxRays_SOURCE_DIR}/../glew/lib
		${PROJECT_SOURCE_DIR}/src/nvgl/glew/bin
		${PROJECT_SOURCE_DIR}/src/nvgl/glew/lib
		DOC "The GLEW library")
ELSE (WIN32)
	FIND_PATH( GLEW_INCLUDE_PATH GL/glew.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		DOC "The directory where GL/glew.h resides")
	FIND_LIBRARY( GLEW_LIBRARY
		NAMES GLEW glew
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The GLEW library")
ENDIF (WIN32)

IF (GLEW_INCLUDE_PATH)
	SET( GLEW_FOUND 1 CACHE STRING "Set to 1 if GLEW is found, 0 otherwise")
ELSE (GLEW_INCLUDE_PATH)
	SET( GLEW_FOUND 0 CACHE STRING "Set to 1 if GLEW is found, 0 otherwise")
ENDIF (GLEW_INCLUDE_PATH)

MARK_AS_ADVANCED( GLEW_FOUND )
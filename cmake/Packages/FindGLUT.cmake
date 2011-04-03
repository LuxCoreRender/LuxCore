#
# From: http://code.google.com/p/nvidia-texture-tools/source/browse/trunk/cmake/FindGLEW.cmake?r=96 
#
# Try to find GLUT library and include path.
# Once done this will define
#
# GLUT_FOUND
# GLUT_INCLUDE_PATH
# GLUT_LIBRARY
# 

IF (WIN32)
	FIND_PATH( GLUT_INCLUDE_PATH GL/glut.h
		$ENV{PROGRAMFILES}/GLUT/include
		${LuxRays_SOURCE_DIR}/../freeglut/include
		${LuxRays_SOURCE_DIR}/../glut/include
		DOC "The directory where GL/glew.h resides")
	FIND_LIBRARY( GLUT_LIBRARY
		NAMES glut GLUT glut32 glut32s freeglut freeglut_static
		PATHS
		$ENV{PROGRAMFILES}/GLUT/lib
		${LuxRays_SOURCE_DIR}/../freeglut/lib
		${LuxRays_SOURCE_DIR}/../glut/lib
		DOC "The GLUT library")
ELSE (WIN32)
	FIND_PATH( GLUT_INCLUDE_PATH GL/glut.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		DOC "The directory where GL/glut.h resides")
	FIND_LIBRARY( GLUT_LIBRARY
		NAMES glut GLUT freeglut freeglut32 freeglut_static
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The GLUT library")
ENDIF (WIN32)

IF (GLUT_INCLUDE_PATH)
	SET( GLUT_FOUND 1 CACHE STRING "Set to 1 if GLUT is found, 0 otherwise")
ELSE (GLUT_INCLUDE_PATH)
	SET( GLUT_FOUND 0 CACHE STRING "Set to 1 if GLUT is found, 0 otherwise")
ENDIF (GLUT_INCLUDE_PATH)

MARK_AS_ADVANCED( GLUT_FOUND )
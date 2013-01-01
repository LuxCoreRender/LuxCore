###########################################################################
#   Copyright (C) 1998-2011 by authors (see AUTHORS.txt )                 #
#                                                                         #
#   This file is part of LuxRays.                                         #
#                                                                         #
#   LuxRays is free software; you can redistribute it and/or modify       #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 3 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   LuxRays is distributed in the hope that it will be useful,            #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program.  If not, see <http://www.gnu.org/licenses/>. #
#                                                                         #
#   LuxRays website: http://www.luxrender.net                             #
###########################################################################


###########################################################################
#
# Generate LuxRays kernel files
#
###########################################################################

set(LUXRAYS_KERNELS
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/bvh.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/qbvh.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/mqbvh.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sampler/sampler_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/filter_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/camera_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/trianglemesh_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/trianglemesh_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/matrix4x4_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/transform_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/transform_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/mc_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/frame_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/frame_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/bsdf_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/bsdf_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/core/epsilon_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum_types.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum_funcs.cl
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/material_types.cl
)

PreprocessOCLKernels(${LuxRays_SOURCE_DIR}/src/kernels luxrays ${LUXRAYS_KERNELS})

###########################################################################
#
# Library project files
#
###########################################################################

	set(LUXRAYS_SRCS_ACCELERATORS
	${LuxRays_SOURCE_DIR}/src/accelerators/bvhaccel.cpp
	${LuxRays_SOURCE_DIR}/src/accelerators/mqbvhaccel.cpp
	${LuxRays_SOURCE_DIR}/src/accelerators/qbvhaccel.cpp
)
SOURCE_GROUP("Source Files\\Accelerators" FILES ${LUXRAYS_SRCS_ACCELERATORS})
	
set(LUXRAYS_SRCS_CORE
	${LuxRays_SOURCE_DIR}/src/core/context.cpp
	${LuxRays_SOURCE_DIR}/src/core/dataset.cpp
	${LuxRays_SOURCE_DIR}/src/core/device.cpp
	${LuxRays_SOURCE_DIR}/src/core/epsilon.cpp
	${LuxRays_SOURCE_DIR}/src/core/trianglemesh.cpp
)
SOURCE_GROUP("Source Files\\Core" FILES ${LUXRAYS_SRCS_CORE})


set(LUXRAYS_SRCS_CORE_GEOMETRY
	${LuxRays_SOURCE_DIR}/src/core/geometry/bbox.cpp
	${LuxRays_SOURCE_DIR}/src/core/geometry/matrix4x4.cpp
	${LuxRays_SOURCE_DIR}/src/core/geometry/transform.cpp
)
SOURCE_GROUP("Source Files\\Core\\Geometry" FILES ${LUXRAYS_SRCS_CORE_GEOMETRY})

IF(LUXRAYS_DISABLE_OPENCL)
	set(LUXRAYS_SRCS_IDEVICES
		${LuxRays_SOURCE_DIR}/src/idevices/nativeidevice.cpp
		${LuxRays_SOURCE_DIR}/src/idevices/virtualidevice.cpp
	)
ELSE(LUXRAYS_DISABLE_OPENCL)
	set(LUXRAYS_SRCS_IDEVICES
		${LuxRays_SOURCE_DIR}/src/idevices/openclidevice.cpp
		${LuxRays_SOURCE_DIR}/src/idevices/nativeidevice.cpp
		${LuxRays_SOURCE_DIR}/src/idevices/virtualidevice.cpp
	)
ENDIF(LUXRAYS_DISABLE_OPENCL)
SOURCE_GROUP("Source Files\\IDevices" FILES ${LUXRAYS_SRCS_IDEVICES})


set(LUXRAYS_SRCS_KERNELS
	${LuxRays_SOURCE_DIR}/src/kernels/bvh_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/mqbvh_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/qbvh_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/sampler_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/filter_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/camera_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/trianglemesh_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/trianglemesh_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/randomgen_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/randomgen_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/matrix4x4_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/transform_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/transform_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/mc_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/frame_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/frame_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/bsdf_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/bsdf_funcs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/epsilon_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/spectrum_types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/spectrum_funcs_kernel.cpp
)
SOURCE_GROUP("Source Files\\Kernels" FILES ${LUXRAYS_SRCS_KERNELS})


set(LUXRAYS_SRCS_UTILS
	${LuxRays_SOURCE_DIR}/src/utils/properties.cpp
)
SOURCE_GROUP("Source Files\\Utils" FILES ${LUXRAYS_SRCS_UTILS})	

set(LUXRAYS_SRCS_UTILS_CORE
	${LuxRays_SOURCE_DIR}/src/utils/core/exttrianglemesh.cpp
)
SOURCE_GROUP("Source Files\\Utils\\core" FILES ${LUXRAYS_SRCS_UTILS_CORE})	

set(LUXRAYS_SRCS_UTILS_FILM
	${LuxRays_SOURCE_DIR}/src/utils/film/film.cpp
)
SOURCE_GROUP("Source Files\\Utils\\film" FILES ${LUXRAYS_SRCS_UTILS_FILM})

set(LUXRAYS_SRCS_UTILS_CONVTEST
	${LuxRays_SOURCE_DIR}/src/utils/convtest/pdiff/lpyramid.cpp
        ${LuxRays_SOURCE_DIR}/src/utils/convtest/pdiff/metric.cpp
        ${LuxRays_SOURCE_DIR}/src/utils/convtest/convtest.cpp
)
SOURCE_GROUP("Source Files\\Utils\\convtest" FILES ${LUXRAYS_SRCS_UTILS_CONVTEST})

set(LUXRAYS_SRCS_UTILS_SAMPLER
	${LuxRays_SOURCE_DIR}/src/utils/sampler/sampler.cpp
)
SOURCE_GROUP("Source Files\\Utils\\sampler" FILES ${LUXRAYS_SRCS_UTILS_SAMPLER})

set(LUXRAYS_SRCS_UTILS_SDL
	${LuxRays_SOURCE_DIR}/src/utils/sdl/bsdf.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/camera.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/extmeshcache.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/light.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/material.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/scene.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/sdl.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/spd.cpp
	${LuxRays_SOURCE_DIR}/src/utils/sdl/texture.cpp
)
SOURCE_GROUP("Source Files\\Utils\\sdl" FILES ${LUXRAYS_SRCS_UTILS_SDL})	

set(LUXRAYS_SRCS_UTILS_PLYMESH
	${LuxRays_SOURCE_DIR}/src/utils/plymesh/rply.cpp
)
SOURCE_GROUP("Source Files\\Utils\\PlyMesh" FILES ${LUXRAYS_SRCS_UTILS_PLYMESH})	

IF(LUXRAYS_DISABLE_OPENCL)
	set(LUXRAYS_SRCS_UTILS_OCL )
ELSE(LUXRAYS_DISABLE_OPENCL)
	set(LUXRAYS_SRCS_UTILS_OCL
		${LuxRays_SOURCE_DIR}/src/utils/ocl/utils.cpp
	)
ENDIF(LUXRAYS_DISABLE_OPENCL)
SOURCE_GROUP("Source Files\\Utils\\Utils" FILES ${LUXRAYS_SRCS_UTILS_OCL})

set(LUXRAYS_SRCS
	${LUXRAYS_SRCS_ACCELERATORS}
	${LUXRAYS_SRCS_CORE}
	${LUXRAYS_SRCS_CORE_GEOMETRY}
	${LUXRAYS_SRCS_IDEVICES}
	${LUXRAYS_SRCS_KERNELS}
	${LUXRAYS_SRCS_UTILS_CORE}
	${LUXRAYS_SRCS_UTILS_FILM}
	${LUXRAYS_SRCS_UTILS_CONVTEST}
	${LUXRAYS_SRCS_UTILS_OCL}
	${LUXRAYS_SRCS_UTILS_SDL}
	${LUXRAYS_SRCS_UTILS_SAMPLER}
	${LUXRAYS_SRCS_UTILS_PLYMESH}
	${LUXRAYS_SRCS_UTILS}
)


set(LUXRAYS_HEADERS_LUXRAYS
	${LuxRays_SOURCE_DIR}/include/luxrays/luxrays.h
	${LuxRays_SOURCE_DIR}/include/luxrays/cfg.h
)
SOURCE_GROUP("Header Files" FILES ${LUXRAYS_HEADERS_LUXRAYS})	

set(LUXRAYS_HEADERS_LUXRAYS_ACCELERATORS
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/bvhaccel.h
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/mqbvhaccel.h
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/qbvhaccel.h
)
SOURCE_GROUP("Header Files\\Accelerators" FILES ${LUXRAYS_HEADERS_LUXRAYS_ACCELERATORS})


set(LUXRAYS_HEADERS_LUXRAYS_CORE
	${LuxRays_SOURCE_DIR}/include/luxrays/core/accelerator.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/context.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/dataset.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/device.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/epsilon.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/intersectiondevice.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/trianglemesh.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/utils.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/virtualdevice.h
)
SOURCE_GROUP("Header Files\\Core" FILES ${LUXRAYS_HEADERS_LUXRAYS_CORE})

set(LUXRAYS_HEADERS_LUXRAYS_CORE_GEOMETRY
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/bbox.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/bsphere.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/matrix4x4.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/normal.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/point.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/ray.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/raybuffer.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/transform.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/triangle.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/uv.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/vector.h
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/vector_normal.h
)
SOURCE_GROUP("Header Files\\Core\\Geometry" FILES ${LUXRAYS_HEADERS_LUXRAYS_CORE_GEOMETRY})

set(LUXRAYS_HEADERS_LUXRAYS_KERNELS
	${LuxRays_SOURCE_DIR}/include/luxrays/kernels/kernels.h
)
SOURCE_GROUP("Header Files\\Kernels" FILES ${LUXRAYS_HEADERS_LUXRAYS_KERNELS})


set(LUXRAYS_HEADERS_LUXRAYS_UTILS
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/properties.h
)
SOURCE_GROUP("Header Files\\Utils" FILES ${LUXRAYS_HEADERS_LUXRAYS_UTILS})

set(LUXRAYS_HEADERS_LUXRAYS_UTILS_CORE
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/atomic.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/exttrianglemesh.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/mc.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum.h
)
SOURCE_GROUP("Header Files\\Utils\\Core" FILES ${LUXRAYS_HEADERS_LUXRAYS_UTILS_CORE})


set(LUXRAYS_HEADERS_LUXRAYS_UTILS_FILM
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/film.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/filter.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/framebuffer.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/tonemapping.h
)
SOURCE_GROUP("Header Files\\Utils\\Film" FILES ${LUXRAYS_HEADERS_LUXRAYS_UTILS_FILM})

set(LUXRAYS_HEADERS_LUXRAYS_UTILS_PLYMESH
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/plymesh/rply.h
)
SOURCE_GROUP("Header Files\\Utils\\PlyMesh" FILES ${LUXRAYS_HEADERS_LUXRAYS_UTILS_PLYMESH})


set(LUXRAYS_HEADERS_LUXRAYS_UTILS_SDL
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/camera.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/extmeshcache.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/light.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/material.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/scene.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/spd.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/texture.h
)
SOURCE_GROUP("Header Files\\Utils\\SDL" FILES ${LUXRAYS_HEADERS_LUXRAYS_UTILS_SDL})

set(LUXRAYS_HEADERS_LUXRAYS_UTILS_SDL_DATA
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/rgbD65_32.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/rgbE_32.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/sun_spect.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/xyzbasis.h
)
SOURCE_GROUP("Header Files\\Utils\\SDL\\Data" FILES ${LUXRAYS_HEADERS_LUXRAYS_UTILS_SDL_DATA})

set(LUXRAYS_HEADERS_LUXRAYS_OPENCL
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/device.h
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/intersectiondevice.h
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/opencl.h
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/utils.h
)
SOURCE_GROUP("Header Files\\OpenCL" FILES ${LUXRAYS_HEADERS_LUXRAYS_OPENCL})

set(LuxRays_HEADERS
	${LUXRAYS_HEADERS_LUXRAYS}
	${LUXRAYS_HEADERS_LUXRAYS_ACCELERATORS}
	${LUXRAYS_HEADERS_LUXRAYS_CORE}
	${LUXRAYS_HEADERS_LUXRAYS_CORE_GEOMETRY}
	${LUXRAYS_HEADERS_LUXRAYS_KERNELS}
	${LUXRAYS_HEADERS_LUXRAYS_OPENCL}
	${LUXRAYS_HEADERS_LUXRAYS_UTILS}
	${LUXRAYS_HEADERS_LUXRAYS_UTILS_CORE}
	${LUXRAYS_HEADERS_LUXRAYS_UTILS_FILM}
	${LUXRAYS_HEADERS_LUXRAYS_UTILS_PLYMESH}
	${LUXRAYS_HEADERS_LUXRAYS_UTILS_SDL}
	${LUXRAYS_HEADERS_LUXRAYS_UTILS_SDL_DATA}
)

add_library(luxrays STATIC ${LUXRAYS_SRCS} ${LuxRays_HEADERS})

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../lib")

IF(GCC AND NOT APPLE)
	# These accelerators needs to be compiled with much reduced optimizations
	# otherwise gcc produces incorrect code and ruins the render on 64bits machines
	SET_SOURCE_FILES_PROPERTIES(${LuxRays_SOURCE_DIR}/src/accelerators/qbvhaccel.cpp COMPILE_FLAGS "-O2")
	SET_SOURCE_FILES_PROPERTIES(${LuxRays_SOURCE_DIR}/src/accelerators/mqbvhaccel.cpp COMPILE_FLAGS "-O2")
ENDIF(GCC AND NOT APPLE)

#
# Copy needed files for compiling LuxRender to dependencies directory, to find 
# them also if builddir name is not "luxrays"
#
IF(OSX_UPDATE_LUXRAYS_REPO)
	ADD_CUSTOM_COMMAND(
	TARGET luxrays POST_BUILD
	COMMAND rm -rf ${OSX_DEPENDENCY_ROOT}/include/LuxRays/luxrays # clean first to assure update
	COMMAND cp ${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/libluxrays.a ${OSX_DEPENDENCY_ROOT}/lib/LuxRays
	COMMAND cp -R ${CMAKE_SOURCE_DIR}/include/luxrays ${OSX_DEPENDENCY_ROOT}/include/LuxRays
	)
ENDIF(OSX_UPDATE_LUXRAYS_REPO)

TARGET_LINK_LIBRARIES(luxrays ${Boost_LIBRARIES})
TARGET_LINK_LIBRARIES(luxrays debug ${FreeImage_LIBRARY_DBG} optimized ${FreeImage_LIBRARY_REL})

IF (NOT LUXRAYS_DISABLE_OPENCL)
	TARGET_LINK_LIBRARIES(luxrays ${OPENGL_gl_LIBRARY})
	TARGET_LINK_LIBRARIES(luxrays ${OPENCL_LIBRARIES})
ENDIF(NOT LUXRAYS_DISABLE_OPENCL)

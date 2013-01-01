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

PreprocessOCLKernel(luxrays BVH
	${LuxRays_SOURCE_DIR}/src/kernels/bvh_kernel.cl
	${LuxRays_SOURCE_DIR}/src/kernels/bvh_kernel.cpp)
PreprocessOCLKernel(luxrays QBVH
	${LuxRays_SOURCE_DIR}/src/kernels/qbvh_kernel.cl
	${LuxRays_SOURCE_DIR}/src/kernels/qbvh_kernel.cpp)
PreprocessOCLKernel(luxrays MQBVH
	${LuxRays_SOURCE_DIR}/src/kernels/mqbvh_kernel.cl
	${LuxRays_SOURCE_DIR}/src/kernels/mqbvh_kernel.cpp)

PreprocessOCLKernel(luxrays SamplerTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sampler/sampler_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/samplertypes_kernel.cpp)
PreprocessOCLKernel(luxrays FilterTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/filter_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/filtertypes_kernel.cpp)
PreprocessOCLKernel(luxrays CameraTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/camera_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/cameratypes_kernel.cpp)
PreprocessOCLKernel(luxrays TriangleMeshTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/core/trianglemesh_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/trianglemeshtypes_kernel.cpp)
PreprocessOCLKernel(luxrays TriangleMeshFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/core/trianglemesh_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/trianglemeshfuncs_kernel.cpp)
PreprocessOCLKernel(luxrays RandomGenTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/randomgentypes_kernel.cpp)
PreprocessOCLKernel(luxrays RandomGenFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/randomgenfuncs_kernel.cpp)
PreprocessOCLKernel(luxrays Matrix4x4Types
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/matrix4x4_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/matrix4x4types_kernel.cpp)
PreprocessOCLKernel(luxrays TransformTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/transform_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/transformtypes_kernel.cpp)
PreprocessOCLKernel(luxrays TransformFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/transform_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/transformfuncs_kernel.cpp)
PreprocessOCLKernel(luxrays McFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/mc_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/mcfuncs_kernel.cpp)
PreprocessOCLKernel(luxrays FrameTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/frame_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/frametypes_kernel.cpp)
PreprocessOCLKernel(luxrays FrameFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/core/geometry/frame_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/framefuncs_kernel.cpp)
PreprocessOCLKernel(luxrays BSDFTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/bsdf_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/bsdftypes_kernel.cpp)
PreprocessOCLKernel(luxrays BSDFFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/bsdf_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/bsdffuncs_kernel.cpp)
PreprocessOCLKernel(luxrays EpsilonTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/core/epsilon_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/epsilontypes_kernel.cpp)
PreprocessOCLKernel(luxrays SpectrumTypes
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum_types.cl
	${LuxRays_SOURCE_DIR}/src/kernels/spectrumtypes_kernel.cpp)
PreprocessOCLKernel(luxrays SpectrumFuncs
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum_funcs.cl
	${LuxRays_SOURCE_DIR}/src/kernels/spectrumfuncs_kernel.cpp)

###########################################################################
#
# Library project files
#
###########################################################################

set(LuxRays_SRCS_ACCELERATORS
	${LuxRays_SOURCE_DIR}/src/accelerators/bvhaccel.cpp
	${LuxRays_SOURCE_DIR}/src/accelerators/mqbvhaccel.cpp
	${LuxRays_SOURCE_DIR}/src/accelerators/qbvhaccel.cpp
)
SOURCE_GROUP("Source Files\\Accelerators" FILES ${LuxRays_SRCS_ACCELERATORS})
	
set(LuxRays_SRCS_CORE
	${LuxRays_SOURCE_DIR}/src/core/context.cpp
	${LuxRays_SOURCE_DIR}/src/core/dataset.cpp
	${LuxRays_SOURCE_DIR}/src/core/device.cpp
	${LuxRays_SOURCE_DIR}/src/core/epsilon.cpp
	${LuxRays_SOURCE_DIR}/src/core/trianglemesh.cpp
)
SOURCE_GROUP("Source Files\\Core" FILES ${LuxRays_SRCS_CORE})


set(LuxRays_SRCS_CORE_GEOMETRY
	${LuxRays_SOURCE_DIR}/src/core/geometry/bbox.cpp
	${LuxRays_SOURCE_DIR}/src/core/geometry/matrix4x4.cpp
	${LuxRays_SOURCE_DIR}/src/core/geometry/transform.cpp
)
SOURCE_GROUP("Source Files\\Core\\Geometry" FILES ${LuxRays_SRCS_CORE_GEOMETRY})

IF(LUXRAYS_DISABLE_OPENCL)
	set(LuxRays_SRCS_IDEVICES
		${LuxRays_SOURCE_DIR}/src/idevices/nativeidevice.cpp
		${LuxRays_SOURCE_DIR}/src/idevices/virtualidevice.cpp
	)
ELSE(LUXRAYS_DISABLE_OPENCL)
	set(LuxRays_SRCS_IDEVICES
		${LuxRays_SOURCE_DIR}/src/idevices/openclidevice.cpp
		${LuxRays_SOURCE_DIR}/src/idevices/nativeidevice.cpp
		${LuxRays_SOURCE_DIR}/src/idevices/virtualidevice.cpp
	)
ENDIF(LUXRAYS_DISABLE_OPENCL)
SOURCE_GROUP("Source Files\\IDevices" FILES ${LuxRays_SRCS_IDEVICES})


set(LuxRays_SRCS_KERNELS
	${LuxRays_SOURCE_DIR}/src/kernels/bvh_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/mqbvh_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/qbvh_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/samplertypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/filtertypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/cameratypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/trianglemeshtypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/trianglemeshfuncs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/randomgentypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/randomgenfuncs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/matrix4x4types_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/transformtypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/transformfuncs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/mcfuncs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/frametypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/framefuncs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/bsdftypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/bsdffuncs_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/epsilontypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/spectrumtypes_kernel.cpp
	${LuxRays_SOURCE_DIR}/src/kernels/spectrumfuncs_kernel.cpp
)
SOURCE_GROUP("Source Files\\Kernels" FILES ${LuxRays_SRCS_KERNELS})


set(LuxRays_SRC_UTILS
	${LuxRays_SOURCE_DIR}/src/utils/properties.cpp
)
SOURCE_GROUP("Source Files\\Utils" FILES ${LuxRays_SRC_UTILS})	

set(LuxRays_SRC_UTILS_CORE
	${LuxRays_SOURCE_DIR}/src/utils/core/exttrianglemesh.cpp
)
SOURCE_GROUP("Source Files\\Utils\\core" FILES ${LuxRays_SRC_UTILS_CORE})	

set(LuxRays_SRC_UTILS_FILM
	${LuxRays_SOURCE_DIR}/src/utils/film/film.cpp
)
SOURCE_GROUP("Source Files\\Utils\\film" FILES ${LuxRays_SRC_UTILS_FILM})

set(LuxRays_SRC_UTILS_CONVTEST
	${LuxRays_SOURCE_DIR}/src/utils/convtest/pdiff/lpyramid.cpp
        ${LuxRays_SOURCE_DIR}/src/utils/convtest/pdiff/metric.cpp
        ${LuxRays_SOURCE_DIR}/src/utils/convtest/convtest.cpp
)
SOURCE_GROUP("Source Files\\Utils\\convtest" FILES ${LuxRays_SRC_UTILS_CONVTEST})

set(LuxRays_SRC_UTILS_SAMPLER
	${LuxRays_SOURCE_DIR}/src/utils/sampler/sampler.cpp
)
SOURCE_GROUP("Source Files\\Utils\\sampler" FILES ${LuxRays_SRC_UTILS_SAMPLER})

set(LuxRays_SRC_UTILS_SDL
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
SOURCE_GROUP("Source Files\\Utils\\sdl" FILES ${LuxRays_SRC_UTILS_SDL})	

set(LuxRays_SRC_UTILS_PLYMESH
	${LuxRays_SOURCE_DIR}/src/utils/plymesh/rply.cpp
)
SOURCE_GROUP("Source Files\\Utils\\PlyMesh" FILES ${LuxRays_SRC_UTILS_PLYMESH})	

IF(LUXRAYS_DISABLE_OPENCL)
	set(LuxRays_SRC_UTILS_OCL )
ELSE(LUXRAYS_DISABLE_OPENCL)
	set(LuxRays_SRC_UTILS_OCL
		${LuxRays_SOURCE_DIR}/src/utils/ocl/utils.cpp
	)
ENDIF(LUXRAYS_DISABLE_OPENCL)
SOURCE_GROUP("Source Files\\Utils\\Utils" FILES ${LuxRays_SRC_UTILS_OCL})

set(LuxRays_SRCS
	${LuxRays_SRCS_ACCELERATORS}
	${LuxRays_SRCS_CORE}
	${LuxRays_SRCS_CORE_GEOMETRY}
	${LuxRays_SRCS_IDEVICES}
	${LuxRays_SRCS_KERNELS}
	${LuxRays_SRC_UTILS_CORE}
	${LuxRays_SRC_UTILS_FILM}
	${LuxRays_SRC_UTILS_CONVTEST}
	${LuxRays_SRC_UTILS_OCL}
	${LuxRays_SRC_UTILS_SDL}
	${LuxRays_SRC_UTILS_SAMPLER}
	${LuxRays_SRC_UTILS_PLYMESH}
	${LuxRays_SRC_UTILS}
)


set(LuxRays_HEADERS_LUXRAYS
	${LuxRays_SOURCE_DIR}/include/luxrays/luxrays.h
	${LuxRays_SOURCE_DIR}/include/luxrays/cfg.h
)
SOURCE_GROUP("Header Files" FILES ${LuxRays_HEADERS_LUXRAYS})	

set(LuxRays_HEADERS_LUXRAYS_ACCELERATORS
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/bvhaccel.h
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/mqbvhaccel.h
	${LuxRays_SOURCE_DIR}/include/luxrays/accelerators/qbvhaccel.h
)
SOURCE_GROUP("Header Files\\Accelerators" FILES ${LuxRays_HEADERS_LUXRAYS_ACCELERATORS})


set(LuxRays_HEADERS_LUXRAYS_CORE
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
SOURCE_GROUP("Header Files\\Core" FILES ${LuxRays_HEADERS_LUXRAYS_CORE})

set(LuxRays_HEADERS_LUXRAYS_CORE_GEOMETRY
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
SOURCE_GROUP("Header Files\\Core\\Geometry" FILES ${LuxRays_HEADERS_LUXRAYS_CORE_GEOMETRY})

set(LuxRays_HEADERS_LUXRAYS_KERNELS
	${LuxRays_SOURCE_DIR}/include/luxrays/kernels/kernels.h
)
SOURCE_GROUP("Header Files\\Kernels" FILES ${LuxRays_HEADERS_LUXRAYS_KERNELS})


set(LuxRays_HEADERS_LUXRAYS_UTILS
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/properties.h
)
SOURCE_GROUP("Header Files\\Utils" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS})

set(LuxRays_HEADERS_LUXRAYS_UTILS_CORE
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/atomic.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/exttrianglemesh.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/mc.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum.h
)
SOURCE_GROUP("Header Files\\Utils\\Core" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_CORE})


set(LuxRays_HEADERS_LUXRAYS_UTILS_FILM
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/film.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/filter.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/framebuffer.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/tonemapping.h
)
SOURCE_GROUP("Header Files\\Utils\\Film" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_FILM})

set(LuxRays_HEADERS_LUXRAYS_UTILS_PLYMESH
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/plymesh/rply.h
)
SOURCE_GROUP("Header Files\\Utils\\PlyMesh" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_PLYMESH})


set(LuxRays_HEADERS_LUXRAYS_UTILS_SDL
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/camera.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/extmeshcache.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/light.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/material.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/scene.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/spd.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/texture.h
)
SOURCE_GROUP("Header Files\\Utils\\SDL" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_SDL})

set(LuxRays_HEADERS_LUXRAYS_UTILS_SDL_DATA
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/rgbD65_32.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/rgbE_32.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/sun_spect.h
	${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/xyzbasis.h
)
SOURCE_GROUP("Header Files\\Utils\\SDL\\Data" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_SDL_DATA})

set(LuxRays_HEADERS_LUXRAYS_OPENCL
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/device.h
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/intersectiondevice.h
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/opencl.h
	${LuxRays_SOURCE_DIR}/include/luxrays/opencl/utils.h
)
SOURCE_GROUP("Header Files\\OpenCL" FILES ${LuxRays_HEADERS_LUXRAYS_OPENCL})

set(LuxRays_HEADERS
	${LuxRays_HEADERS_LUXRAYS}
	${LuxRays_HEADERS_LUXRAYS_ACCELERATORS}
	${LuxRays_HEADERS_LUXRAYS_CORE}
	${LuxRays_HEADERS_LUXRAYS_CORE_GEOMETRY}
	${LuxRays_HEADERS_LUXRAYS_KERNELS}
	${LuxRays_HEADERS_LUXRAYS_OPENCL}
	${LuxRays_HEADERS_LUXRAYS_UTILS}
	${LuxRays_HEADERS_LUXRAYS_UTILS_CORE}
	${LuxRays_HEADERS_LUXRAYS_UTILS_FILM}
	${LuxRays_HEADERS_LUXRAYS_UTILS_PLYMESH}
	${LuxRays_HEADERS_LUXRAYS_UTILS_SDL}
	${LuxRays_HEADERS_LUXRAYS_UTILS_SDL_DATA}
)

add_library(luxrays STATIC ${LuxRays_SRCS} ${LuxRays_HEADERS})

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

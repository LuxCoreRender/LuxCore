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

SET(KERNELS BVH QBVH MQBVH Pixel_ClearFB Pixel_ClearSampleFB Pixel_UpdateFrameBuffer Pixel_AddSampleBuffer Pixel_AddSampleBufferPreview Pixel_AddSampleBufferGaussian2x2)
FOREACH(kernel ${KERNELS})
	STRING(TOLOWER ${kernel} kernel_lowercase)
  IF(WIN32)
    add_custom_command(
      OUTPUT ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo "#include \"luxrays/kernels/kernels.h\"" >  ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo std::string luxrays::KernelSource_${kernel} = >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
#      COMMAND ../BuildTools/ProcessClKernelFile.exe ${LuxRays_SOURCE_DIR}\\src\\kernels\\${kernel_lowercase}_kernel.cl >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND for /F \"tokens=*\" %%a in (${LuxRays_SOURCE_DIR}\\src\\kernels\\${kernel_lowercase}_kernel.cl) do echo \"%%a\\n\" >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo ";" >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      MAIN_DEPENDENCY ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cl
    )
	ELSE(WIN32)
    add_custom_command(
      OUTPUT ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo \"\#include \\"luxrays/kernels/kernels.h\\"\" > ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo "std::string luxrays::KernelSource_${kernel} = " >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND cat ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cl | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      COMMAND echo "\;" >> ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cpp
      MAIN_DEPENDENCY ${LuxRays_SOURCE_DIR}/src/kernels/${kernel_lowercase}_kernel.cl
    )
  ENDIF(WIN32)
ENDFOREACH(kernel)

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
  ${LuxRays_SOURCE_DIR}/src/core/trianglemesh.cpp
)
SOURCE_GROUP("Source Files\\Core" FILES ${LuxRays_SRCS_CORE})


set(LuxRays_SRCS_CORE_GEOMETRY
  ${LuxRays_SOURCE_DIR}/src/core/geometry/bbox.cpp
  ${LuxRays_SOURCE_DIR}/src/core/geometry/matrix4x4.cpp
	${LuxRays_SOURCE_DIR}/src/core/geometry/ray.cpp
  ${LuxRays_SOURCE_DIR}/src/core/geometry/transform.cpp
)
SOURCE_GROUP("Source Files\\Core\\Geometry" FILES ${LuxRays_SRCS_CORE_GEOMETRY})

set(LuxRays_SRCS_IDEVICES
  ${LuxRays_SOURCE_DIR}/src/idevices/openclidevice.cpp
  ${LuxRays_SOURCE_DIR}/src/idevices/nativeidevice.cpp
  ${LuxRays_SOURCE_DIR}/src/idevices/virtualidevice.cpp
)
SOURCE_GROUP("Source Files\\IDevices" FILES ${LuxRays_SRCS_IDEVICES})


set(LuxRays_SRCS_KERNELS
  ${LuxRays_SOURCE_DIR}/src/kernels/bvh_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/mqbvh_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/qbvh_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/pixel_addsamplebuffer_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/pixel_addsamplebufferpreview_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/pixel_addsamplebuffergaussian2x2_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/pixel_clearfb_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/pixel_clearsamplefb_kernel.cpp
  ${LuxRays_SOURCE_DIR}/src/kernels/pixel_updateframebuffer_kernel.cpp
)
SOURCE_GROUP("Source Files\\Kernels" FILES ${LuxRays_SRCS_KERNELS})


set(LuxRays_SRCS_PDEVICES
  ${LuxRays_SOURCE_DIR}/src/pdevices/nativepdevice.cpp
  ${LuxRays_SOURCE_DIR}/src/pdevices/openclpdevice.cpp
)
SOURCE_GROUP("Source Files\\PDevices" FILES ${LuxRays_SRCS_PDEVICES})	


set(LuxRays_SRC_UTILS
  ${LuxRays_SOURCE_DIR}/src/utils/properties.cpp
)
SOURCE_GROUP("Source Files\\Utils" FILES ${LuxRays_SRC_UTILS})	

set(LuxRays_SRC_UTILS_CORE
	${LuxRays_SOURCE_DIR}/src/utils/core/exttrianglemesh.cpp
)
SOURCE_GROUP("Source Files\\Utils\\core" FILES ${LuxRays_SRC_UTILS_CORE})	

set(LuxRays_SRC_UTILS_SDL
	${LuxRays_SOURCE_DIR}/src/utils/sdl/extmeshcache.cpp
  ${LuxRays_SOURCE_DIR}/src/utils/sdl/light.cpp
  ${LuxRays_SOURCE_DIR}/src/utils/sdl/scene.cpp
  ${LuxRays_SOURCE_DIR}/src/utils/sdl/spd.cpp
  ${LuxRays_SOURCE_DIR}/src/utils/sdl/texmap.cpp
)
SOURCE_GROUP("Source Files\\Utils\\SDL" FILES ${LuxRays_SRC_UTILS_SDL})	

set(LuxRays_SRC_UTILS_PLYMESH
  ${LuxRays_SOURCE_DIR}/src/utils/plymesh/rply.cpp
)
SOURCE_GROUP("Source Files\\Utils\\PlyMesh" FILES ${LuxRays_SRC_UTILS_PLYMESH})	


set(LuxRays_SRCS
	${LuxRays_SRCS_ACCELERATORS}
	${LuxRays_SRCS_CORE}
	${LuxRays_SRCS_CORE_GEOMETRY}
	${LuxRays_SRCS_IDEVICES}
	${LuxRays_SRCS_KERNELS}
	${LuxRays_SRCS_PDEVICES}
	${LuxRays_SRC_UTILS_CORE}
	${LuxRays_SRC_UTILS_SDL}
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
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/acceleretor.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/context.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/dataset.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/device.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/intersectiondevice.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/pixeldevice.h
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

set(LuxRays_HEADERS_LUXRAYS_CORE_PIXEL
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/pixel/filter.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/pixel/framebuffer.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/pixel/samplebuffer.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/core/pixel/spectrum.h
)
SOURCE_GROUP("Header Files\\Core\\Pixel" FILES ${LuxRays_HEADERS_LUXRAYS_CORE_PIXEL})


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
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/randomgen.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/core/spectrum.h
)
SOURCE_GROUP("Header Files\\Utils\\Core" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_CORE})


set(LuxRays_HEADERS_LUXRAYS_UTILS_FILM
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/film/film.h
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
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/mc.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/scene.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/spd.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/texmap.h
)
SOURCE_GROUP("Header Files\\Utils\\SDL" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_SDL})

set(LuxRays_HEADERS_LUXRAYS_UTILS_SDL_DATA
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/rgbD65_32.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/rgbE_32.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/sun_spect.h
  ${LuxRays_SOURCE_DIR}/include/luxrays/utils/sdl/data/xyzbasis.h
)
SOURCE_GROUP("Header Files\\Utils\\SDL\\Data" FILES ${LuxRays_HEADERS_LUXRAYS_UTILS_SDL_DATA})


set(LuxRays_HEADERS
	${LuxRays_HEADERS_LUXRAYS}
	${LuxRays_HEADERS_LUXRAYS_ACCELERATORS}
	${LuxRays_HEADERS_LUXRAYS_CORE}
	${LuxRays_HEADERS_LUXRAYS_CORE_GEOMETRY}
	${LuxRays_HEADERS_LUXRAYS_CORE_PIXEL}
	${LuxRays_HEADERS_LUXRAYS_KERNELS}
	${LuxRays_HEADERS_LUXRAYS_UTILS}
	${LuxRays_HEADERS_LUXRAYS_UTILS_CORE}
	${LuxRays_HEADERS_LUXRAYS_UTILS_FILM}
	${LuxRays_HEADERS_LUXRAYS_UTILS_PLYMESH}
	${LuxRays_HEADERS_LUXRAYS_UTILS_SDL}
	${LuxRays_HEADERS_LUXRAYS_UTILS_SDL_DATA}
)

add_library(LuxRays STATIC ${LuxRays_SRCS} ${LuxRays_HEADERS})

###########################################################################
#
# Predefines
#
###########################################################################

IF(GCC)
	# These accelerators needs to be compiled with much reduced optimizations
	# otherwise gcc produces incorrect code and ruins the render on 64bits machines
	SET_SOURCE_FILES_PROPERTIES(${LuxRays_SOURCE_DIR}/src/accelerators/qbvhaccel.cpp COMPILE_FLAGS "-O2")
	SET_SOURCE_FILES_PROPERTIES(${LuxRays_SOURCE_DIR}/src/accelerators/mqbvhaccel.cpp COMPILE_FLAGS "-O2")
ENDIF(GCC)

#
# copy needed files for compiling LuxRender to dependencies directory, to find 
# them also if builddir name is not "luxrays"
#
IF(APPLE)
	ADD_CUSTOM_COMMAND(
	TARGET LuxRays POST_BUILD
	COMMAND cp ${CMAKE_BINARY_DIR}/src/release/libluxrays.a ${CMAKE_BINARY_DIR}/../macos/lib/LuxRays
	COMMAND cp -R ${CMAKE_BINARY_DIR}/include/luxrays ${CMAKE_BINARY_DIR}/../macos/include/LuxRays
	)
ENDIF(APPLE)

TARGET_LINK_LIBRARIES(LuxRays ${OPENGL_gl_LIBRARY})

TARGET_LINK_LIBRARIES(LuxRays debug ${FreeImage_LIBRARY_DBG} optimized ${FreeImage_LIBRARY_REL} general ${FreeImage_LIBRARY_REL})
TARGET_LINK_LIBRARIES(LuxRays ${OPENCL_LIBRARY})

TARGET_LINK_LIBRARIES(LuxRays ${Boost_LIBRARIES})
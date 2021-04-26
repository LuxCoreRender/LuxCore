################################################################################
# Copyright 1998-2020 by authors (see AUTHORS.txt)
#
#   This file is part of LuxCoreRender.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
################################################################################

#############################################################################
#############################################################################
#############################################################################
###
### SLG Library
###
#############################################################################
#############################################################################
#############################################################################

#############################################################################
#
# SLG Docs
#
#############################################################################

find_package(Doxygen)

if(DOXYGEN_FOUND)
	message(STATUS "Found Doxygen and generating SLG documentation")

	# Generate doxygen.template
	set(DOXYGEN_SLG_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/../../doxygen/slg.template)
	configure_file(
	  "${DOXYGEN_SLG_TEMPLATE}.in"
	  "${DOXYGEN_SLG_TEMPLATE}"
	  )

	set(DOXYGEN_SLG_INPUT ${CMAKE_CURRENT_BINARY_DIR}/../../doxygen-slg.conf)
	set(DOXYGEN_SLG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../../doc/slg)
	set(DOXYGEN_SLG_OUTPUT ${DOXYGEN_OUTPUT_DIR}/slg/html/index.html)

	message(STATUS "Doxygen SLG output: " ${DOXYGEN_SLG_OUTPUT})

	if(DOXYGEN_DOT_FOUND)
			message(STATUS "Found dot")
			set(DOXYGEN_DOT_CONF "HAVE_DOT = YES")
	endif(DOXYGEN_DOT_FOUND)

	add_custom_command(
		OUTPUT ${DOXYGEN_SLG_OUTPUT}
		# Creating custom doxygen-luxrays.conf
		COMMAND mkdir -p ${DOXYGEN_SLG_OUTPUT_DIR}
		COMMAND cp ${DOXYGEN_SLG_TEMPLATE} ${DOXYGEN_SLG_INPUT}
		COMMAND echo "INPUT = " ${CMAKE_CURRENT_SOURCE_DIR}/../../include/slg  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/slg >> ${DOXYGEN_SLG_INPUT}
		COMMAND echo "OUTPUT_DIRECTORY = " ${DOXYGEN_SLG_OUTPUT_DIR} >> ${DOXYGEN_SLG_INPUT}
		COMMAND echo ${DOXYGEN_DOT_CONF} >> ${DOXYGEN_SLG_INPUT}
		# Launch doxygen
		COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_SLG_INPUT}
		DEPENDS ${DOXYGEN_SLG_TEMPLATE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../..
	)

	add_custom_target(apidoc_slg DEPENDS ${DOXYGEN_SLG_OUTPUT})
endif(DOXYGEN_FOUND)

###########################################################################

link_directories (${LuxCoreRender_LIB_DIR})

add_definitions(-DCPP_API_EXPORTS ${VISIBILITY_FLAGS}) # for controlling visibility

###########################################################################
#
# BCD files
#
###########################################################################

set(BCD_SRCS
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/Chronometer.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/CovarianceMatrix.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/Denoiser.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/DenoisingUnit.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/MultiscaleDenoiser.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/SamplesAccumulator.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/SpikeRemovalFilter.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/src/core/Utils.cpp
)

SOURCE_GROUP("Source Files\\BCD Library" FILES ${BCD_SRCS})

include_directories(${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/include)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/bcd-1.1/include/bcd/core)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/eigen-3.3.4)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/lut)

# Use only MPL licensed source code
add_definitions("-DEIGEN_MPL2_ONLY")

# Disable all warnings
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set_source_files_properties(${BCD_SRCS} PROPERTIES COMPILE_FLAGS "-w")
endif()

add_library(bcd STATIC ${BCD_SRCS})

###########################################################################
#
# OpenVDB files
#
###########################################################################

set(OPENVDB_SRCS
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/Grid.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/Metadata.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/MetaMap.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/Platform.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/openvdb.cc	
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/Archive.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/Compression.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/DelayedLoadMetadata.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/File.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/GridDescriptor.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/Queue.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/Stream.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/io/TempFile.cc	
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/math/Maps.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/math/Proximity.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/math/QuantizedUnitVec.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/math/Transform.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/points/AttributeArray.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/points/AttributeArrayString.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/points/AttributeGroup.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/points/AttributeSet.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/points/points.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/points/StreamCompression.cc	
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/util/Formats.cc
	${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0/openvdb/util/Util.cc
)

SOURCE_GROUP("Source Files\\OpenVDB Library" FILES ${OPENVDB_SRCS})

include_directories(${LuxCoreRender_SOURCE_DIR}/deps/openvdb-7.0.0)

# Required by OpenVDB to read ABI 3
#add_definitions("-D OPENVDB_USE_DEPRECATED_ABI -DOPENVDB_3_ABI_COMPATIBLE -DOPENVDB_STATICLIB -DOPENVDB_OPENEXR_STATICLIB")
add_definitions("-DOPENVDB_USE_BLOSC -DOPENVDB_STATICLIB -DOPENVDB_OPENEXR_STATICLIB")

# Disable all warnings
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set_source_files_properties(${OPENVDB_SRCS} PROPERTIES COMPILE_FLAGS "-w")
endif()

add_library(openvdb STATIC ${OPENVDB_SRCS})

###########################################################################
#
# OpenSubdiv files
#
###########################################################################

set(OPENSUBDIV_SRCS
	${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/version.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/bilinearPatchBuilder.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/catmarkPatchBuilder.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/error.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/loopPatchBuilder.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/patchBasis.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/patchBuilder.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/patchDescriptor.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/patchMap.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/patchTable.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/patchTableFactory.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/ptexIndices.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/stencilTable.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/stencilTableFactory.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/stencilBuilder.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/topologyDescriptor.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/topologyRefiner.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/far/topologyRefinerFactory.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/osd/cpuEvaluator.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/osd/cpuKernel.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/osd/cpuPatchTable.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/osd/cpuVertexBuffer.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/sdc/crease.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/sdc/typeTraits.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/fvarLevel.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/fvarRefinement.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/level.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/quadRefinement.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/refinement.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/sparseSelector.cpp
    ${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/vtr/triRefinement.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/version.cpp
)

if (OPENMP_FOUND)
	set(OPENSUBDIV_SRCS
		${OPENSUBDIV_SRCS}
		${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/osd/ompEvaluator.cpp
		${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0/opensubdiv/osd/ompKernel.cpp
	)
endif()

SOURCE_GROUP("Source Files\\OpenSubdiv Library" FILES ${OPENSUBDIV_SRCS})

include_directories(${LuxCoreRender_SOURCE_DIR}/deps/opensubdiv-3.4.0)

# Required by VisualStudio to have M_PI defined
add_definitions("-D_USE_MATH_DEFINES")

# Disable all warnings
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set_source_files_properties(${OPENSUBDIV_SRCS} PROPERTIES COMPILE_FLAGS "-w")
endif()

add_library(opensubdiv STATIC ${OPENSUBDIV_SRCS})

###########################################################################
#
# OpenColorIO files
#
###########################################################################

set(OPENCOLORIO_SRCS	
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/apphelpers/CategoryHelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/apphelpers/ColorSpaceHelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/apphelpers/DisplayViewHelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/apphelpers/LegacyViewingPipeline.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/apphelpers/MixingHelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Baker.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/BitDepthUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Caching.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ColorSpace.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ColorSpaceSet.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Config.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Context.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ContextVariableUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/CPUProcessor.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Display.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/DynamicProperty.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Exception.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/cdl/CDLParser.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/cdl/CDLReaderHelper.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/cdl/CDLWriter.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/ctf/CTFReaderHelper.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/ctf/CTFReaderUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/ctf/CTFTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/ctf/IndexMapping.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormat3DL.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatCCC.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatCC.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatCDL.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatCSP.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatCTF.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatDiscreet1DL.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatHDL.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatICC.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatIridasCube.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatIridasItx.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatIridasLook.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatPandora.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatResolveCube.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatSpi1D.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatSpi3D.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatSpiMtx.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatTruelight.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FileFormatVF.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/FormatMetadata.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/xmlutils/XMLReaderHelper.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/xmlutils/XMLReaderUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/fileformats/xmlutils/XMLWriterUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/FileRules.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/GPUProcessor.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/GpuShader.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/GpuShaderDesc.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/GpuShaderUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/HashUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ImageDesc.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ImagePacking.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Logging.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Look.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/LookParse.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/MathUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/md5/md5.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/NamedTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/OCIOYaml.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Op.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/OpOptimizers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/allocation/AllocationOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/cdl/CDLOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/cdl/CDLOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/cdl/CDLOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/cdl/CDLOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/exponent/ExponentOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/exposurecontrast/ExposureContrastOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/exposurecontrast/ExposureContrastOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/exposurecontrast/ExposureContrastOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/exposurecontrast/ExposureContrastOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/fixedfunction/FixedFunctionOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/fixedfunction/FixedFunctionOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/fixedfunction/FixedFunctionOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/fixedfunction/FixedFunctionOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gamma/GammaOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gamma/GammaOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gamma/GammaOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gamma/GammaOpUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gamma/GammaOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingprimary/GradingPrimary.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingprimary/GradingPrimaryOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingprimary/GradingPrimaryOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingprimary/GradingPrimaryOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingprimary/GradingPrimaryOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingrgbcurve/GradingBSplineCurve.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingrgbcurve/GradingRGBCurveOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingrgbcurve/GradingRGBCurveOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingrgbcurve/GradingRGBCurveOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingrgbcurve/GradingRGBCurveOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingrgbcurve/GradingRGBCurve.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingtone/GradingTone.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingtone/GradingToneOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingtone/GradingToneOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingtone/GradingToneOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/gradingtone/GradingToneOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/log/LogOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/log/LogOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/log/LogOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/log/LogOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/log/LogUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut1d/Lut1DOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut1d/Lut1DOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut1d/Lut1DOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut1d/Lut1DOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut3d/Lut3DOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut3d/Lut3DOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut3d/Lut3DOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/lut3d/Lut3DOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/matrix/MatrixOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/matrix/MatrixOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/matrix/MatrixOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/matrix/MatrixOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/noop/NoOps.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/OpTools.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/range/RangeOpCPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/range/RangeOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/range/RangeOpGPU.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/range/RangeOp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ops/reference/ReferenceOpData.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ParseUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/PathUtils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Platform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Processor.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ScanlineHelper.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/Transform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/AllocationTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/ACES.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/BuiltinTransformRegistry.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/ColorMatrixHelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/OpHelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/BuiltinTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/CDLTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/ColorSpaceTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/DisplayViewTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/ExponentTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/ExponentWithLinearTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/ExposureContrastTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/FileTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/FixedFunctionTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/GradingPrimaryTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/GradingRGBCurveTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/GradingToneTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/GroupTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/LogAffineTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/LogCameraTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/LogTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/LookTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/Lut1DTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/Lut3DTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/MatrixTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/RangeTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/ArriCameras.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/CanonCameras.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/Displays.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/PanasonicCameras.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/RedCameras.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/transforms/builtins/SonyCameras.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ViewingRules.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/ViewTransform.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO/SystemMonitor.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/apputils/argparse.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/apputils/strutil.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/libutils/oiiohelpers/oiiohelpers.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/pystring/pystring.cpp
)

set(YAMLCPP_SRCS	
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/binary.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/directives.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/emitfromevents.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/emitterstate.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/exceptions.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/memory.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/node.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/nodeevents.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/ostream_wrapper.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/parser.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/scanner.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/scantag.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/simplekey.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/stream.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/convert.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/emit.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/emitter.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/emitterutils.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/exp.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/nodebuilder.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/node_data.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/null.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/parse.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/regex_yaml.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/scanscalar.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/scantoken.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/singledocparser.cpp
	${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/src/tag.cpp
)

set(EXPAT_SRCS	
	${LuxCoreRender_SOURCE_DIR}/deps/expat-2.3.0/src/xmlparse.c
	${LuxCoreRender_SOURCE_DIR}/deps/expat-2.3.0/src/xmlrole.c
	${LuxCoreRender_SOURCE_DIR}/deps/expat-2.3.0/src/xmltok.c
	${LuxCoreRender_SOURCE_DIR}/deps/expat-2.3.0/src/xmltok_impl.c
	${LuxCoreRender_SOURCE_DIR}/deps/expat-2.3.0/src/xmltok_ns.c
)

SOURCE_GROUP("Source Files\\OpenColorIO Library" FILES ${OPENCOLORIO_SRCS} ${YAMLCPP_SRCS} ${EXPAT_SRCS})

include_directories(${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/include)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/opencolorio-2.0.0/src/OpenColorIO)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/yaml-cpp-0.6.3/include)
include_directories(${LuxCoreRender_SOURCE_DIR}/deps/expat-2.3.0/src)

if(NOT WIN32)
	# "sys/random.h" not available on Windows
	add_definitions("-DHAVE_GETRANDOM")
	add_definitions("-DUSE_SSE")
endif(NOT WIN32)

if(WIN32)
	# Prevents __declspec(dllimport) on Windows
	add_definitions("-DOpenColorIO_SKIP_IMPORTS -DXML_STATIC")
endif(WIN32)

# Disable all warnings
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set_source_files_properties(${OPENCOLORIO_SRCS} PROPERTIES COMPILE_FLAGS "-w")
endif()

add_library(opencolorio STATIC ${OPENCOLORIO_SRCS} ${YAMLCPP_SRCS} ${EXPAT_SRCS})

if(WIN32)
	target_compile_definitions(opencolorio PRIVATE "-D_MBCS -DUSE_SSE")
endif(WIN32)

###########################################################################
#
# Kernel files
#
###########################################################################

set(SLG_KERNELS
	${LuxCoreRender_SOURCE_DIR}/include/slg/cameras/camera_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/cameras/camera_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/engines/caches/photongi/pgic_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/engines/caches/photongi/pgic_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/engines/pathoclbase/kernels/pathoclbase_datatypes.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/engines/pathoclbase/kernels/pathoclbase_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/engines/pathoclbase/kernels/pathoclbase_kernels_micro.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/engines/pathtracer_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/film_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/film_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/film_mergesamplebuffer_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/denoiser/filmdenoiser_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/sampleresult_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_backgroundimg_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_bloom_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_cameraresponse_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_coloraberration_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_gammacorrection_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_gaussianblur3x3_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_objectidmask_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_optixdenoiser_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_premultiplyalpha_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_vignetting_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/plugin_whitebalance_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_reduce_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_autolinear_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_linear_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_luxlinear_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/imagepipeline/plugins/tonemaps/tonemap_reinhard02_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/sampleresult_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/filters/filter_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/film/filters/filter_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/imagemap/imagemap_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/imagemap/imagemap_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/light_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/light_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/strategies/dlsc_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/strategies/dlsc_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/strategies/lightstrategy_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/visibility/elvc_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/lights/visibility/elvc_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/samplers/sampler_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/samplers/sampler_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/samplers/sampler_random_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/samplers/sampler_sobol_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/samplers/sampler_metropolis_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/samplers/sampler_tilepath_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/bsdf/bsdf_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/bsdf/bsdf_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/bsdf/bsdfutils_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/bsdf/hitpoint_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/bsdf/hitpoint_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/material_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_generic.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_default.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_archglass.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_carpaint.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_clearvol.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_cloth.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glass.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glossy2.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glossycoating.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_glossytranslucent.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_heterogeneousvol.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_homogeneousvol.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_matte.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_matte_translucent.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_metal2.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_mirror.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_mix.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_null.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_roughglass.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_roughmatte_translucent.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_thinfilmcoating.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_velvet.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_disney.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/materialdefs_funcs_twosided.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/material_funcs_evalops.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/materials/material_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/scene/scene_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/scene/scene_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/scene/sceneobject_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/hitpoint/texture_hitpoint_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/mapping/mapping_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/mapping/mapping_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/math/texture_abs_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/math/texture_clamp_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/math/texture_random_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_funcs_evalops.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_bilerp_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_blackbody_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_blender_noise_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_blender_noise_funcs2.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_blender_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_bombing_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_brick_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_bump_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_colordepth_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_densitygrid_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_distort_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_hsv_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_irregulardata_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_noise_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_imagemap_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_others_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/texture_triplanar_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/fresnel/texture_fresnelcolor_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/textures/fresnel/texture_fresnelconst_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/evalstack_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/mortoncurve_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/pathdepthinfo_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/pathdepthinfo_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/pathvolumeinfo_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/pathvolumeinfo_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/pathinfo_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/pathinfo_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/utils/varianceclamping_funcs.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/volumes/volume_types.cl
	${LuxCoreRender_SOURCE_DIR}/include/slg/volumes/volume_funcs.cl
)

# Make sure some IDE won't try to compile CL by some built-in rule:
SET_SOURCE_FILES_PROPERTIES(${SLG_KERNELS} PROPERTIES HEADER_FILE_ONLY TRUE)

SOURCE_GROUP("Source Files\\SLG Kernels" FILES ${SLG_KERNELS})

#############################################################################
#
# SLG Core library
#
#############################################################################

# Note: the following order of object linking is important for GCC when building
# a static library (to avoid an undefined symbol reference with the linker)
set(SLG_CORE_SRCS
	${LuxCoreRender_SOURCE_DIR}/src/slg/bsdf/hitpoint.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/bsdf/bsdf.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/cameras/camera.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/cameras/environment.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/cameras/orthographic.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/cameras/perspective.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/cameras/projective.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/cameras/stereo.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/colorspace.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/indexbvh.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/indexkdtree.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/indexoctree.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/sdl.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/sphericalfunction/sphericalfunction.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/core/sphericalfunction/photometricdataies.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/renderengine.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/cpurenderengine.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/oclrenderengine.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathtracer.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilerepository.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilerepositoryserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bakecpu/bakecpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bakecpu/bakecpurenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bakecpu/bakecputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bidircpu/bidircpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bidircpu/bidircputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bidircpu/bidircpurenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bidirvmcpu/bidirvmcputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/bidirvmcpu/hashgrid.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicbvh.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgickdtree.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicoctree.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicproperties.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicradius.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicupdate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/pgicvisibility.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/tracephotonsthread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/caches/photongi/photongicache.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compiledscene.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilecamera.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilefilm.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilegeometry.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compileimagemaps.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilelights.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilematerials.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilepathtracer.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilephotongi.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compilesceneobjects.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/compiletextures.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbase.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbasenativethread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthreadfilm.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthreadinit.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathoclbase/pathoclbaseoclthreadkernels.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathocl/pathocl.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathocl/pathoclrenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathocl/pathoclopenclthread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathocl/pathoclnativethread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/rtpathocl/rtpathocl.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/rtpathocl/rtpathoclthread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/lightcpu/lightcpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/lightcpu/lightcpurenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/lightcpu/lightcputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathcpu/pathcpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathcpu/pathcpurenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/pathcpu/pathcputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/rtpathcpu/rtpathcpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/rtpathcpu/rtpathcputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/filesaver/filesaver.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathcpu/tilepathcpu.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathcpu/tilepathcpurenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathcpu/tilepathcputhread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathocl.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathoclrenderstate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathnativethread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/engines/tilepathocl/tilepathoclthread.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/imagemap/imagemap.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/imagemap/imagemapcache.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/imagemap/imagemapcacheserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/imagemap/imagemapserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/constantinfinitelight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/distantlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/infinitelight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/laserlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/light.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/lightsourcedefs.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/mapspherelight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/mappointlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/pointlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/projectionlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/sharpdistantlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/sky2light.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/spherelight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/spotlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/sunlight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/trianglelight.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/dlscache.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/dlscacheimpl/dlscacheimpl.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/dlscacheimpl/dlscoctree.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/dlscacheimpl/dlscbvh.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/distributionlightstrategy.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/lightstrategy.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/logpower.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/power.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/strategies/uniform.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/lights/visibility/envlightvisibilitycache.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/archglass.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/carpaint.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/cloth.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/disney.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/glass.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/glossy2.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/glossycoating.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/glossytranslucent.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/material.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/materialdefs.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/matte.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/mattetranslucent.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/metal2.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/mirror.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/mixmat.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/null.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/roughglass.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/roughmatte.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/roughmattetranslucent.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/thinfilmcoating.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/twosided.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/materials/velvet.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/sampler.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/random.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/rtpathcpusampler.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/tilepathsampler.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/sobol.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/soboldata.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/sobolsequence.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/samplers/metropolis.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/colorspaceconverters.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/extmeshcache.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/extmeshcacheserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parsecamera.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parselights.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parsematerials.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parseobjects.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parseshapes.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parsetextures.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/parsevolumes.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/sceneobject.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/sceneobjectdefs.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/scene.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/scenepreprocess.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/sceneserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/scene/sceneupdate.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/bevelshape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/displacement.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/edgedetectoraov.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/groupshape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/harlequinshape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/islandaovshape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/meshshape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/pointiness.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/randomtriangleaovshape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/shape.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/simplify.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/strands.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/shapes/subdiv.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/slg.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/mapping/mapping.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/band.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/bevel.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/bilerp.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/blackbody.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/blender_noiselib.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/blender_texture.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/bombing.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/brick.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/brightcontrast.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/checkerboard.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/colordepth.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/constfloat.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/constfloat3.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/cloud.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/densitygrid.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/distort.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/dots.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fbm.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnelapprox.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresnelcauchy.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresnelcolor.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresnelconst.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresnelluxpop.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresnelpreset.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresnelsopra.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/fresnel/fresneltexture.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/hitpoint/hitpointaov.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/hitpoint/hitpointcolor.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/hitpoint/position.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/hitpoint/shadingnormal.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/hsv.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/imagemaptex.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/irregulardata.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/lampspectrum.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/marble.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/abs.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/add.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/clamp.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/divide.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/greaterthan.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/lessthan.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/mixtex.cpp
    ${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/modulo.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/power.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/random.cpp
    ${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/remap.cpp
    ${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/rounding.cpp  
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/scale.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/math/subtract.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/normalmap.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/object_id.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/texture.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/texturedefs.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/triplanar.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/uv.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/vectormath/dotproduct.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/vectormath/makefloat3.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/vectormath/splitfloat3.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/windy.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/wireframe.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/textures/wrinkled.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/filenameresolver.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/film2sceneradius.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/harlequincolors.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/meshutils.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/scenevisibility.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/pathdepthinfo.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/pathinfo.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/pathvolumeinfo.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/utils/varianceclamping.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/volumes/clear.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/volumes/heterogenous.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/volumes/homogenous.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/volumes/volume.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/renderconfig.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/rendersession.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/renderstate.cpp
)
SOURCE_GROUP("Source Files\\SLG Core Library" FILES ${SLG_CORE_SRCS})

add_library(slg-core STATIC ${SLG_CORE_SRCS})

#############################################################################
#
# SLG Kernels library
#
#############################################################################

set(generated_kernels_dir "${GENERATED_DIR}/slg-kernels")
file(MAKE_DIRECTORY ${generated_kernels_dir})

PreprocessOCLKernels(${generated_kernels_dir} slg ${SLG_KERNELS})

set(SLG_KERNEL_SRCS
	${generated_kernels_dir}/sampler_types_kernel.cpp
	${generated_kernels_dir}/sampler_funcs_kernel.cpp
	${generated_kernels_dir}/sampler_random_funcs_kernel.cpp
	${generated_kernels_dir}/sampler_sobol_funcs_kernel.cpp
	${generated_kernels_dir}/sampler_metropolis_funcs_kernel.cpp
	${generated_kernels_dir}/sampler_tilepath_funcs_kernel.cpp
	${generated_kernels_dir}/film_types_kernel.cpp
	${generated_kernels_dir}/film_funcs_kernel.cpp
	${generated_kernels_dir}/film_mergesamplebuffer_funcs_kernel.cpp
	${generated_kernels_dir}/filmdenoiser_funcs_kernel.cpp
	${generated_kernels_dir}/sampleresult_types_kernel.cpp
	${generated_kernels_dir}/plugin_backgroundimg_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_bloom_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_cameraresponse_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_coloraberration_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_gammacorrection_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_gaussianblur3x3_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_objectidmask_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_optixdenoiser_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_premultiplyalpha_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_vignetting_funcs_kernel.cpp
	${generated_kernels_dir}/plugin_whitebalance_funcs_kernel.cpp
	${generated_kernels_dir}/tonemap_reduce_funcs_kernel.cpp
	${generated_kernels_dir}/tonemap_autolinear_funcs_kernel.cpp
	${generated_kernels_dir}/tonemap_linear_funcs_kernel.cpp
	${generated_kernels_dir}/tonemap_luxlinear_funcs_kernel.cpp
	${generated_kernels_dir}/tonemap_reinhard02_funcs_kernel.cpp
	${generated_kernels_dir}/sampleresult_funcs_kernel.cpp
	${generated_kernels_dir}/filter_types_kernel.cpp
	${generated_kernels_dir}/filter_funcs_kernel.cpp
	${generated_kernels_dir}/camera_types_kernel.cpp
	${generated_kernels_dir}/camera_funcs_kernel.cpp
	${generated_kernels_dir}/bsdf_types_kernel.cpp
	${generated_kernels_dir}/bsdf_funcs_kernel.cpp
	${generated_kernels_dir}/bsdfutils_funcs_kernel.cpp
	${generated_kernels_dir}/imagemap_types_kernel.cpp
	${generated_kernels_dir}/imagemap_funcs_kernel.cpp
	${generated_kernels_dir}/material_types_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_generic_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_default_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_archglass_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_carpaint_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_clearvol_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_cloth_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_disney_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_glass_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_glossy2_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_glossycoating_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_glossytranslucent_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_heterogeneousvol_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_homogeneousvol_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_matte_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_matte_translucent_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_metal2_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_mirror_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_mix_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_null_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_roughglass_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_roughmatte_translucent_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_thinfilmcoating_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_twosided_kernel.cpp
	${generated_kernels_dir}/materialdefs_funcs_velvet_kernel.cpp
	${generated_kernels_dir}/material_funcs_evalops_kernel.cpp
	${generated_kernels_dir}/material_funcs_kernel.cpp
	${generated_kernels_dir}/texture_types_kernel.cpp
	${generated_kernels_dir}/texture_funcs_kernel.cpp
	${generated_kernels_dir}/texture_funcs_evalops_kernel.cpp
	${generated_kernels_dir}/texture_blender_noise_funcs_kernel.cpp
	${generated_kernels_dir}/texture_blender_noise_funcs2_kernel.cpp
	${generated_kernels_dir}/texture_blender_funcs_kernel.cpp
	${generated_kernels_dir}/texture_bump_funcs_kernel.cpp
	${generated_kernels_dir}/texture_noise_funcs_kernel.cpp
	${generated_kernels_dir}/texture_abs_funcs_kernel.cpp
	${generated_kernels_dir}/texture_bilerp_funcs_kernel.cpp
	${generated_kernels_dir}/texture_blackbody_funcs_kernel.cpp
	${generated_kernels_dir}/texture_bombing_funcs_kernel.cpp
	${generated_kernels_dir}/texture_brick_funcs_kernel.cpp
	${generated_kernels_dir}/texture_clamp_funcs_kernel.cpp
	${generated_kernels_dir}/texture_colordepth_funcs_kernel.cpp
	${generated_kernels_dir}/texture_densitygrid_funcs_kernel.cpp
	${generated_kernels_dir}/texture_distort_funcs_kernel.cpp
	${generated_kernels_dir}/texture_fresnelcolor_funcs_kernel.cpp
	${generated_kernels_dir}/texture_fresnelconst_funcs_kernel.cpp
	${generated_kernels_dir}/texture_hitpoint_funcs_kernel.cpp
	${generated_kernels_dir}/texture_hsv_funcs_kernel.cpp
	${generated_kernels_dir}/texture_irregulardata_funcs_kernel.cpp
	${generated_kernels_dir}/texture_imagemap_funcs_kernel.cpp
	${generated_kernels_dir}/texture_others_funcs_kernel.cpp
	${generated_kernels_dir}/texture_random_funcs_kernel.cpp
	${generated_kernels_dir}/texture_triplanar_funcs_kernel.cpp
	${generated_kernels_dir}/evalstack_funcs_kernel.cpp
	${generated_kernels_dir}/mortoncurve_funcs_kernel.cpp
	${generated_kernels_dir}/pathdepthinfo_types_kernel.cpp
	${generated_kernels_dir}/pathdepthinfo_funcs_kernel.cpp
	${generated_kernels_dir}/pathvolumeinfo_types_kernel.cpp
	${generated_kernels_dir}/pathvolumeinfo_funcs_kernel.cpp
	${generated_kernels_dir}/pathinfo_types_kernel.cpp
	${generated_kernels_dir}/pathinfo_funcs_kernel.cpp
	${generated_kernels_dir}/varianceclamping_funcs_kernel.cpp
	${generated_kernels_dir}/volume_types_kernel.cpp
	${generated_kernels_dir}/volume_funcs_kernel.cpp
	${generated_kernels_dir}/light_types_kernel.cpp
	${generated_kernels_dir}/light_funcs_kernel.cpp
	${generated_kernels_dir}/dlsc_types_kernel.cpp
	${generated_kernels_dir}/dlsc_funcs_kernel.cpp
	${generated_kernels_dir}/elvc_types_kernel.cpp
	${generated_kernels_dir}/elvc_funcs_kernel.cpp
	${generated_kernels_dir}/lightstrategy_funcs_kernel.cpp
	${generated_kernels_dir}/scene_types_kernel.cpp
	${generated_kernels_dir}/scene_funcs_kernel.cpp
	${generated_kernels_dir}/sceneobject_types_kernel.cpp
	${generated_kernels_dir}/mapping_types_kernel.cpp
	${generated_kernels_dir}/mapping_funcs_kernel.cpp
	${generated_kernels_dir}/hitpoint_types_kernel.cpp
	${generated_kernels_dir}/hitpoint_funcs_kernel.cpp
	${generated_kernels_dir}/pgic_types_kernel.cpp
	${generated_kernels_dir}/pgic_funcs_kernel.cpp
	${generated_kernels_dir}/pathoclbase_datatypes_kernel.cpp
	${generated_kernels_dir}/pathoclbase_funcs_kernel.cpp
	${generated_kernels_dir}/pathoclbase_kernels_micro_kernel.cpp
	${generated_kernels_dir}/pathtracer_types_kernel.cpp
)

SOURCE_GROUP("Source Files\\SLG Kernels\\Generated" FILES ${SLG_KERNEL_SRCS})

add_library(slg-kernels STATIC ${SLG_KERNELS} ${SLG_KERNEL_SRCS})

#############################################################################
#
# SLG Film library
#
#############################################################################

set(SLG_FILM_SRCS
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/film.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmaddsample.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmchannels.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmimagepipeline.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmimagepipelinehw.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmoutput.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmoutputs.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmparse.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmsamplescounts.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmsamplesplatter.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmserialize.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filmproperties.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/sampleresult.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/convtest/filmconvtest.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/noiseestimation/filmnoiseestimation.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/denoiser/filmdenoiser.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/denoiser/samplesaccumulator.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/filter.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/filterdistribution.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/none.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/blackmanharris.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/box.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/catmullrom.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/gaussian.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/mitchell.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/mitchellss.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/filters/sinc.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/imagepipeline.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/imagepipelineserialization.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/radiancechannelscale.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/radiancechannelscaleserialization.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/backgroundimg.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/bakemapmargin.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/bcddenoiser.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/bloom.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/cameraresponse.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/coloraberration.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/colorlut.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/contourlines.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/gammacorrection.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/gaussianblur3x3.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/mist.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/nop.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/patterns.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/objectidmask.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/outputswitcher.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/vignetting.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/premultiplyalpha.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/autolinear.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/linear.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/luxlinear.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/opencolorio.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/reinhard02.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/tonemaps/tonemap.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/intel_oidn.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/whitebalance.cpp
	${LuxCoreRender_SOURCE_DIR}/src/slg/film/imagepipeline/plugins/optixdenoiser.cpp
)
SOURCE_GROUP("Source Files\\SLG Film Library" FILES ${SLG_FILM_SRCS})

add_library(slg-film STATIC ${SLG_FILM_SRCS})

###########################################################################
#
# Predefines
#
###########################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../lib")

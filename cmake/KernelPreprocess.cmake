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

################################################################################
#
# Definition of the function for preprocessing OpenCL kernels
#
################################################################################

FUNCTION(PreprocessOCLKernel NAMESPACE KERNEL SRC DST)
	MESSAGE(STATUS "Preprocessing OpenCL kernel: " ${SRC} " => " ${DST} )

	add_custom_command(
		OUTPUT ${DST}
		COMMAND ${CMAKE_COMMAND}
			-DDST=${DST} -DSRC=${SRC}
			-DNAMESPACE=${NAMESPACE} -DKERNEL=${KERNEL}
			-P "${PROJECT_SOURCE_DIR}/cmake/Scripts/PreprocessKernel.cmake"
		MAIN_DEPENDENCY ${SRC}
	)

ENDFUNCTION(PreprocessOCLKernel)

FUNCTION(PreprocessOCLKernels)
	set(dest_dir ${ARGV0})
	set(namespace ${ARGV1})
	list(REMOVE_AT ARGV 0 1)

	foreach(kernel ${ARGV})
		get_filename_component(kernel_filename ${kernel} NAME)
		string(LENGTH ${kernel_filename} kernel_filename_length)
		math(EXPR kernel_filename_length "${kernel_filename_length} - 3")
		string(SUBSTRING ${kernel_filename} 0 ${kernel_filename_length} kernel_name)

		PreprocessOCLKernel(${namespace} ${kernel_name}
			${kernel}
			${dest_dir}/${kernel_name}_kernel.cpp)
	endforeach(kernel)
ENDFUNCTION(PreprocessOCLKernels)

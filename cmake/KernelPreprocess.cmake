################################################################################
# Copyright 1998-2013 by authors (see AUTHORS.txt)
#
#   This file is part of LuxRender.
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

	IF(WIN32)
		add_custom_command(
			OUTPUT ${DST}
			COMMAND echo \#include ^<string^> > ${DST}
			COMMAND echo namespace ${NAMESPACE} { namespace ocl { >> ${DST}
			COMMAND echo std::string KernelSource_${KERNEL} = >> ${DST}
			
			# need delayed expansion for the env variable
			# so we can use the search/replace functionality when expanding env vars
			# which for some reason doesn't work for loop vars
			# however delayed expansion relies on ! keyword which causes "lone" !'s 
			# to get eaten in the first set statement
			# so, enable after the initial set statement and disable when done
			
			#COMMAND for /F \"usebackq tokens=*\" %%a in (${SRC}) do if \"%%a\"==\"\" (echo \"\\n\" >> ${DST}) else (set line=%%a&&set line=!line:\\=\\\\!&& echo \"!line:\"=\\\"!\\n\" >> ${DST}) 
			COMMAND for /F \"usebackq tokens=*\" %%a in (${SRC}) do if \"%%a\"==\"\" (echo \"\\n\" >> ${DST}) else (
			COMMAND set line=%%a
			COMMAND setlocal ENABLEDELAYEDEXPANSION
			# escape \
			COMMAND set line=!line:\\=\\\\!
			# escape "
			COMMAND set line=!line:\"=\\\"!
			# output processed line
			COMMAND echo \"!line!\\n\" >> ${DST}
			COMMAND endlocal
			COMMAND )
			COMMAND echo \; } } >> ${DST}
			MAIN_DEPENDENCY ${SRC}
		)
	ELSE(WIN32)
		add_custom_command(
			OUTPUT ${DST}
			COMMAND echo \"\#include <string>\" > ${DST}
			COMMAND echo namespace ${NAMESPACE} \"{ namespace ocl {\" >> ${DST}
			COMMAND echo "std::string KernelSource_${KERNEL} = " >> ${DST}
			COMMAND cat ${SRC} | sed 's/\r//g' | sed 's/\\\\/\\\\\\\\/g' | sed 's/\"/\\\\\"/g' | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> ${DST}
			COMMAND echo "\; } }" >> ${DST}
			MAIN_DEPENDENCY ${SRC}
		)
	ENDIF(WIN32)
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

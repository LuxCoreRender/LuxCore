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
# Definition of the function for preprocessing OpenCL kernels
#
###########################################################################

FUNCTION(PreprocessOCLKernel NAMESPACE KERNEL SRC DST)
	MESSAGE(STATUS "Preprocessing OpenCL kernel: " ${SRC} " => " ${DST} )

	IF(WIN32)
		add_custom_command(
			OUTPUT ${DST}
			COMMAND echo "#include <string>" > ${DST}
			COMMAND echo namespace ${NAMESPACE} "{ namespace ocl {" >> ${DST}
			COMMAND echo std::string KernelSource_PathOCL_${KERNEL} = >> ${DST}
# TODO: this code need to be update in order to replace " char with \" (i.e. sed 's/"/\\"/g')
			COMMAND for /F \"usebackq tokens=*\" %%a in (${SRC}) do echo \"%%a\\n\" >> ${DST}
			COMMAND echo "; } }" >> ${DST}
			MAIN_DEPENDENCY ${SRC}
		)
	ELSE(WIN32)
		add_custom_command(
			OUTPUT ${DST}
			COMMAND echo \"\#include <string>\" > ${DST}
			COMMAND echo namespace ${NAMESPACE} \"{ namespace ocl {\" >> ${DST}
			COMMAND echo "std::string KernelSource_${KERNEL} = " >> ${DST}
			COMMAND cat ${SRC} | sed 's/\\\\/\\\\\\\\/g' | sed 's/\"/\\\\\"/g' | awk '{ printf \(\"\\"%s\\\\n\\"\\n\", $$0\) }' >> ${DST}
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

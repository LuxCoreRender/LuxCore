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

# Routines to set up build rules that copy files or entire directories to the
# build directory if they do not exist or have changed

if (BuildStage_MODULE_INCLUDED)
	return()
else()
	set(BuildStage_MODULE_INCLUDED TRUE)
endif()

function(BuildStage_files target dst_dir src_dir first_file) #...

	macro(absolute_path out base path)
		if (IS_ABSOLUTE "${path}")
			set(${out} "${path}")
		else()
			set(${out} "${base}/${path}")
		endif()
	endmacro()

	absolute_path(abs_dst ${CMAKE_CURRENT_BINARY_DIR} ${dst_dir})
	absolute_path(abs_src ${CMAKE_CURRENT_SOURCE_DIR} ${src_dir})

	set(files ${first_file} ${ARGN})

	set(dirs "")
	set(n_dirs "1")
	set(dst_files "")
	foreach(path IN LISTS files)
		absolute_path(src_path ${abs_src} ${path})

		file(RELATIVE_PATH rel_path ${abs_src} ${src_path})
		absolute_path(dst_path ${abs_dst} ${rel_path})

		get_filename_component(dir ${dst_path} DIRECTORY)
		list(FIND dirs ${dir} index)
		if (${index} EQUAL -1) # first time this directory is seen
			set(index ${n_dirs})
			math(EXPR n_dirs ${n_dirs}+1)
			list(APPEND dirs ${dir})

			add_custom_command(OUTPUT ${dir}
					COMMAND ${CMAKE_COMMAND} -E make_directory ${dir})
		endif()

		add_custom_command(OUTPUT ${dst_path}
				COMMAND ${CMAKE_COMMAND} -E copy ${src_path} ${dst_path}
				DEPENDS ${src_path} ${dir})

		list(APPEND dst_files ${dst_path})
	endforeach()

	# Silence MSB8065/MSB8064 warnings with VS2019
	set_source_files_properties(${dirs} PROPERTIES SYMBOLIC True)

	add_custom_target(${target} DEPENDS ${dst_files})

endfunction()

function(BuildStage_directory target dst_dir src_dir)

	file(GLOB_RECURSE files "${src_dir}/*.*")
	BuildStage_files(${target} ${dst_dir} ${src_dir} ${files})

endfunction()


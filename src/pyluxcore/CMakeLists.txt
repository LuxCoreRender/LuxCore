################################################################################
# Copyright 1998-2025 by authors (see AUTHORS.txt)
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
# PyLuxCore (Python bindings)
#
################################################################################

set(PYLUXCORE_SRCS
)
include_directories(${PROJECT_SOURCE_DIR}/deps/bcd-1.1/include)

pybind11_add_module(pyluxcore MODULE
  ${PROJECT_SOURCE_DIR}/src/pyluxcore/pyluxcore.cpp
  ${PROJECT_SOURCE_DIR}/src/pyluxcore/pyluxcoreforblender.cpp
)

target_link_libraries(pyluxcore PRIVATE
    luxcore_static
    OpenVDB::openvdb
    pybind11::pybind11
    Python::Module
    onetbb::onetbb
    blender-types
    robin_hood::robin_hood
    fmt::fmt
)

if(APPLE)
  target_link_libraries(pyluxcore PRIVATE llvm-openmp::llvm-openmp)
else()
  target_link_libraries(pyluxcore PRIVATE OpenMP::OpenMP_CXX)
endif(APPLE)

set_target_properties(pyluxcore PROPERTIES PREFIX "")
if(MSVC)
  set_target_properties(pyluxcore PROPERTIES SUFFIX ".pyd")
endif(MSVC)

# Install
INSTALL(TARGETS pyluxcore DESTINATION pyluxcore COMPONENT pyluxcore)

# OIDN
# oidnDenoise
add_executable(oidn-denoise IMPORTED)
set_property(TARGET oidn-denoise PROPERTY IMPORTED_LOCATION "${LUX_OIDN_DENOISE_PATH}")

if(WIN32)
  # Windows lacks rpath system, so we have to put oidnDenoise in the same
  # folder as the dlls...
  set(OIDN_DESTINATION "pyluxcore.libs")
else()
  # For Macos, reminder: don't put it in pyluxcore/.dylibs, it would prevent
  # other deps' relocation
  set(OIDN_DESTINATION "pyluxcore.oidn")
endif(WIN32)

if(LINUX)
  set_property(
    TARGET oidn-denoise
    PROPERTY INSTALL_RPATH "\\\${ORIGIN}/../pyluxcore.libs"
  )
  install(
    PROGRAMS "${LUX_OIDN_DENOISE_CPU}"
    DESTINATION "pyluxcore.libs"
    COMPONENT pyluxcore
  )
endif(LINUX)

if(APPLE)
  set_target_properties(
    oidn-denoise PROPERTIES
    INSTALL_RPATH "\\\${ORIGIN}/../pyluxcore/.pylibs"
    BUILD_RPATH ""
  )
endif(APPLE)

INSTALL(
  IMPORTED_RUNTIME_ARTIFACTS oidn-denoise
  DESTINATION ${OIDN_DESTINATION}
  PERMISSIONS
    OWNER_WRITE OWNER_READ OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
  COMPONENT pyluxcore
)


if(WIN32)
  # For Win32, we need to add device_cpu
  # device_cpu should first have a pyd extension to be repaired.
  # This will be reverted after repairing
  install(
    PROGRAMS "${LUX_OIDN_DENOISE_BINS}/OpenImageDenoise_device_cpu.dll"
    DESTINATION "pyluxcore.libs"
    RENAME "OpenImageDenoise_device_cpu.pyd"
    COMPONENT pyluxcore
  )

  # Delvewheel doesn't handle executables, however we need to delocate oidnDenoise.
  # Therefore we had to let Delvewheel believe oidnDenoise is a lib, and now
  # we have to make it an executable again...
  message(STATUS "oidnDenoise suffix back to pyd")
  INSTALL(
    CODE
    "
    MESSAGE(STATUS
        \"Renaming '$<INSTALL_PREFIX>/pyluxcore.libs/oidnDenoise.exe' \
        into '$<INSTALL_PREFIX>/pyluxcore.libs/oidnDenoise.pyd'\"
    )
    FILE(RENAME
        $<INSTALL_PREFIX>/pyluxcore.libs/oidnDenoise.exe
        $<INSTALL_PREFIX>/pyluxcore.libs/oidnDenoise.pyd
    )
    "
    COMPONENT pyluxcore
  )

endif(WIN32)


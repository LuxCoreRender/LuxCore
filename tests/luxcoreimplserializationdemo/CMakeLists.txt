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
# LuxCore library serialization demo
#
################################################################################

set(LUXCOREIMPL_SERIALIZATIONDEMO_SRCS
	luxcoreimplserializationdemo.cpp
	)

include_directories(${LuxRays_SOURCE_DIR}/deps/bcd-1.1/include)
include_directories(${LuxRays_SOURCE_DIR}/deps/opencolorio-2.0.0/include)

add_executable(luxcoreimplserializationdemo ${LUXCOREIMPL_SERIALIZATIONDEMO_SRCS})

TARGET_LINK_LIBRARIES(luxcoreimplserializationdemo luxcore slg-core slg-film slg-kernels luxrays bcd opensubdiv openvdb opencolorio ${BLOSC_LIBRARY} ${EMBREE_LIBRARY} ${OIDN_LIBRARY} ${TBB_LIBRARY} ${TIFF_LIBRARIES} ${TIFF_LIBRARIES} ${OPENEXR_LIBRARIES} ${PNG_LIBRARIES} ${JPEG_LIBRARIES})
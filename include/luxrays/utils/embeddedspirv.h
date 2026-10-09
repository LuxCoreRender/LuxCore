/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)
 *
 *   This file is part of LuxCoreRender.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ***************************************************************************/

#ifndef _LUXRAYS_EMBEDDEDSPIRV_H
#define	_LUXRAYS_EMBEDDEDSPIRV_H

#include <cstddef>
#include <string>

namespace luxrays {

// Return the embedded pre-compiled SPIR-V module with the given kernel cache
// hash (i.e. the same hash used by oclKernelPersistentCache) or nullptr if
// there isn't one. The returned data is valid for the entire life of the
// process.
//
// The modules are pre-compiled at build time by the spirv-baker tool (see
// src/slg/kernels/spirvbaker.cpp) and embedded in the binaries. The
// definition of this function is provided either by the generated registry
// (src/slg/CMakeLists.txt) or, when the SPIR-V pre-compilation is disabled,
// by a stub.
const unsigned char *GetEmbeddedSPIRVModule(const std::string &hash, std::size_t *size);

}

#endif

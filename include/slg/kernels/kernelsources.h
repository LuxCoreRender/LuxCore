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

#ifndef _SLG_KERNELSOURCES_H
#define	_SLG_KERNELSOURCES_H

#include <string>
#include <utility>
#include <vector>

namespace slg {
namespace ocl {

// Return the base source (i.e. all the shared functions and types, without
// the micro-kernels) used by PathOCL, TilePathOCL and RTPathOCL render
// engines. It is the one and only place where the list of the OpenCL kernel
// sources, in the right order, is defined: it is used both at run time (to
// compile the kernels) and at build time (by the spirv-baker tool in order to
// pre-compile the kernels in SPIR-V format).
std::string GetPathOCLBaseKernelSources();

// Split the micro-kernels source in a (kernel name, source chunk) pair for
// each __kernel. All chunks share the preamble (i.e. the text before the first
// __kernel) of the source.
std::vector<std::pair<std::string, std::string>> SplitMicroKernelSources(
	const std::string &src);

// Add the render engine kernel parameters not related to the device used.
// Used both at run time and at build time (by the spirv-baker tool); the
// spirv-baker adds the platform/vendor related parameter on its own.
void GetKernelParamters(std::vector<std::string> &params,
	const std::string &renderEngineType,
	const float epsilonMin, const float epsilonMax);

}
}

#endif

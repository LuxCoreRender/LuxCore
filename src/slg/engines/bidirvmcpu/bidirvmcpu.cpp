/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include "slg/engines/bidirvmcpu/bidirvmcpu.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// BiDirCPURenderEngine
//------------------------------------------------------------------------------

BiDirVMCPURenderEngine::BiDirVMCPURenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		BiDirCPURenderEngine(rcfg, flm, flmMutex) {
}

void BiDirVMCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	lightPathsCount = Max(1024u, cfg.Get(Property("bidirvm.lightpath.count")(16 * 1024)).Get<u_int>());
	baseRadius = cfg.Get(Property("bidirvm.startradius.scale")(.003f)).Get<float>() * renderConfig->scene->dataSet->GetBSphere().rad;
	radiusAlpha = cfg.Get(Property("bidirvm.alpha")(.95f)).Get<float>();

	BiDirCPURenderEngine::StartLockLess();
}

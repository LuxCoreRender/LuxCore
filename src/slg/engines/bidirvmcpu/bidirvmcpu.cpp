/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

BiDirVMCPURenderEngine::BiDirVMCPURenderEngine(const RenderConfig *rcfg) :
		BiDirCPURenderEngine(rcfg) {
}

void BiDirVMCPURenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	lightPathsCount = Max(1024u, cfg.Get(GetDefaultProps().Get("bidirvm.lightpath.count")).Get<u_int>());
	baseRadius = cfg.Get(GetDefaultProps().Get("bidirvm.startradius.scale")).Get<float>() * renderConfig->scene->dataSet->GetBSphere().rad;
	radiusAlpha = cfg.Get(GetDefaultProps().Get("bidirvm.alpha")).Get<float>();

	BiDirCPURenderEngine::StartLockLess();
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties BiDirVMCPURenderEngine::ToProperties(const Properties &cfg) {
	return BiDirCPURenderEngine::ToProperties(cfg) <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("bidirvm.lightpath.count")) <<
			cfg.Get(GetDefaultProps().Get("bidirvm.startradius.scale")) <<
			cfg.Get(GetDefaultProps().Get("bidirvm.alpha"));
}

RenderEngine *BiDirVMCPURenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new BiDirVMCPURenderEngine(rcfg);
}

const Properties &BiDirVMCPURenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			BiDirCPURenderEngine::GetDefaultProps() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("bidirvm.lightpath.count")(16 * 1024) <<
			Property("bidirvm.startradius.scale")(.003f) <<
			Property("bidirvm.alpha")(.95f);

	return props;
}

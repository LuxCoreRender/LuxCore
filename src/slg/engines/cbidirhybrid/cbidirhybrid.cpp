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

#include "slg/engines/cbidirhybrid/cbidirhybrid.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// CBiDirHybridRenderEngine
//------------------------------------------------------------------------------

CBiDirHybridRenderEngine::CBiDirHybridRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		BiDirHybridRenderEngine(rcfg, flm, flmMutex) {
}

void CBiDirHybridRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	eyePathCount = cfg.Get(Property("cbidir.eyepath.count")(5)).Get<int>();
	lightPathCount = cfg.Get(Property("cbidir.lightpath.count")(5)).Get<int>();

	BiDirHybridRenderEngine::StartLockLess();
}

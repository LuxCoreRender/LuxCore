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

#ifndef _SLG_RENDERENGINEREGISTRY_H
#define	_SLG_RENDERENGINEREGISTRY_H

#include <string>

#include "slg/core/objectstaticregistry.h"
#include "slg/engines/pathocl/pathocl.h"
#include "slg/engines/rtpathocl/rtpathocl.h"
#include "slg/engines/lightcpu/lightcpu.h"
#include "slg/engines/pathcpu/pathcpu.h"
#include "slg/engines/rtpathcpu/rtpathcpu.h"
#include "slg/engines/bidircpu/bidircpu.h"
#include "slg/engines/bidirvmcpu/bidirvmcpu.h"
#include "slg/engines/filesaver/filesaver.h"
#include "slg/engines/tilepathcpu/tilepathcpu.h"
#include "slg/engines/tilepathocl/tilepathocl.h"
#include "slg/engines/bakecpu/bakecpu.h"

namespace slg {

//------------------------------------------------------------------------------
// RenderEngineRegistry
//------------------------------------------------------------------------------

class RenderEngineRegistry {
protected:
	RenderEngineRegistry() { }

	//--------------------------------------------------------------------------

	typedef RenderEngineType ObjectType;
	// Used to register all sub-class String2EngineType() static methods
	typedef RenderEngineType (*GetObjectType)();
	// Used to register all sub-class EngineType2String() static methods
	typedef std::string (*GetObjectTag)();
	// Used to register all sub-class ToProperties() static methods
	typedef luxrays::Properties (*ToProperties)(const luxrays::Properties &cfg);
	// Used to register all sub-class FromProperties() static methods
	typedef RenderEngine *(*FromProperties)(const RenderConfig *rcfg);
	// Used to register all sub-class FromPropertiesOCL() static methods
	typedef std::string (*FromPropertiesOCL)(const luxrays::Properties &cfg);

	OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(RenderEngineRegistry);

	//--------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, PathOCLRenderEngine);
#endif
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, LightCPURenderEngine);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, PathCPURenderEngine);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, BiDirCPURenderEngine);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, BiDirVMCPURenderEngine);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, FileSaverRenderEngine);
#if !defined(LUXRAYS_DISABLE_OPENCL)
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, RTPathOCLRenderEngine);
#endif
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, TilePathCPURenderEngine);
#if !defined(LUXRAYS_DISABLE_OPENCL)
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, TilePathOCLRenderEngine);
#endif
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, RTPathCPURenderEngine);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(RenderEngineRegistry, BakeCPURenderEngine);
	// Just add here any new Engine (don't forget in the .cpp too)

	friend class RenderEngine;
};

}

#endif	/* _SLG_RENDERENGINEREGISTRY_H */

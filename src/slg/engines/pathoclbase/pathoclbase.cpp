/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <string>
#include <sstream>
#include <stdexcept>

#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/oclintersectiondevice.h"

#include "slg/slg.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/film/filter.h"
#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseRenderEngine
//------------------------------------------------------------------------------

PathOCLBaseRenderEngine::PathOCLBaseRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex,
		const bool realTime) : OCLRenderEngine(rcfg, flm, flmMutex) {
	film->AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film->SetOverlappedScreenBufferUpdateFlag(true);
	if (realTime) {
		film->SetRadianceGroupCount(1);

		// Remove all Film channels
		film->RemoveChannel(Film::ALPHA);
		film->RemoveChannel(Film::DEPTH);
		film->RemoveChannel(Film::POSITION);
		film->RemoveChannel(Film::GEOMETRY_NORMAL);
		film->RemoveChannel(Film::SHADING_NORMAL);
		film->RemoveChannel(Film::MATERIAL_ID);
		film->RemoveChannel(Film::DIRECT_DIFFUSE);
		film->RemoveChannel(Film::DIRECT_GLOSSY);
		film->RemoveChannel(Film::EMISSION);
		film->RemoveChannel(Film::INDIRECT_DIFFUSE);
		film->RemoveChannel(Film::INDIRECT_GLOSSY);
		film->RemoveChannel(Film::INDIRECT_SPECULAR);
		film->RemoveChannel(Film::MATERIAL_ID_MASK);
		film->RemoveChannel(Film::DIRECT_SHADOW_MASK);
		film->RemoveChannel(Film::INDIRECT_SHADOW_MASK);
		film->RemoveChannel(Film::UV);
		film->RemoveChannel(Film::RAYCOUNT);
	} else
		film->SetRadianceGroupCount(rcfg->scene->lightGroupCount);
	film->Init();

	const Properties &cfg = renderConfig->cfg;
	compiledScene = NULL;

	//--------------------------------------------------------------------------
	// Allocate devices
	//--------------------------------------------------------------------------

	std::vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDeviceDescs);

	// Check if I have to disable image storage and set max. QBVH stack size
	const bool enableImageStorage = cfg.GetBoolean("accelerator.imagestorage.enable", true);
	const size_t qbvhStackSize = cfg.GetInt("accelerator.qbvh.stacksize.max",
			OCLRenderEngine::GetQBVHEstimatedStackSize(*(renderConfig->scene->dataSet)));
	SLG_LOG("OpenCL Devices used:");
	for (size_t i = 0; i < devs.size(); ++i) {
		SLG_LOG("[" << devs[i]->GetName() << "]");
		devs[i]->SetMaxStackSize(qbvhStackSize);
		intersectionDevices.push_back(devs[i]);

		OpenCLIntersectionDevice *oclIntersectionDevice = (OpenCLIntersectionDevice *)(devs[i]);
		oclIntersectionDevice->SetEnableImageStorage(enableImageStorage);
		// Disable the support for hybrid rendering
		oclIntersectionDevice->SetDataParallelSupport(false);

		// Check if OpenCL 1.1 is available
		SLG_LOG("  Device OpenCL version: " << oclIntersectionDevice->GetDeviceDesc()->GetOpenCLVersion());
		if (!oclIntersectionDevice->GetDeviceDesc()->IsOpenCL_1_1()) {
			// NVIDIA drivers report OpenCL 1.0 even if they are 1.1 so I just
			// print a warning instead of throwing an exception
			SLG_LOG("WARNING: OpenCL version 1.1 or better is required. Device " + devs[i]->GetName() + " may not work.");
		}
	}

	// Set the LuxRays SataSet
	ctx->SetDataSet(renderConfig->scene->dataSet);

	//--------------------------------------------------------------------------
	// Setup render threads array
	//--------------------------------------------------------------------------

	const size_t renderThreadCount = intersectionDevices.size();
	SLG_LOG("Configuring "<< renderThreadCount << " CPU render threads");
	renderThreads.resize(renderThreadCount, NULL);
}

PathOCLBaseRenderEngine::~PathOCLBaseRenderEngine() {
	if (editMode)
		EndEdit(EditActionList());
	if (started)
		Stop();

	for (size_t i = 0; i < renderThreads.size(); ++i)
		delete renderThreads[i];

	delete compiledScene;
}

void PathOCLBaseRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	if (cfg.IsDefined("opencl.memory.maxpagesize"))
		maxMemPageSize = cfg.GetSize("opencl.memory.maxpagesize", 512 * 1024 * 1024);
	else {
		// Look for the max. page size allowed
		maxMemPageSize = ((OpenCLIntersectionDevice *)(intersectionDevices[0]))->GetDeviceDesc()->GetMaxMemoryAllocSize();
		for (u_int i = 1; i < intersectionDevices.size(); ++i)
			maxMemPageSize = Min(maxMemPageSize, ((OpenCLIntersectionDevice *)(intersectionDevices[i]))->GetDeviceDesc()->GetMaxMemoryAllocSize());
	}
	SLG_LOG("[PathOCLBaseRenderEngine] OpenCL max. page memory size: " << maxMemPageSize / 1024 << "Kbytes");

	//--------------------------------------------------------------------------
	// Compile the scene
	//--------------------------------------------------------------------------

	compiledScene = new CompiledScene(renderConfig->scene, film, maxMemPageSize);

	//--------------------------------------------------------------------------
	// Start render threads
	//--------------------------------------------------------------------------

	const size_t renderThreadCount = intersectionDevices.size();
	SLG_LOG("Starting "<< renderThreadCount << " PathOCL render threads");
	for (size_t i = 0; i < renderThreadCount; ++i) {
		if (!renderThreads[i]) {
			renderThreads[i] = CreateOCLThread(i,
					(OpenCLIntersectionDevice *)(intersectionDevices[i]));
		}
	}

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Start();
}

void PathOCLBaseRenderEngine::StopLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Stop();

	delete compiledScene;
	compiledScene = NULL;
}

void PathOCLBaseRenderEngine::BeginEditLockLess() {
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->Interrupt();
	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->BeginEdit();
}

void PathOCLBaseRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	compiledScene->Recompile(editActions);

	for (size_t i = 0; i < renderThreads.size(); ++i)
		renderThreads[i]->EndEdit(editActions);
}

#endif

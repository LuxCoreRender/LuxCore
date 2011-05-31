/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#include "renderengine.h"
#include "renderconfig.h"

#include "luxrays/core/intersectiondevice.h"

//------------------------------------------------------------------------------
// RenderEngine
//------------------------------------------------------------------------------

RenderEngine::RenderEngine(RenderConfig *cfg, NativeFilm *flm, boost::mutex *flmMutex) {
	renderConfig = cfg;
	film = flm;
	filmMutex = flmMutex;
	started = false;
	editMode = false;
}

RenderEngine::~RenderEngine() {
	if (editMode)
		EndEditLockLess(EditActionList());
	if (started)
		StopLockLess();
}

void RenderEngine::StartLockLess() {
	assert (!started);
	started = true;
}

void RenderEngine::StopLockLess() {
	assert (started);
	started = false;
}

void RenderEngine::BeginEditLockLess() {
	assert (started);
	assert (!editMode);

	editMode = true;
}

void RenderEngine::EndEditLockLess(const EditActionList &editActions) {
	assert (started);
	assert (editMode);

	editMode = false;
}

//------------------------------------------------------------------------------
// OCLRenderEngine
//------------------------------------------------------------------------------

OCLRenderEngine::OCLRenderEngine(RenderConfig *rcfg, NativeFilm *flm, boost::mutex *flmMutex) :
	RenderEngine(rcfg, flm, flmMutex) {
	const Properties &cfg = renderConfig->cfg;

	const bool useCPUs = (cfg.GetInt("opencl.cpu.use", 1) != 0);
	const bool useGPUs = (cfg.GetInt("opencl.gpu.use", 1) != 0);
	const unsigned int forceGPUWorkSize = cfg.GetInt("opencl.gpu.workgroup.size", 64);
	const int oclPlatformIndex = cfg.GetInt("opencl.platform.index", -1);
	const string oclDeviceConfig = cfg.GetString("opencl.devices.select", "");

	// Create LuxRays context
	ctx = new Context(DebugHandler, oclPlatformIndex);

	// Start OpenCL devices
	std::vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
	DeviceDescription::Filter(DEVICE_TYPE_OPENCL, descs);

	// Device info
	bool haveSelectionString = (oclDeviceConfig.length() > 0);
	if (haveSelectionString && (oclDeviceConfig.length() != descs.size())) {
		stringstream ss;
		ss << "OpenCL device selection string has the wrong length, must be " <<
				descs.size() << " instead of " << oclDeviceConfig.length();
		throw runtime_error(ss.str().c_str());
	}

	std::vector<DeviceDescription *> selectedDescs;

	for (size_t i = 0; i < descs.size(); ++i) {
		OpenCLDeviceDescription *desc = (OpenCLDeviceDescription *)descs[i];

		if (haveSelectionString) {
			if (oclDeviceConfig.at(i) == '1') {
				if (desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				selectedDescs.push_back(desc);
			}
		} else {
			if ((useCPUs && desc->GetOpenCLType() == OCL_DEVICE_TYPE_CPU) ||
					(useGPUs && desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)) {
				if (desc->GetOpenCLType() == OCL_DEVICE_TYPE_GPU)
					desc->SetForceWorkGroupSize(forceGPUWorkSize);
				selectedDescs.push_back(descs[i]);
			}
		}
	}

	if (selectedDescs.size() == 0)
		throw runtime_error("No OpenCL device selected or available");

	// Allocate devices
	std::vector<IntersectionDevice *> devs = ctx->AddIntersectionDevices(selectedDescs);

	for (size_t i = 0; i < devs.size(); ++i)
		oclIntersectionDevices.push_back((OpenCLIntersectionDevice *)devs[i]);

	SLG_LOG("OpenCL Devices used:");
	for (size_t i = 0; i < oclIntersectionDevices.size(); ++i)
		SLG_LOG("[" << oclIntersectionDevices[i]->GetName() << "]");

	// Check if I have to disable image storage
	const bool frocedDisableImageStorage = (renderConfig->scene->GetAccelType() == 2);
	for (size_t i = 0; i < oclIntersectionDevices.size(); ++i)
		oclIntersectionDevices[i]->SetQBVHDisableImageStorage(frocedDisableImageStorage);

	// Set the Luxrays SataSet
	renderConfig->scene->UpdateDataSet(ctx);
	ctx->SetDataSet(renderConfig->scene->dataSet);

	// Disable the support for hybrid rendering
	for (size_t i = 0; i < oclIntersectionDevices.size(); ++i)
		oclIntersectionDevices[i]->SetHybridRenderingSupport(false);
}

OCLRenderEngine::~OCLRenderEngine() {
	if (editMode)
		EndEditLockLess(EditActionList());
	if (started)
		StopLockLess();
}

void OCLRenderEngine::StartLockLess() {
	RenderEngine::StartLockLess();

	ctx->Start();
}

void OCLRenderEngine::StopLockLess() {
	RenderEngine::StopLockLess();

	ctx->Stop();
}

void OCLRenderEngine::BeginEditLockLess() {
	RenderEngine::BeginEditLockLess();

	// Stop all intersection devices
	ctx->Stop();
}

void OCLRenderEngine::EndEditLockLess(const EditActionList &editActions) {
	RenderEngine::EndEditLockLess(editActions);

	bool dataSetUpdated;
	if (editActions.Has(GEOMETRY_EDIT) ||
			((renderConfig->scene->dataSet->GetAcceleratorType() != ACCEL_MQBVH) &&
			editActions.Has(INSTANCE_TRANS_EDIT))) {
		// To avoid reference to the DataSet de-allocated inside UpdateDataSet()
		ctx->SetDataSet(NULL);

		// For all other accelerator, I have to rebuild the DataSet
		renderConfig->scene->UpdateDataSet(ctx);

		// Set the Luxrays SataSet
		ctx->SetDataSet(renderConfig->scene->dataSet);

		dataSetUpdated = true;
	} else
		dataSetUpdated = false;

	// Restart all intersection devices
	ctx->Start();

	if (!dataSetUpdated && (renderConfig->scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) &&
			editActions.Has(INSTANCE_TRANS_EDIT)) {
		// Update the DataSet
		ctx->UpdateDataSet();
	}
}

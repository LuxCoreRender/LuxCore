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

#ifndef _RENDERCONFIG_H
#define	_RENDERCONFIG_H

#include "scene.h"
#include "renderthread.h"
#include "displayfunc.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/core/virtualdevice.h"
#include "luxrays/utils/properties.h"

// Must be a multiple of 1024 (for AMD CPU device) and 64 (for most GPUs)
#define RAY_BUFFER_SIZE (65536)

extern string SLG_LABEL;

class RenderingConfig {
public:
	RenderingConfig(const bool lowLatency, const string &sceneFileName, const unsigned int w,
		const unsigned int h, const unsigned int nativeThreadCount,
		const bool useCPUs, const bool useGPUs,
		const unsigned int forceGPUWorkSize);

	RenderingConfig(const string &fileName);

	~RenderingConfig();

	void Init();

	void Init(const bool lowLatency, const string &sceneFileName, const unsigned int w,
		const unsigned int h, const unsigned int nativeThreadCount,
		const bool useCPUs, const bool useGPUs,
		const unsigned int forceGPUWorkSize, const unsigned int filmType,
		const unsigned int oclPlatformIndex = 0,
		const unsigned int oclDeviceThreads = 0, const string &oclDeviceConfig = "");

	void ReInit(const bool reallocBuffers, const unsigned int w = 0, unsigned int h = 0);

	void SetMaxPathDepth(const int delta);

	void SetShadowRays(const int delta);

	const vector<IntersectionDevice *> &GetIntersectionDevices() { return intersectionCPUGPUDevices; }
	const vector<RenderThread *> &GetRenderThreads() { return renderThreads; }

	Properties cfg;

	char captionBuffer[512];
	unsigned int screenRefreshInterval;

	Scene *scene;
	Film *film;

private:
	void SetUpOpenCLDevices(const bool lowLatency, const bool useCPUs, const bool useGPUs,
		const unsigned int forceGPUWorkSize, const unsigned int oclDeviceThreads, const string &oclDeviceConfig);

	void SetUpNativeDevices(const unsigned int nativeThreadCount);

	void StartAllRenderThreads();

	void StopAllRenderThreads();

	Context *ctx;

	vector<RenderThread *> renderThreads;

	vector<IntersectionDevice *> intersectionGPUDevices;
	vector<IntersectionDevice *> intersectionCPUDevices;
	vector<IntersectionDevice *> intersectionCPUGPUDevices;

	vector<IntersectionDevice *> intersectionAllDevices;
};

#endif	/* _RENDERCONFIG_H */

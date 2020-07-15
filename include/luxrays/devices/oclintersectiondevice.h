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

#ifndef _LUXRAYS_OCLINTERSECTIONDEVICE_H
#define	_LUXRAYS_OCLINTERSECTIONDEVICE_H

#include "luxrays/devices/ocldevice.h"
#include "luxrays/core/hardwareintersectiondevice.h"
#include "luxrays/utils/oclerror.h"
#include "luxrays/utils/oclcache.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays {

//------------------------------------------------------------------------------
// OpenCLIntersectionDevice
//------------------------------------------------------------------------------

class OpenCLIntersectionDevice : public OpenCLDevice, public HardwareIntersectionDevice {
public:
	OpenCLIntersectionDevice(const Context *context,
		OpenCLDeviceDescription *desc, const size_t devIndex);
	virtual ~OpenCLIntersectionDevice();

	virtual void SetDataSet(DataSet *newDataSet);
	virtual void Start();
	virtual void Stop();

	//--------------------------------------------------------------------------
	// Data parallel interface: to trace a multiple rays (i.e. on the GPU)
	//--------------------------------------------------------------------------

	virtual void EnqueueTraceRayBuffer(HardwareDeviceBuffer *rayBuff,
			HardwareDeviceBuffer *rayHitBuff,
			const unsigned int rayCount);

	friend class Context;

protected:
	virtual void Update();

	HardwareIntersectionKernel *kernel;
};

}

#endif

#endif	/* _LUXRAYS_OCLINTERSECTIONDEVICE_H */


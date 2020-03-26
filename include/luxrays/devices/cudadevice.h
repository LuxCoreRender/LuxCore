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

#ifndef _LUXRAYS_CUDADEVICE_H
#define	_LUXRAYS_CUDADEVICE_H

#include "luxrays/core/hardwaredevice.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/cuda.h"

#if defined(LUXRAYS_ENABLE_CUDA)

namespace luxrays {

//------------------------------------------------------------------------------
// CUDADeviceDescription
//------------------------------------------------------------------------------

class CUDADeviceDescription : public DeviceDescription {
public:
	CUDADeviceDescription(CUdevice cudaDevice, const size_t devIndex);
	virtual ~CUDADeviceDescription();

	virtual int GetComputeUnits() const;
	virtual u_int GetNativeVectorWidthFloat() const;
	virtual size_t GetMaxMemory() const;
	virtual size_t GetMaxMemoryAllocSize() const;

	friend class Context;

protected:
	static void AddDeviceDescs(std::vector<DeviceDescription *> &descriptions);

	size_t deviceIndex;

private:
	CUdevice cudaDevice;
};

}

#endif

#endif	/* _LUXRAYS_CUDADEVICE_H */

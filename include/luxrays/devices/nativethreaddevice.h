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

#ifndef _LUXRAYS_NATIVETHREADDEVICE_H
#define	_LUXRAYS_NATIVETHREADDEVICE_H

#include <string>
#include <cstdlib>

#include <boost/thread/thread.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/dataset.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/utils.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Native thread devices
//------------------------------------------------------------------------------

class NativeThreadDeviceDescription : public DeviceDescription {
public:
	NativeThreadDeviceDescription(const std::string deviceName) :
		DeviceDescription(deviceName, DEVICE_TYPE_NATIVE_THREAD) { }

	friend class Context;

protected:
	static void AddDeviceDescs(std::vector<DeviceDescription *> &descriptions);
};

}

#endif	/* _LUXRAYS_DEVICE_H */
_LUXRAYS_NATIVETHREADDEVICE_H
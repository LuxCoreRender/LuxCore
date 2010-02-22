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

#ifndef _LUXRAYS_CONTEXT_H
#define	_LUXRAYS_CONTEXT_H

#include <cstdlib>
#include <sstream>
#include <ostream>

#define __CL_ENABLE_EXCEPTIONS
#define __NO_STD_VECTOR
#define __NO_STD_STRING

#if defined(__APPLE__)
#include <OpenCL/cl.hpp>
#else
#include <CL/cl.hpp>
#endif

#include "luxrays/luxrays.h"
#include "luxrays/core/dataset.h"

namespace luxrays {

typedef void (*LuxRaysDebugHandler)(const char *msg);

#define LR_LOG(c, a) { if (c->HasDebugHandler()) { std::stringstream _LR_LOG_LOCAL_SS; _LR_LOG_LOCAL_SS << a; c->PrintDebugMsg(_LR_LOG_LOCAL_SS.str().c_str()); } }

class DeviceDescription;
class OpenCLDeviceDescription;

class Context {
public:
	Context(LuxRaysDebugHandler handler = NULL, const int openclPlatformIndex = -1);
	~Context();

	const std::vector<DeviceDescription *> &GetAvailableDeviceDescriptions() const;
	const std::vector<IntersectionDevice *> &GetIntersectionDevices() const;
	const std::vector<IntersectionDevice *> &GetIntersectionDevices(DeviceDescription type) const;

	std::vector<IntersectionDevice *> AddIntersectionDevices(const std::vector<DeviceDescription *> &deviceDesc);
	std::vector<IntersectionDevice *> AddVirtualM2MIntersectionDevices(const unsigned int count,
		const std::vector<DeviceDescription *> &deviceDesc);

	void SetDataSet(const DataSet *dataSet);
	void Start();
	void Interrupt();
	void Stop();

	bool HasDebugHandler() const { return debugHandler != NULL; }
	void PrintDebugMsg(const char *msg) const {
		if (debugHandler)
			debugHandler(msg);
	}

	friend class OpenCLIntersectionDevice;

protected:
	// OpenCL items
	cl::Platform oclPlatform;

private:
	std::vector<IntersectionDevice *> CreateIntersectionDevices(const std::vector<DeviceDescription *> &deviceDesc);

	LuxRaysDebugHandler debugHandler;

	const DataSet *currentDataSet;
	std::vector<DeviceDescription *> deviceDescriptions;

	// All devices
	std::vector<IntersectionDevice *> devices;
	// Virtual devices
	std::vector<VirtualM2OIntersectionDevice *> m2mDevices;

	bool started;
};

}

#endif	/* _LUXRAYS_CONTEXT_H */

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

/*!
 * \file
 *
 * \brief LuxRays is a library dedicated to accelerate the ray intersection process by using GPUs.
 * \author Bucciarelli David, Ducharme Alain, Verwiebe Jens, Bech Tom et al.
 * \version 1.0
 * \date March 2010
 */

/*!
 * \mainpage
 * \section intro Introduction
 * LuxRays is a library dedicated to accelerate the ray intersection process by
 * using OpenCL devices (i.e. GPUs, CPUs, etc.).
 */

#ifndef _LUXRAYS_CONTEXT_H
#define	_LUXRAYS_CONTEXT_H

#include <cstdlib>
#include <sstream>
#include <ostream>

#include "luxrays/luxrays.h"
#include "luxrays/core/dataset.h"

namespace luxrays {

typedef void (*LuxRaysDebugHandler)(const char *msg);

#define LR_LOG(c, a) { if (c->HasDebugHandler() && c->IsVerbose()) { std::stringstream _LR_LOG_LOCAL_SS; _LR_LOG_LOCAL_SS << a; c->PrintDebugMsg(_LR_LOG_LOCAL_SS.str().c_str()); } }

class DeviceDescription;
class OpenCLDeviceDescription;
class OpenCLIntersectionDevice;

/*!
 * \brief Interface to all main LuxRays functions.
 *
 * A Context is the main tool to access all LuxRays functionalities. It includes
 * methods to list and create devices, to define the data set to use and to
 * start/stop all the activities.
 */
class Context {
public:
	/*!
	 * \brief Construct a new LuxRays Context for the optionally defined OpenCL platform.
	 *
	 * \param handler is an optional pointer to a debug message handler. I can be NULL.
	 * \param openclPlatformIndex is the index of the OpenCL platform to use (the
	 * order is the one returned by cl::Platform::get() function). If the values is -1,
	 * the all the available platforms will be selected.
	 * \param verbose is an optional flag to enable/disable the log print of information
	 * related to the available devices.
	 */
	Context(LuxRaysDebugHandler handler = NULL, const int openclPlatformIndex = -1,
			const bool verbose = true);

	/*!	\brief Free all Context associated resources.
	 */
	~Context();

	//--------------------------------------------------------------------------
	// Methods dedicated to device listing and creation
	//--------------------------------------------------------------------------

	/*!
	 * \brief Return a list of all available device descriptions within the Context.
	 *
	 * \return the vector of all DeviceDescription available.
	 */
	const std::vector<DeviceDescription *> &GetAvailableDeviceDescriptions() const;

	/*!
	 * \brief Return a list of all intersection device created within the Context.
	 *
	 * \return the vector of all IntersectionDevice in the Context.
	 */
	const std::vector<IntersectionDevice *> &GetIntersectionDevices() const;

	/*!
	 * \brief Create an IntersectionDevice within the Context.
	 *
	 * \param deviceDesc is a DeviceDescription vector of the devices to create
	 *
	 * \return the vector of all IntersectionDevice created.
	 */
	std::vector<IntersectionDevice *> AddIntersectionDevices(std::vector<DeviceDescription *> &deviceDescs);

	/*!
	 * \brief Create a Virtual IntersectionDevice within the Context.
	 *
	 * Create an Virtual IntersectionDevice. This kind of device is
	 * useful when you have multiple threads producing work for multiple GPUs. All
	 * the routing of the work to the least busy GPU is handled by LuxRays.
	 *
	 * \param deviceDescs is a DeviceDescription vector of the devices used by virtual devices.
	 *
	 * \return the vector of all real IntersectionDevice created from deviceDescs. They are
	 * deleted once the virtual device is deleted.
	 */
	std::vector<IntersectionDevice *> AddVirtualIntersectionDevice(std::vector<DeviceDescription *> &deviceDescs);

	//--------------------------------------------------------------------------
	// Methods dedicated to DataSet definition
	//--------------------------------------------------------------------------

	DataSet *GetCurrentDataSet() const { return currentDataSet; }

	void SetDataSet(DataSet *dataSet);
	void UpdateDataSet();

	//--------------------------------------------------------------------------
	// Methods dedicated to Context management (i.e. start/stop, etc.)
	//--------------------------------------------------------------------------

	void Start();
	void Interrupt();
	void Stop();
	bool IsRunning() const { return started; }

	//--------------------------------------------------------------------------
	// Methods dedicated to message debug handling
	//--------------------------------------------------------------------------

	bool HasDebugHandler() const { return debugHandler != NULL; }
	void PrintDebugMsg(const char *msg) const {
		if (debugHandler)
			debugHandler(msg);
	}

	void SetVerbose(const bool v) { verbose = v; }
	bool IsVerbose() const { return verbose; }

#if !defined(LUXRAYS_DISABLE_OPENCL)
	friend class OpenCLIntersectionDevice;
#endif

private:
	std::vector<IntersectionDevice *> CreateIntersectionDevices(
		std::vector<DeviceDescription *> &deviceDesc, const size_t indexOffset);

	LuxRaysDebugHandler debugHandler;

	DataSet *currentDataSet;
	std::vector<DeviceDescription *> deviceDescriptions;

	// All intersection devices (including virtual)
	std::vector<IntersectionDevice *> idevices;

	bool started, verbose;
};

}

#endif	/* _LUXRAYS_CONTEXT_H */

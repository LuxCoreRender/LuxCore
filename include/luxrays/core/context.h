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

/*! \file
 *
 *   \brief LuxRays is a library dedicated to accelerate the ray intersection process by using GPUs.
 *   \author BUCCIARELLI David, DUCHARME Alain, VERWIEBE Jens, BECH Tom et al.
 *   \version 1.0
 *   \date March 2010
 *
 */

/*! \mainpage
 * \section intro Introduction
 * LuxRays is a library dedicated to accelerate the ray intersection process by
 * using GPUs.
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

#define LR_LOG(c, a) { if (c->HasDebugHandler()) { std::stringstream _LR_LOG_LOCAL_SS; _LR_LOG_LOCAL_SS << a; c->PrintDebugMsg(_LR_LOG_LOCAL_SS.str().c_str()); } }

class DeviceDescription;
class OpenCLDeviceDescription;
class OpenCLIntersectionDevice;

/*! \brief Interface to all main LuxRays functions.
 *
 * A Context is the main tool to access all LuxRays functionalities. It includes
 * methods to list and create devices, to define the data set to use and to
 * start/stop all the activities.
 */
class Context {
public:
	/*! \brief Construct a new LuxRays Context for the optionally defined OpenCL platform.
	 *
	 * \param handler is an optional pointer to a debug message handler. I can be NULL.
	 * \param openclPlatformIndex is the index of the OpenCL platform to use (the
	 *	order is the one returned by cl::Platform::get() function). If the values is -1,
	 *	the first available platform is selected.
	 */
	Context(LuxRaysDebugHandler handler = NULL, const int openclPlatformIndex = -1);

	/*!	\brief Free all Context associated resources.
	 */
	~Context();

	//--------------------------------------------------------------------------
	// Methods dedicated to device listing and creation
	//--------------------------------------------------------------------------

	/*!	\brief Return a list of all aviable device descriptions within the Context.
	 *
	 *	\return the vector of all DeviceDescription available.
	 */
	const std::vector<DeviceDescription *> &GetAvailableDeviceDescriptions() const;

	/*!	\brief Return a list of all intersection device created within the Context.
	 *
	 *	\return the vector of all IntersectionDevice in the Context.
	 */
	const std::vector<IntersectionDevice *> &GetIntersectionDevices() const;

	/*!	\brief Return a list of all VirtualM2O intersection device created within the Context.
	 *
	 *	\return the vector of all VirtualM2O in the Context.
	 */
	const std::vector<VirtualM2OHardwareIntersectionDevice *> &GetVirtualM2OIntersectionDevices() const;

	/*!	\brief Return a list of all VirtualM2O intersection device created within the Context.
	 *
	 *	\return the vector of all VirtualM2O in the Context.
	 */
	const std::vector<VirtualM2MHardwareIntersectionDevice *> &GetVirtualM2MIntersectionDevices() const;

	/*!	\brief Return a list of all pixel device created within the Context.
	 *
	 *	\return the vector of all PixelDevice in the Context.
	 */
	const std::vector<PixelDevice *> &GetPixelDevices() const;

	/*!	\brief Create an IntersectionDevice within the Context.
	 *
	 *	\param deviceDesc is a DeviceDescription vector of the devices to create
	 *
	 *	\return the vector of all IntersectionDevice created.
	 */
	std::vector<IntersectionDevice *> AddIntersectionDevices(std::vector<DeviceDescription *> &deviceDescs);

	/*!	\brief Create an Virtual Many-To-Many IntersectionDevice within the Context.
	 *
	 *	Create an Virtual Many-To-Many IntersectionDevice. This kind of device is
	 *	useful when you have multiple threads producing work for multiple GPUs. All
	 *	the routing of the work to the least busy GPU is handled by LuxRays.
	 *
	 *	\param count is the number of virtual devices to create.
	 *	\param deviceDescs is a DeviceDescription vector of the devices used by virtual devices.
	 *
	 *	\return the vector of all virtual IntersectionDevice created.
	 */
	std::vector<IntersectionDevice *> AddVirtualM2MIntersectionDevices(const unsigned int count,
		std::vector<DeviceDescription *> &deviceDescs);

	/*!	\brief Create an Virtual Many-To-One IntersectionDevice within the Context.
	 *
	 *	Create an Virtual Many-To-One IntersectionDevice. This kind of device is
	 *	useful when you have multiple threads producing work for a single GPU. All
	 *	the routing of the work forward to the GPU and backward is handled by LuxRays.
	 *
	 *	\param count is the number of virtual devices to create.
	 *	\param deviceDescs is a DeviceDescription vector of the devices used by virtual devices.
	 *
	 *	\return the vector of all virtual IntersectionDevice created.
	 */
	std::vector<IntersectionDevice *> AddVirtualM2OIntersectionDevices(const unsigned int count,
		std::vector<DeviceDescription *> &deviceDescs);

	/*!	\brief Create an PixelDevice within the Context.
	 *
	 *	\param deviceDesc is a DeviceDescription vector of the devices to create
	 *
	 *	\return the vector of all PixelDevice created.
	 */
	std::vector<PixelDevice *> AddPixelDevices(std::vector<DeviceDescription *> &deviceDescs);

	//--------------------------------------------------------------------------
	// Methods dedicated to DataSet definition
	//--------------------------------------------------------------------------

	void SetDataSet(DataSet *dataSet);
	void UpdateDataSet();

	//--------------------------------------------------------------------------
	// Methods dedicated to Context mangement (i.e. start/stop, etc.)
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

	friend class OpenCLIntersectionDevice;

private:
	std::vector<IntersectionDevice *> CreateIntersectionDevices(std::vector<DeviceDescription *> &deviceDesc);
	std::vector<PixelDevice *> CreatePixelDevices(std::vector<DeviceDescription *> &deviceDesc);

	LuxRaysDebugHandler debugHandler;

	DataSet *currentDataSet;
	std::vector<DeviceDescription *> deviceDescriptions;

	// All intersection devices (including virtual)
	std::vector<IntersectionDevice *> idevices;
	// All OpenCL devices
	std::vector<OpenCLIntersectionDevice *> oclDevices;
	// Virtual intersection devices
	std::vector<VirtualM2MHardwareIntersectionDevice *> m2mDevices;
	std::vector<VirtualM2OHardwareIntersectionDevice *> m2oDevices;

	// All pixel devices
	std::vector<PixelDevice *> pdevices;

	bool started;
};

}

#endif	/* _LUXRAYS_CONTEXT_H */

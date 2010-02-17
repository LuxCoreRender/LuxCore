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

#include <cstdlib>

#include "luxrays/luxrays.h"
#include "luxrays/core/context.h"
#include "luxrays/core/device.h"

void DebugHandler(const char *msg) {
	std::cerr << msg << std::endl;
}

int main(int argc, char** argv) {
	std::cerr << "Usage (easy mode): " << argv[0] << std::endl;

	//--------------------------------------------------------------------------
	// Create the context
	//--------------------------------------------------------------------------

	luxrays::Context *ctx = new luxrays::Context(DebugHandler);

	// Looks for the first GPU device
	std::vector<luxrays::DeviceDescription *> deviceDescs = std::vector<luxrays::DeviceDescription *>(ctx->GetAvailableDeviceDescriptions());
	luxrays::DeviceDescription::FilterOne(deviceDescs);

	if (deviceDescs.size() < 1) {
		std::cerr << "Unable to find a GPU or CPU intersection device" << std::endl;
		return (EXIT_FAILURE);
	}

	std::cerr << "Selected intersection device: " << deviceDescs[0]->GetName();
	ctx->AddIntersectionDevices(deviceDescs);

	return (EXIT_SUCCESS);
}

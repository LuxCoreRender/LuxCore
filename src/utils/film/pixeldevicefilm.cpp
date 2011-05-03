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

#include "luxrays/utils/film/pixeldevicefilm.h"

using namespace luxrays;
using namespace luxrays::utils;

PixelDeviceFilm::PixelDeviceFilm(Context *context, const unsigned int w,
			const unsigned int h, DeviceDescription *deviceDesc) : Film(w, h) {
	ctx = context;

	std::vector<DeviceDescription *> descs;
	descs.push_back(deviceDesc);
	pixelDevice = ctx->AddPixelDevices(descs)[0];
	pixelDevice->Init(w, h);
}

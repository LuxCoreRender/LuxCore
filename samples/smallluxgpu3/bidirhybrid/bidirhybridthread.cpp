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

#include "smalllux.h"
#include "renderconfig.h"
#include "bidirhybrid/bidirhybrid.h"

#include "luxrays/core/intersectiondevice.h"
#include "luxrays/opencl/utils.h"

#if defined(__APPLE__)
//OSX version detection
#include <sys/utsname.h>
#endif

//------------------------------------------------------------------------------
// BiDirHybridRenderThread
//------------------------------------------------------------------------------

BiDirHybridRenderThread::BiDirHybridRenderThread(BiDirHybridRenderEngine *engine,
		const u_int index, IntersectionDevice *device, const u_int seedVal) :
		HybridRenderThread(engine, index, device, seedVal) {
}

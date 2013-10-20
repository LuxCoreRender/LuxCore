/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SMALLLUXGPU_H
#define	_SMALLLUXGPU_H

#include <cmath>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstddef>

#if defined(__linux__) || defined(__APPLE__) || defined(__CYGWIN__)
#include <stddef.h>
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
	Unsupported Platform !!!
#endif

#include "luxrays/luxrays.h"
#include "luxrays/core/utils.h"
#include "slg/sdl/scene.h"
#include "slg/film/film.h"
#include "slg/rendersession.h"
#include "luxrays/utils/atomic.h"

#include "slg/slg.h"

extern slg::RenderConfig *config;
extern slg::RenderSession *session;

// Options
extern bool optMouseGrabMode;
extern bool optUseLuxVRName;
extern bool optOSDPrintHelp;
extern bool optRealTimeMode;
extern bool optUseGameMode;
extern float optMoveScale;
extern float optMoveStep;
extern float optRotateStep;

extern void UpdateMoveStep();

#endif	/* _SMALLLUXGPU_H */

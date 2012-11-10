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

#ifndef _LUXRAYS_SDL_H
#define	_LUXRAYS_SDL_H

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/spectrum.h"

namespace luxrays { namespace sdl {

extern void (*LuxRaysSDLDebugHandler)(const char *msg);

#define SDL_LOG(a) { if (luxrays::sdl::LuxRaysSDLDebugHandler) { std::stringstream _LR_LOG_LOCAL_SS; _LR_LOG_LOCAL_SS << a; luxrays::sdl::LuxRaysSDLDebugHandler(_LR_LOG_LOCAL_SS.str().c_str()); } }

} }

#endif	/* _LUXRAYS_SDL_H */

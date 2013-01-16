#line 2 "epsilon_types.cl"

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

// NOTE: DEFAULT_EPSILON_MIN is very small. A plane passing exactly for the
// origin will suffer of self shadow problems because the Ray class will use
// MachineEpsilon(ray.o) as epsilon for the ray.mint. However it is pretty much
// the only case where there is a problem so better to not change anything.
// As workaround, moving the plane away from the origin is enough.
#define DEFAULT_EPSILON_MIN 1e-9f
#define DEFAULT_EPSILON_MAX 1e-1f
#define DEFAULT_EPSILON_STATIC 1e-5f

// An epsilon that can be used as threshold for cos(theta). For instance:
// if (Dot(N, LightDir) < DEFAULT_COS_EPSILON_STATIC) return Spectrum();
#define DEFAULT_COS_EPSILON_STATIC 1e-4f

// This is about 1e-5f for values near 1.f
#define DEFAULT_EPSILON_DISTANCE_FROM_VALUE 0x80u

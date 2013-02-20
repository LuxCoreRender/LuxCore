#line 2 "ray_funcs.cl"

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

void Ray_Init4(__global Ray *ray, const float3 orig, const float3 dir,
		const float mint, const float maxt) {
	VSTORE3F(orig, &ray->o.x);
	VSTORE3F(dir, &ray->d.x);
	ray->mint = mint;
	ray->maxt = maxt;
}

void Ray_Init3(__global Ray *ray, const float3 orig, const float3 dir, const float maxt) {
	VSTORE3F(orig, &ray->o.x);
	VSTORE3F(dir, &ray->d.x);
	ray->mint = MachineEpsilon_E_Float3(orig);
	ray->maxt = maxt;
}

void Ray_Init2(__global Ray *ray, const float3 orig, const float3 dir) {
	VSTORE3F(orig, &ray->o.x);
	VSTORE3F(dir, &ray->d.x);
	ray->mint = MachineEpsilon_E_Float3(orig);
	ray->maxt = INFINITY;
}

#line 2 "mapping_funcs.cl"

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

float2 UVMapping2D_Map(__global TextureMapping2D *mapping, __global HitPoint *hitPoint) {
	const float2 scale = VLOAD2F(&mapping->uvMapping2D.uScale);
	const float2 delta = VLOAD2F(&mapping->uvMapping2D.uDelta);
	const float2 uv = VLOAD2F(&hitPoint->uv.u);
	
	return uv * scale + delta;
}

float3 UVMapping3D_Map(__global TextureMapping3D *mapping, __global HitPoint *hitPoint) {
	const float2 scale = VLOAD2F(&mapping->uvMapping3D.uScale);
	const float2 delta = VLOAD2F(&mapping->uvMapping3D.uDelta);
	const float2 uv = VLOAD2F(&hitPoint->uv.u);

	const float2 muv = uv * scale + delta;
	const float3 p = (float3)(uv.xy, 0.f);
	return Transform_ApplyPoint(&mapping->worldToLocal, p);
}

float3 GlobalMapping3D_Map(__global TextureMapping3D *mapping, __global HitPoint *hitPoint) {
	const float3 p = VLOAD3F(&hitPoint->p.x);
	return Transform_ApplyPoint(&mapping->worldToLocal, p);
}

float2 TextureMapping2D_Map(__global TextureMapping2D *mapping, __global HitPoint *hitPoint) {
	switch (mapping->type) {
		case UVMAPPING2D:
			return UVMapping2D_Map(mapping, hitPoint);
		default:
			return 0.f;
	}
}

float3 TextureMapping3D_Map(__global TextureMapping3D *mapping, __global HitPoint *hitPoint) {
	switch (mapping->type) {
		case UVMAPPING3D:
			return UVMapping3D_Map(mapping, hitPoint);
		case GLOBALMAPPING3D:
			return GlobalMapping3D_Map(mapping, hitPoint);
		default:
			return 0.f;
	}
}

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

float2 UVMapping_Map2D(__global TextureMapping *mapping, const float2 uv) {
	const float2 scale = VLOAD2F(&mapping->uvMapping.uScale);
	const float2 delta = VLOAD2F(&mapping->uvMapping.uDelta);
	
	return uv * scale + delta;
}

float3 UVMapping_Map3D(__global TextureMapping *mapping, const float3 p) {
	const float2 scale = VLOAD2F(&mapping->uvMapping.uScale);
	const float2 delta = VLOAD2F(&mapping->uvMapping.uDelta);
	const float2 mp = (float2)(p.x, p.y) * scale + delta;

	return (float3)(mp.xy, 0.f);
}

float2 GlobalMapping3D_Map2D(__global TextureMapping *mapping, const float2 uv) {
	const float3 p = Transform_ApplyPoint(&mapping->globalMapping3D.worldToLocal, (float3)(uv.xy, 0.f));
	return p.xy;
}

float3 GlobalMapping3D_Map3D(__global TextureMapping *mapping, const float3 p) {
	return Transform_ApplyPoint(&mapping->globalMapping3D.worldToLocal, p);
}

float2 Mapping_Map2D(__global TextureMapping *mapping, const float2 uv) {
	switch (mapping->type) {
		case UVMAPPING:
			return UVMapping_Map2D(mapping, uv);
		case GLOBALMAPPING3D:
			return GlobalMapping3D_Map2D(mapping, uv);
		default:
			return 0.f;
	}
}

float3 Mapping_Map3D(__global TextureMapping *mapping, const float3 p) {
	switch (mapping->type) {
		case UVMAPPING:
			return UVMapping_Map3D(mapping, p);
		case GLOBALMAPPING3D:
			return GlobalMapping3D_Map3D(mapping, p);
		default:
			return 0.f;
	}
}

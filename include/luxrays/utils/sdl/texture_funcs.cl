#line 2 "material_funcs.cl"

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

//------------------------------------------------------------------------------
// ConstFloat texture
//------------------------------------------------------------------------------

float3 ConstFloatTexture_GetColorValue(__global Texture *texture, const float2 uv) {
	return texture->constFloat.value;
}

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

float3 ConstFloat3Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	return vload3(0, &texture->constFloat3.color.r);
}

//------------------------------------------------------------------------------
// ConstFloat4 texture
//------------------------------------------------------------------------------

float3 ConstFloat4Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	return vload3(0, &texture->constFloat4.color.r);
}

//------------------------------------------------------------------------------

float3 Texture_GetColorValue(__global Texture *texture, const float2 uv) {
	switch (texture->type) {
		case CONST_FLOAT:
			return ConstFloatTexture_GetColorValue(texture, uv);
		case CONST_FLOAT3:
			return ConstFloat3Texture_GetColorValue(texture, uv);
		case CONST_FLOAT4:
			return ConstFloat4Texture_GetColorValue(texture, uv);
		default:
			return 0.f;
	}
}

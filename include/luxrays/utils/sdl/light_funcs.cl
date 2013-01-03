#line 2 "light_funcs.cl"

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
// InfiniteLight
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_INFINITELIGHT)

float3 InfiniteLight_GetRadiance(
	__global InfiniteLight *infiniteLight,
#if defined(PARAM_HAS_IMAGEMAPS)
	__global ImageMap *imageMapDescs,
#if defined(PARAM_IMAGEMAPS_PAGE_0)
	__global float *imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
	__global float *imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
	__global float *imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
	__global float *imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
	__global float *imageMapBuff4,
#endif
#endif
	const float3 dir) {
	const float2 uv = (float2)(
		1.f - SphericalPhi(-dir) * (1.f / (2.f * M_PI_F))+ infiniteLight->shiftU,
		SphericalTheta(-dir) * M_1_PI_F + infiniteLight->shiftV);

	return vload3(0, &infiniteLight->gain.r) * ImageMapInstance_GetColor(
			&infiniteLight->imageMapInstance,
#if defined(PARAM_HAS_IMAGEMAPS)
			imageMapDescs,
#if defined(PARAM_IMAGEMAPS_PAGE_0)
			imageMapBuff0,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_1)
			imageMapBuff1,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_2)
			imageMapBuff2,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_3)
			imageMapBuff3,
#endif
#if defined(PARAM_IMAGEMAPS_PAGE_4)
			imageMapBuff4,
#endif
#endif
			uv);
}

#endif

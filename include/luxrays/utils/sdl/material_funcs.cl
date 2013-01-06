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
// Matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTE)

float3 MatteMaterial_Evaluate(__global Material *material, __global Texture *texs,
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
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Texture_GetColorValue(&texs[material->matte.kdTexIndex],
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
	return M_1_PI_F * kd;
}

float3 MatteMaterial_Sample(__global Material *material, __global Texture *texs,
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
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Texture_GetColorValue(&texs[material->matte.kdTexIndex],
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
	return M_1_PI_F * kd;
}

#endif

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIRROR)

float3 MirrorMaterial_Sample(__global Material *material, __global Texture *texs,
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
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	const float3 kr = Texture_GetColorValue(&texs[material->mirror.krTexIndex],
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
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return kr / (*cosSampledDir);
}

#endif

//------------------------------------------------------------------------------
// Generic material functions
//------------------------------------------------------------------------------

BSDFEvent Material_GetEventTypes(__global Material *mat) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return DIFFUSE | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return SPECULAR | REFLECT;
#endif
		default:
			return NONE;
	}
}

bool Material_IsDelta(__global Material *mat) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return false;
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
		default:
			return true;
	}
}

float3 Material_Evaluate(__global Material *mat, __global Texture *texs,
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
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Evaluate(mat, texs,
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
					uv, lightDir, eyeDir,
					event, directPdfW);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
		default:
			return BLACK;
	}
}

float3 Material_Sample(__global Material *mat, __global Texture *texs,
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
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	switch (mat->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Sample(mat, texs,
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
					uv, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Sample(mat, texs,
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
					uv, fixedDir, sampledDir,
					u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event);
#endif
		default:
			return BLACK;
	}
}

float3 Material_GetEmittedRadiance(__global Material *mat, __global Texture *texs,
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
	const float2 triUV) {
	const uint emitTexIndex = mat->emitTexIndex;
	if (emitTexIndex == NULL_INDEX)
		return BLACK;

	return Texture_GetColorValue(&texs[emitTexIndex],
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
			triUV);	
}

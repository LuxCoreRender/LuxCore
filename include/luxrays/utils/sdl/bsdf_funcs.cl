#line 2 "bsdf_funcs.cl"

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

void BSDF_Init(
		__global BSDF *bsdf,
		__global uint *meshMats,
		__global uint *meshIDs,
		__global Point *vertices,
		__global Vector *vertNormals,
		__global UV *vertUVs,
		__global Triangle *triangles,
		__global Ray *ray,
		__global RayHit *rayHit
		) {
	const float3 rayOrig = VLOAD3F(&ray->o.x);
	const float3 rayDir = VLOAD3F(&ray->d.x);
	VSTORE3F(rayOrig + rayHit->t * rayDir, &bsdf->hitPoint.x);
	VSTORE3F(-rayDir, &bsdf->fixedDir.x);

	const uint currentTriangleIndex = rayHit->index;
	const uint meshIndex = meshIDs[currentTriangleIndex];

	// Get the material
	const uint matIndex = meshMats[meshIndex];
	bsdf->materialIndex = matIndex;

	// Interpolate face normal and UV coordinates
//	const float b1 = rayHit->b1;
//	const float b2 = rayHit->b2;

//	const float3 geometryN = Mesh_GetGeometryNormal(vertices, triangles, currentTriangleIndex);
//	VSTORE3F(geometryN, &bsdf->geometryN.x);
//	float3 shadeN = Mesh_InterpolateNormal(vertNormals, triangles, currentTriangleIndex, b1, b2);
//	const float2 hitPointUV = Mesh_InterpolateUV(vertUVs, triangles, currentTriangleIndex, b1, b2);
//
//	VSTORE2F(hitPointUV, &bsdf->hitPointUV.u);
//
//	Frame_SetFromZ(&bsdf->frame, shadeN);
//
//	VSTORE3F(shadeN, &bsdf->shadeN.x);
}

float3 BSDF_Evaluate(__global BSDF *bsdf,
		const float3 generatedDir, BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	//const Vector &eyeDir = fromLight ? generatedDir : fixedDir;
	//const Vector &lightDir = fromLight ? fixedDir : generatedDir;
	const float3 eyeDir = VLOAD3F(&bsdf->fixedDir.x);
	const float3 lightDir = generatedDir;
	const float3 geometryN = VLOAD3F(&bsdf->geometryN.x);

	const float dotLightDirNG = dot(lightDir, geometryN);
	const float absDotLightDirNG = fabs(dotLightDirNG);
	const float dotEyeDirNG = dot(eyeDir, geometryN);
	const float absDotEyeDirNG = fabs(dotEyeDirNG);

	if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
			(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	__global Material *mat = &mats[bsdf->materialIndex];
	const float sideTest = dotEyeDirNG * dotLightDirNG;
	const BSDFEvent matEvent = Material_GetEventTypes(mat
			MATERIALS_PARAM);
	if (((sideTest > 0.f) && !(matEvent & REFLECT)) ||
			((sideTest < 0.f) && !(matEvent & TRANSMIT)))
		return BLACK;

	__global Frame *frame = &bsdf->frame;
	const float3 localLightDir = Frame_ToLocal(frame, lightDir);
	const float3 localEyeDir = Frame_ToLocal(frame, eyeDir);
	const float3 result = Material_Evaluate(mat, VLOAD2F(&bsdf->hitPointUV.u),
			localLightDir, localEyeDir,	event, directPdfW
			MATERIALS_PARAM);

	// Adjoint BSDF
//	if (fromLight) {
//		const float absDotLightDirNS = AbsDot(lightDir, shadeN);
//		const float absDotEyeDirNS = AbsDot(eyeDir, shadeN);
//		return result * ((absDotLightDirNS * absDotEyeDirNG) / (absDotEyeDirNS * absDotLightDirNG));
//	} else
		return result;
}

float3 BSDF_Sample(__global BSDF *bsdf, const float u0, const float u1,
		float3 *sampledDir, float *pdfW, float *cosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	const float3 fixedDir = VLOAD3F(&bsdf->fixedDir.x);

	//--------------------------------------------------------------------------
	// Original code:

	//const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, fixedDir);
	//--------------------------------------------------------------------------

	//--------------------------------------------------------------------------
	// Looking for a work around: 

//	const float Zx = bsdf->shadeN.x;
//	const float Zy = bsdf->shadeN.y;
//	const float Zz = bsdf->shadeN.z;
//
//	float Xx, Xy, Xz;
//	//if (fabs(Zx) > fabs(Zy)) {
//	if (Zx * Zx > Zy * Zy) {
//		//const float invLen = 1.f / sqrt(Z.x * Z.x + Z.z * Z.z);
//		const float len = sqrt(Zx * Zx + Zz * Zz);
//		Xx = -Zz / len;
//		Xy = 0.f;
//		Xz = Zx / len;
//	} else {
//		//const float invLen = 1.f / sqrt(Z.y * Z.y + Z.z * Z.z);
//		const float len = sqrt(Zy * Zy + Zz * Zz);
//		Xx = 0.f;
//		Xy = Zz / len;
//		Xz = -Zy / len;
//	}
//
//	float Yx, Yy, Yz;
//	Yx = (Zy * Xz) - (Zz * Xy);
//	Yy = (Zz * Xx) - (Zx * Xz);
//	Yz = (Zx * Xy) - (Zy * Xx);
//
//	float dotx = fixedDir.x * Xx + fixedDir.y * Xy + fixedDir.z * Xz;
//	float doty = fixedDir.x * Yx + fixedDir.y * Yy + fixedDir.z * Yz;
//	float dotz = fixedDir.x * Zx + fixedDir.y * Zy + fixedDir.z * Zz;
//	const float3 localFixedDir = (float3)(dotx, doty, dotz);

	const float3 localFixedDir = fixedDir;

	//--------------------------------------------------------------------------

	return (float3)(fabs(localFixedDir.x), fabs(localFixedDir.y), fabs(localFixedDir.z));

//	float3 localSampledDir;
//
//	const float3 result = Material_Sample(&mats[bsdf->materialIndex], VLOAD2F(&bsdf->hitPointUV.u),
//			localFixedDir, &localSampledDir, u0, u1,
//#if defined(PARAM_HAS_PASSTHROUGH)
//			bsdf->passThroughEvent,
//#endif
//			pdfW, cosSampledDir, event
//			MATERIALS_PARAM);
//	if (Spectrum_IsBlack(result))
//		return 0.f;
//
//	//*sampledDir = Frame_ToWorld(&bsdf->frame, localSampledDir);
//
//	(*sampledDir).x = Xx * localSampledDir.x + Yx * localSampledDir.y + Zx * localSampledDir.z;
//	(*sampledDir).y = Xy * localSampledDir.x + Yy * localSampledDir.y + Zy * localSampledDir.z;
//	(*sampledDir).z = Xz * localSampledDir.x + Yz * localSampledDir.y + Zz * localSampledDir.z;
//
//	// Adjoint BSDF
////	if (fromLight) {
////		const float absDotFixedDirNS = fabsf(localFixedDir.z);
////		const float absDotSampledDirNS = fabsf(localSampledDir.z);
////		const float absDotFixedDirNG = AbsDot(fixedDir, geometryN);
////		const float absDotSampledDirNG = AbsDot(*sampledDir, geometryN);
////		return result * ((absDotFixedDirNS * absDotSampledDirNG) / (absDotSampledDirNS * absDotFixedDirNG));
////	} else
//		return result;
}

bool BSDF_IsDelta(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	return Material_IsDelta(&mats[bsdf->materialIndex]
			MATERIALS_PARAM);
}

#if (PARAM_DL_LIGHT_COUNT > 0)
float3 BSDF_GetEmittedRadiance(__global BSDF *bsdf,
		__global TriangleLight *triLightDefs, float *directPdfA
		MATERIALS_PARAM_DECL) {
	const uint triangleLightSourceIndex = bsdf->triangleLightSourceIndex;
	if (triangleLightSourceIndex == NULL_INDEX)
		return BLACK;
	else
		return TriangleLight_GetRadiance(&triLightDefs[triangleLightSourceIndex],
				VLOAD3F(&bsdf->fixedDir.x), VLOAD3F(&bsdf->geometryN.x), VLOAD2F(&bsdf->hitPointUV.u), directPdfA
				MATERIALS_PARAM);
}
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
float3 BSDF_GetPassThroughTransparency(__global BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	const float3 localFixedDir = Frame_ToLocal(&bsdf->frame, VLOAD3F(&bsdf->fixedDir.x));

	return Material_GetPassThroughTransparency(&mats[bsdf->materialIndex],
			VLOAD2F(&bsdf->hitPointUV.u), localFixedDir, bsdf->passThroughEvent
			MATERIALS_PARAM);
}
#endif

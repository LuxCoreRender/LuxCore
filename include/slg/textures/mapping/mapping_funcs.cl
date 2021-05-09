#line 2 "mapping_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

//------------------------------------------------------------------------------
// UVMapping2D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float2 UVMapping2D_Map(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint TEXTURES_PARAM_DECL) {
	const float2 uv = HitPoint_GetUV(hitPoint, mapping->dataIndex EXTMESH_PARAM);

	// Scale
	const float uScaled = uv.x * mapping->uvMapping2D.uScale;
	const float vScaled = uv.y * mapping->uvMapping2D.vScale;

	// Rotate
	const float sinTheta = mapping->uvMapping2D.sinTheta;
	const float cosTheta = mapping->uvMapping2D.cosTheta;
	const float uRotated = uScaled * cosTheta - vScaled * sinTheta;
	const float vRotated = vScaled * cosTheta + uScaled * sinTheta;

	// Translate
	const float uTranslated = uRotated + mapping->uvMapping2D.uDelta;
	const float vTranslated = vRotated + mapping->uvMapping2D.vDelta;

	return MAKE_FLOAT2(uTranslated, vTranslated);
}

OPENCL_FORCE_INLINE float2 UVMapping2D_MapDuv(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint, float2 *ds, float2 *dt TEXTURES_PARAM_DECL) {
	const float signUScale = mapping->uvMapping2D.uScale > 0 ? 1.f : -1.f;
	const float signVScale = mapping->uvMapping2D.vScale > 0 ? 1.f : -1.f;
	const float sinTheta = mapping->uvMapping2D.sinTheta;
	const float cosTheta = mapping->uvMapping2D.cosTheta;
	
	*ds = MAKE_FLOAT2(signUScale * cosTheta, signUScale * sinTheta);
	*dt = MAKE_FLOAT2(-signVScale * sinTheta, signVScale * cosTheta);
	
	return UVMapping2D_Map(mapping, hitPoint TEXTURES_PARAM);
}

//------------------------------------------------------------------------------
// UVRandomMapping2D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float2 UVRandomMapping2D_MapImpl(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint, float2 *ds, float2 *dt TEXTURES_PARAM_DECL) {
	// Select random parameters
	uint seed;
	switch (mapping->uvRandomMapping2D.seedType) {
		default:
		case OBJECT_ID:
			seed = hitPoint->objectID;
			break;
		case TRIANGLE_AOV:
			seed = (uint)HitPoint_GetTriAOV(hitPoint, mapping->uvRandomMapping2D.triAOVIndex EXTMESH_PARAM);
			break;
		case OBJECT_ID_OFFSET:
			seed = hitPoint->objectID + mapping->uvRandomMapping2D.objectIDOffset;
			break;
	}

	Seed rndSeed;
	Rnd_Init(seed, &rndSeed);

	const float uvRotation = LerpWithStep(Rnd_FloatValue(&rndSeed),
			mapping->uvRandomMapping2D.uvRotationMin, mapping->uvRandomMapping2D.uvRotationMax,
			mapping->uvRandomMapping2D.uvRotationStep);
	const float uScale = Lerp(Rnd_FloatValue(&rndSeed),
			mapping->uvRandomMapping2D.uScaleMin, mapping->uvRandomMapping2D.uScaleMax);
	const float vScale = mapping->uvRandomMapping2D.uniformScale ?
		uScale :
		Lerp(Rnd_FloatValue(&rndSeed),
			mapping->uvRandomMapping2D.vScaleMin, mapping->uvRandomMapping2D.vScaleMax);
	const float uDelta = Lerp(Rnd_FloatValue(&rndSeed),
			mapping->uvRandomMapping2D.uDeltaMin, mapping->uvRandomMapping2D.uDeltaMax);
	const float vDelta = Lerp(Rnd_FloatValue(&rndSeed),
			mapping->uvRandomMapping2D.vDeltaMin, mapping->uvRandomMapping2D.vDeltaMax);

	// Get the hit point UV
	const float2 uv = HitPoint_GetUV(hitPoint, mapping->dataIndex EXTMESH_PARAM);
	
	// Scale
	const float uScaled = uv.x * uScale;
	const float vScaled = uv.y * vScale;

	// Rotate
	const float rad = Radians(-uvRotation);
	const float sinTheta = sin(rad);
	const float cosTheta = cos(rad);
	const float uRotated = uScaled * cosTheta - vScaled * sinTheta;
	const float vRotated = vScaled * cosTheta + uScaled * sinTheta;

	// Translate
	const float uTranslated = uRotated + uDelta;
	const float vTranslated = vRotated + vDelta;
	
	if (ds && dt) {
		const float signUScale = uScale > 0 ? 1.f : -1.f;;
		const float signVScale = vScale > 0 ? 1.f : -1.f;

		*ds = MAKE_FLOAT2(signUScale * cosTheta, signUScale * sinTheta);
		*dt = MAKE_FLOAT2(-signVScale * sinTheta, signVScale * cosTheta);
	}
	
	return MAKE_FLOAT2(uTranslated, vTranslated);
}

OPENCL_FORCE_INLINE float2 UVRandomMapping2D_Map(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint TEXTURES_PARAM_DECL) {
	return  UVRandomMapping2D_MapImpl(mapping, hitPoint, NULL, NULL TEXTURES_PARAM);
}

OPENCL_FORCE_INLINE float2 UVRandomMapping2D_MapDuv(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint, float2 *ds, float2 *dt TEXTURES_PARAM_DECL) {
	return  UVRandomMapping2D_MapImpl(mapping, hitPoint, ds, dt TEXTURES_PARAM);
}

//------------------------------------------------------------------------------
// TextureMapping2D
//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float2 TextureMapping2D_Map(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint TEXTURES_PARAM_DECL) {
	switch (mapping->type) {
		case UVMAPPING2D:
			return UVMapping2D_Map(mapping, hitPoint TEXTURES_PARAM);
		case UVRANDOMMAPPING2D:
			return UVRandomMapping2D_Map(mapping, hitPoint TEXTURES_PARAM);
		default:
			return MAKE_FLOAT2(0.f, 0.f);
	}
}

OPENCL_FORCE_NOT_INLINE float2 TextureMapping2D_MapDuv(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint, float2 *ds, float2 *dt TEXTURES_PARAM_DECL) {
	switch (mapping->type) {
		case UVMAPPING2D:
			return UVMapping2D_MapDuv(mapping, hitPoint, ds, dt TEXTURES_PARAM);
		case UVRANDOMMAPPING2D:
			return UVRandomMapping2D_MapDuv(mapping, hitPoint, ds, dt TEXTURES_PARAM);
		default:
			return MAKE_FLOAT2(0.f, 0.f);
	}
}

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 UVMapping3D_Map(__global const TextureMapping3D *mapping,
		__global const HitPoint *hitPoint, float3 *shadeN TEXTURES_PARAM_DECL) {
	if (shadeN)
		*shadeN = normalize(Transform_ApplyNormal(&mapping->worldToLocal, VLOAD3F(&hitPoint->shadeN.x)));

	const float2 uv = HitPoint_GetUV(hitPoint, mapping->uvMapping3D.dataIndex EXTMESH_PARAM);
	return Transform_ApplyPoint(&mapping->worldToLocal, MAKE_FLOAT3(uv.x, uv.y, 0.f));
}

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 GlobalMapping3D_Map(__global const TextureMapping3D *mapping,
		__global const HitPoint *hitPoint, float3 *shadeN) {
	if (shadeN)
		*shadeN = normalize(Transform_ApplyNormal(&mapping->worldToLocal, VLOAD3F(&hitPoint->shadeN.x)));

	const float3 p = VLOAD3F(&hitPoint->p.x);
	return Transform_ApplyPoint(&mapping->worldToLocal, p);
}

//------------------------------------------------------------------------------
// LocalMapping3D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 LocalMapping3D_Map(__global const TextureMapping3D *mapping,
		__global const HitPoint *hitPoint, float3 *shadeN) {
	if (shadeN) {
		const Matrix4x4 mInv = Matrix4x4_Mul(&hitPoint->localToWorld.m, &mapping->worldToLocal.mInv);
		
		const float3 sn = VLOAD3F(&hitPoint->shadeN.x);
		*shadeN = normalize(Matrix4x4_ApplyNormal_Private(&mInv, sn));
	}

	const Matrix4x4 m = Matrix4x4_Mul(&mapping->worldToLocal.m, &hitPoint->localToWorld.mInv);

	const float3 p = VLOAD3F(&hitPoint->p.x);
	return Matrix4x4_ApplyPoint_Private(&m, p);
}

//------------------------------------------------------------------------------
// LocalRandomMapping3D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 LocalRandomMapping3D_Map(__global const TextureMapping3D *mapping,
		__global const HitPoint *hitPoint, float3 *shadeN TEXTURES_PARAM_DECL) {
	if (shadeN) {
		const Matrix4x4 mInv = Matrix4x4_Mul(&hitPoint->localToWorld.m, &mapping->worldToLocal.mInv);
		
		const float3 sn = VLOAD3F(&hitPoint->shadeN.x);
		*shadeN = normalize(Matrix4x4_ApplyNormal_Private(&mInv, sn));
	}

	Matrix4x4 m = Matrix4x4_Mul(&mapping->worldToLocal.m, &hitPoint->localToWorld.mInv);

	// Select random parameters
	uint seed;
	switch (mapping->localRandomMapping.seedType) {
		default:
		case OBJECT_ID:
			seed = hitPoint->objectID;
			break;
		case TRIANGLE_AOV:
			seed = (uint)HitPoint_GetTriAOV(hitPoint, mapping->localRandomMapping.triAOVIndex EXTMESH_PARAM);
			break;
		case OBJECT_ID_OFFSET:
			seed = hitPoint->objectID + mapping->localRandomMapping.objectIDOffset;
			break;
	}

	Seed rndSeed;
	Rnd_Init(seed, &rndSeed);
	
	const float xRotation = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.xRotationMin, mapping->localRandomMapping.xRotationMax);
	const float yRotation = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.yRotationMin, mapping->localRandomMapping.yRotationMax);
	const float zRotation = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.zRotationMin, mapping->localRandomMapping.zRotationMax);
	
	const float xScale = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.xScaleMin, mapping->localRandomMapping.xScaleMax);
	const bool uniformScale = mapping->localRandomMapping.uniformScale;
	const float yScale = uniformScale ? xScale : Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.yScaleMin, mapping->localRandomMapping.yScaleMax);
	const float zScale = uniformScale ? xScale : Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.zScaleMin, mapping->localRandomMapping.zScaleMax);
	
	const float xTranslate = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.xTranslateMin, mapping->localRandomMapping.xTranslateMax);
	const float yTranslate = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.yTranslateMin, mapping->localRandomMapping.yTranslateMax);
	const float zTranslate = Lerp(Rnd_FloatValue(&rndSeed), mapping->localRandomMapping.zTranslateMin, mapping->localRandomMapping.zTranslateMax);

	Matrix4x4 mRandomTrans = Matrix4x4_Scale(xScale, yScale, zScale);

	const Matrix4x4 mRotateX = Matrix4x4_RotateX(xRotation);
	mRandomTrans = Matrix4x4_Mul_Private(&mRandomTrans, &mRotateX);
	const Matrix4x4 mRotateY = Matrix4x4_RotateY(yRotation);
	mRandomTrans = Matrix4x4_Mul_Private(&mRandomTrans, &mRotateY);
	const Matrix4x4 mRotateZ = Matrix4x4_RotateZ(zRotation);
	mRandomTrans = Matrix4x4_Mul_Private(&mRandomTrans, &mRotateZ);

	const Matrix4x4 mTranslate = Matrix4x4_Translate(xTranslate, yTranslate, zTranslate);
	mRandomTrans = Matrix4x4_Mul_Private(&mRandomTrans, &mTranslate);
	
	m = Matrix4x4_Mul_Private(&mRandomTrans, &m);

	const float3 p = VLOAD3F(&hitPoint->p.x);
	return Matrix4x4_ApplyPoint_Private(&m, p);
}

//------------------------------------------------------------------------------

OPENCL_FORCE_NOT_INLINE float3 TextureMapping3D_Map(__global const TextureMapping3D *mapping,
		__global const HitPoint *hitPoint, float3 *shadeN TEXTURES_PARAM_DECL) {
	switch (mapping->type) {
		case UVMAPPING3D:
			return UVMapping3D_Map(mapping, hitPoint, shadeN TEXTURES_PARAM);
		case GLOBALMAPPING3D:
			return GlobalMapping3D_Map(mapping, hitPoint, shadeN);
		case LOCALMAPPING3D:
			return LocalMapping3D_Map(mapping, hitPoint, shadeN);
		case LOCALRANDOMMAPPING3D:
			return LocalRandomMapping3D_Map(mapping, hitPoint, shadeN TEXTURES_PARAM);
		default:
			return BLACK;
	}
}

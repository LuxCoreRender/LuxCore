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
// 2D mapping
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

OPENCL_FORCE_NOT_INLINE float2 TextureMapping2D_Map(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint TEXTURES_PARAM_DECL) {
	switch (mapping->type) {
		case UVMAPPING2D:
			return UVMapping2D_Map(mapping, hitPoint TEXTURES_PARAM);
		default:
			return MAKE_FLOAT2(0.f, 0.f);
	}
}

OPENCL_FORCE_NOT_INLINE float2 TextureMapping2D_MapDuv(__global const TextureMapping2D *mapping,
		__global const HitPoint *hitPoint, float2 *ds, float2 *dt TEXTURES_PARAM_DECL) {
	switch (mapping->type) {
		case UVMAPPING2D:
			return UVMapping2D_MapDuv(mapping, hitPoint, ds, dt TEXTURES_PARAM);
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

OPENCL_FORCE_NOT_INLINE float3 TextureMapping3D_Map(__global const TextureMapping3D *mapping,
		__global const HitPoint *hitPoint, float3 *shadeN TEXTURES_PARAM_DECL) {
	switch (mapping->type) {
		case UVMAPPING3D:
			return UVMapping3D_Map(mapping, hitPoint, shadeN TEXTURES_PARAM);
		case GLOBALMAPPING3D:
			return GlobalMapping3D_Map(mapping, hitPoint, shadeN);
		case LOCALMAPPING3D:
			return LocalMapping3D_Map(mapping, hitPoint, shadeN);
		default:
			return BLACK;
	}
}

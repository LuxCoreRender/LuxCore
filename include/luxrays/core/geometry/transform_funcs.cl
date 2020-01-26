#line 2 "transform_funcs.cl"

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

OPENCL_FORCE_INLINE void Transform_Init(__global Transform *trans) {
	Matrix4x4_IdentityGlobal(&trans->m);
	Matrix4x4_IdentityGlobal(&trans->mInv);
}

OPENCL_FORCE_INLINE float3 Transform_ApplyPoint(__global const Transform* restrict trans, const float3 point) {
	return Matrix4x4_ApplyPoint(&trans->m, point);
}

OPENCL_FORCE_INLINE float3 Transform_ApplyVector(__global const Transform* restrict trans, const float3 vector) {
	return Matrix4x4_ApplyVector(&trans->m, vector);
}

OPENCL_FORCE_INLINE float3 Transform_ApplyNormal(__global const Transform* restrict trans, const float3 normal) {
	return Matrix4x4_ApplyNormal(&trans->mInv, normal);
}

OPENCL_FORCE_INLINE float3 Transform_InvApplyPoint(__global const Transform* restrict trans, const float3 point) {
	return Matrix4x4_ApplyPoint(&trans->mInv, point);
}

OPENCL_FORCE_INLINE float3 Transform_InvApplyVector(__global const Transform* restrict trans, const float3 vector) {
	return Matrix4x4_ApplyVector(&trans->mInv, vector);
}

OPENCL_FORCE_INLINE float3 Transform_InvApplyNormal(__global const Transform* restrict trans, const float3 normal) {
	return Matrix4x4_ApplyNormal(&trans->m, normal);
}

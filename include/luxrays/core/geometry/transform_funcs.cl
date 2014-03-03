#line 2 "transform_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

float3 Transform_ApplyPoint(__global Transform *trans, const float3 point) {
	return Matrix4x4_ApplyPoint(&trans->m, point);
}

float3 Transform_ApplyVector(__global Transform *trans, const float3 vector) {
	return Matrix4x4_ApplyVector(&trans->m, vector);
}

float3 Transform_ApplyNormal(__global Transform *trans, const float3 normal) {
	return Matrix4x4_ApplyNormal(&trans->m, normal);
}

float3 Transform_InvApplyPoint(__global Transform *trans, const float3 point) {
	return Matrix4x4_ApplyPoint(&trans->mInv, point);
}

float3 Transform_InvApplyVector(__global Transform *trans, const float3 vector) {
	return Matrix4x4_ApplyVector(&trans->mInv, vector);
}

float3 Transform_InvApplyNormal(__global Transform *trans, const float3 normal) {
	return Matrix4x4_ApplyNormal(&trans->mInv, normal);
}

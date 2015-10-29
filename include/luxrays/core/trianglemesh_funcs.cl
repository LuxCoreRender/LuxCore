#line 2 "trianglemesh_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

float3 Mesh_GetGeometryNormal(__global const Point* restrict vertices,
		__global const Triangle* restrict triangles, const uint triIndex) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float3 p0 = VLOAD3F(&vertices[tri->v[0]].x);
	const float3 p1 = VLOAD3F(&vertices[tri->v[1]].x);
	const float3 p2 = VLOAD3F(&vertices[tri->v[2]].x);

	return Triangle_GetGeometryNormal(p0, p1, p2);
}

float3 Mesh_InterpolateNormal(__global const Vector* restrict normals,
		__global const Triangle* restrict triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float3 n0 = VLOAD3F(&normals[tri->v[0]].x);
	const float3 n1 = VLOAD3F(&normals[tri->v[1]].x);
	const float3 n2 = VLOAD3F(&normals[tri->v[2]].x);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateNormal(n0, n1, n2, b0, b1, b2);
}

float2 Mesh_InterpolateUV(__global const UV* restrict vertUVs,
		__global const Triangle *triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float2 uv0 = VLOAD2F(&vertUVs[tri->v[0]].u);
	const float2 uv1 = VLOAD2F(&vertUVs[tri->v[1]].u);
	const float2 uv2 = VLOAD2F(&vertUVs[tri->v[2]].u);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);
}

float3 Mesh_InterpolateColor(__global const Spectrum* restrict vertCols,
		__global const Triangle* restrict triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float3 rgb0 = VLOAD3F(vertCols[tri->v[0]].c);
	const float3 rgb1 = VLOAD3F(vertCols[tri->v[1]].c);
	const float3 rgb2 = VLOAD3F(vertCols[tri->v[2]].c);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateColor(rgb0, rgb1, rgb2, b0, b1, b2);
}

float Mesh_InterpolateAlpha(__global const float* restrict vertAlphas,
		__global const Triangle* restrict triangles,
		const uint triIndex, const float b1, const float b2) {
	__global const Triangle* restrict tri = &triangles[triIndex];
	const float a0 = vertAlphas[tri->v[0]];
	const float a1 = vertAlphas[tri->v[1]];
	const float a2 = vertAlphas[tri->v[2]];

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateAlpha(a0, a1, a2, b0, b1, b2);
}


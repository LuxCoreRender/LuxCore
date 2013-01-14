#line 2 "trianglemesh_funcs.cl"

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

float3 Mesh_GetGeometryNormal(__global Point *vertices,
		__global Triangle *triangles, const uint triIndex) {
	__global Triangle *tri = &triangles[triIndex];
	const float3 p0 = VLOAD3F(&vertices[tri->v[0]].x);
	const float3 p1 = VLOAD3F(&vertices[tri->v[1]].x);
	const float3 p2 = VLOAD3F(&vertices[tri->v[2]].x);

	return Triangle_GetGeometryNormal(p0, p1, p2);
}

float3 Mesh_InterpolateNormal(__global Vector *normals, __global Triangle *triangles,
		const uint triIndex, const float b1, const float b2) {
	__global Triangle *tri = &triangles[triIndex];
	const float3 n0 = VLOAD3F(&normals[tri->v[0]].x);
	const float3 n1 = VLOAD3F(&normals[tri->v[1]].x);
	const float3 n2 = VLOAD3F(&normals[tri->v[2]].x);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateNormal(n0, n1, n2, b0, b1, b2);
}

float2 Mesh_InterpolateUV(__global UV *vertUVs, __global Triangle *triangles,
		const uint triIndex, const float b1, const float b2) {
	__global Triangle *tri = &triangles[triIndex];
	const float2 uv0 = VLOAD2F(&vertUVs[tri->v[0]].u);
	const float2 uv1 = VLOAD2F(&vertUVs[tri->v[1]].u);
	const float2 uv2 = VLOAD2F(&vertUVs[tri->v[2]].u);

	const float b0 = 1.f - b1 - b2;
	return Triangle_InterpolateUV(uv0, uv1, uv2, b0, b1, b2);
}

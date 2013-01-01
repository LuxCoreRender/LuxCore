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

float3 MatteMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
				return 0.f;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return 0.f;

	*event = DIFFUSE | REFLECT;

	return M_1_PI_F * vload3(0, &material->param.matte.kd.r);
}

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

float3 MirrorMaterial_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return vload3(0, &material->param.mirror.kr.r) / (*cosSampledDir);
}

//------------------------------------------------------------------------------

float3 Material_Sample(__global Material *material,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,  const float passThroughEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	switch (material->type) {
		case MATTE:
			return MatteMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1, passThroughEvent, pdfW, cosSampledDir, event);
		case MIRROR:
			return MirrorMaterial_Sample(material, uv, fixedDir, sampledDir,
					u0, u1, passThroughEvent, pdfW, cosSampledDir, event);
		default:
			return 0.f;
	}
}

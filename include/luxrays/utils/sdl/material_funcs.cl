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
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Texture_GetColorValue(&texs[material->matte.kdTexIndex], uv
			IMAGEMAPS_PARAM);
	return M_1_PI_F * kd;
}

float3 MatteMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	if (fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Texture_GetColorValue(&texs[material->matte.kdTexIndex], uv
			IMAGEMAPS_PARAM);
	return M_1_PI_F * kd;
}

#endif

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIRROR)

float3 MirrorMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	*event = SPECULAR | REFLECT;

	*sampledDir = (float3)(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabs((*sampledDir).z);
	const float3 kr = Texture_GetColorValue(&texs[material->mirror.krTexIndex], uv
			IMAGEMAPS_PARAM);
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return kr / (*cosSampledDir);
}

#endif

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLASS)

float3 GlassMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;
	const float3 reflDir = rayDir - (2.f * dot(N, rayDir)) * N;

	const float nc = Texture_GetGreyValue(&texs[material->glass.ousideIorTexIndex], uv
			IMAGEMAPS_PARAM);
	const float nt = Texture_GetGreyValue(&texs[material->glass.iorTexIndex], uv
			IMAGEMAPS_PARAM);
	const float nnt = into ? (nc / nt) : (nt / nc);
	const float nnt2 = nnt * nnt;
	const float ddn = dot(rayDir, shadeN);
	const float cos2t = 1.f - nnt2 * (1.f - ddn * ddn);

	// Total internal reflection
	if (cos2t < 0.f) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		const float3 kr = Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	}

	const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(nnt * rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float a = nt - nc;
	const float b = nt + nc;
	const float R0 = a * a / (b * b);
	const float Re = R0 + (1.f - R0) * c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return BLACK;
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabs((*sampledDir).z);
			*pdfW = 1.f;

			const float3 kr = Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
					IMAGEMAPS_PARAM);
			// The cosSampledDir is used to compensate the other one used inside the integrator
			return kr / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		// The cosSampledDir is used to compensate the other one used inside the integrator
//		if (fromLight)
//			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
//		else
//			return Kt->GetColorValue(uv) / (*cosSampledDir);
		const float3 kt = Texture_GetColorValue(&texs[material->glass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt / (*cosSampledDir);
	} else if (u0 < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = P / Re;

		const float3 kr = Texture_GetColorValue(&texs[material->glass.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = (1.f - P) / Tr;

		// The cosSampledDir is used to compensate the other one used inside the integrator
//		if (fromLight)
//			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
//		else
//			return Kt->GetColorValue(uv) / (*cosSampledDir);
		const float3 kt = Texture_GetColorValue(&texs[material->glass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt / (*cosSampledDir);
	}
}

#endif

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

float3 GlossyReflection(const float3 fixedDir, const float exponent,
		const float u0, const float u1) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);

	const float phi = 2.f * M_PI * u0;
	const float cosTheta = pow(1.f - u1, exponent);
	const float sinTheta = sqrt(fmax(0.f, 1.f - cosTheta * cosTheta));
	const float x = cos(phi) * sinTheta;
	const float y = sin(phi) * sinTheta;
	const float z = cosTheta;

	const float3 dir = -fixedDir;
	const float dp = dot(shadeN, dir);
	const float3 w = dir - (2.f * dp) * shadeN;

	const float3 u = normalize(cross(
			(fabs(shadeN.x) > .1f) ? ((float3)(0.f, 1.f, 0.f)) : ((float3)(1.f, 0.f, 0.f)),
			w));
	const float3 v = cross(w, u);

	return x * u + y * v + z * w;
}

#if defined (PARAM_ENABLE_MAT_METAL)

float3 MetalMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	const float e = 1.f / (Texture_GetGreyValue(&texs[material->metal.expTexIndex], uv
				IMAGEMAPS_PARAM) + 1.f);
	*sampledDir = GlossyReflection(fixedDir, e, u0, u1);

	if ((*sampledDir).z * fixedDir.z > 0.f) {
		*event = SPECULAR | REFLECT;
		*pdfW = 1.f;
		*cosSampledDir = fabs((*sampledDir).z);

		const float3 kt = Texture_GetColorValue(&texs[material->metal.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	} else
		return BLACK;
}

#endif

//------------------------------------------------------------------------------
// ArchGlass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_ARCHGLASS)

float3 ArchGlassMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;
	const float3 reflDir = rayDir - (2.f * dot(N, rayDir)) * N;

	const float ddn = dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return BLACK;
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabs((*sampledDir).z);
			*pdfW = 1.f;

			const float3 kr = Texture_GetColorValue(&texs[material->archglass.krTexIndex], uv
					IMAGEMAPS_PARAM);
			// The cosSampledDir is used to compensate the other one used inside the integrator
			return kr / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = 1.f;

		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	} else if (u0 < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = P / Re;

		const float3 kr = Texture_GetColorValue(&texs[material->archglass.krTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kr / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabs((*sampledDir).z);
		*pdfW = (1.f - P) / Tr;

		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return kt / (*cosSampledDir);
	}
}

float3 ArchGlassMaterial_GetPassThroughTransparency(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		IMAGEMAPS_PARAM_DECL) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const float3 shadeN = (float3)(0.f, 0.f, into ? 1.f : -1.f);
	const float3 N = (float3)(0.f, 0.f, 1.f);

	const float3 rayDir = -fixedDir;

	const float ddn = dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrt(cos2t));
	const float3 nkk = kk * N;
	const float3 transDir = normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f)
		return BLACK;
	else if (Re == 0.f) {
		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt;
	} else if (passThroughEvent < P)
		return BLACK;
	else {
		const float3 kt = Texture_GetColorValue(&texs[material->archglass.ktTexIndex], uv
				IMAGEMAPS_PARAM);
		return kt;
	}
}

#endif

//------------------------------------------------------------------------------
// NULL material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_NULL)

float3 NullMaterial_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	*sampledDir = -fixedDir;
	*cosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return WHITE;
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
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return SPECULAR | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
			return SPECULAR | REFLECT;
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return SPECULAR | REFLECT | TRANSMIT;
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return SPECULAR | TRANSMIT;
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
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
#endif
		default:
			return true;
	}
}

float3 Material_Evaluate(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		IMAGEMAPS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Evaluate(material, texs, uv, lightDir, eyeDir, event, directPdfW
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
#endif
		default:
			return BLACK;
	}
}

float3 Material_Sample(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event
		IMAGEMAPS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
			return MatteMaterial_Sample(material, texs, uv, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
			return MirrorMaterial_Sample(material, texs, uv, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
			#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_Sample(material, texs, uv, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
			return GlassMaterial_Sample(material, texs, uv, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
			return MetalMaterial_Sample(material, texs, uv, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return NullMaterial_Sample(material, texs, uv, fixedDir, sampledDir, u0, u1,
#if defined(PARAM_HAS_PASSTHROUGHT)
					passThroughEvent,
#endif
					pdfW, cosSampledDir, event
					IMAGEMAPS_PARAM);
#endif
		default:
			return BLACK;
	}
}

float3 Material_GetEmittedRadiance(__global Material *material, __global Texture *texs, const float2 triUV
		IMAGEMAPS_PARAM_DECL) {
	const uint emitTexIndex = material->emitTexIndex;
	if (emitTexIndex == NULL_INDEX)
		return BLACK;

	return Texture_GetColorValue(&texs[emitTexIndex], triUV
			IMAGEMAPS_PARAM);
}

#if defined(PARAM_HAS_PASSTHROUGHT)
float3 Material_GetPassThroughTransparency(__global Material *material, __global Texture *texs,
		const float2 uv, const float3 fixedDir, const float passThroughEvent
		IMAGEMAPS_PARAM_DECL) {
	switch (material->type) {
#if defined (PARAM_ENABLE_MAT_ARCHGLASS)
		case ARCHGLASS:
			return ArchGlassMaterial_GetPassThroughTransparency(material, texs,
					uv, fixedDir, passThroughEvent
					IMAGEMAPS_PARAM);
#endif
#if defined (PARAM_ENABLE_MAT_NULL)
		case NULLMAT:
			return WHITE;
#endif
#if defined (PARAM_ENABLE_MAT_MATTE)
		case MATTE:
#endif
#if defined (PARAM_ENABLE_MAT_MIRROR)
		case MIRROR:
#endif
#if defined (PARAM_ENABLE_MAT_GLASS)
		case GLASS:
#endif
#if defined (PARAM_ENABLE_MAT_METAL)
		case METAL:
#endif
		default:
			return BLACK;
	}
}
#endif

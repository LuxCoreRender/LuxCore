#line 2 "materialdefs_funcs_glass.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
// Glass material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_GLASS)

BSDFEvent GlassMaterial_GetEventTypes() {
	return SPECULAR | REFLECT | TRANSMIT;
}

bool GlassMaterial_IsDelta() {
	return true;
}

float3 GlassMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW,
		const float3 ktTexVal, const float3 krTexVal,
		const float3 nc, const float3 nt, const bool dispersion) {
	return BLACK;
}

float3 GlassMaterial_Sample(
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const float3 ktTexVal, const float3 krTexVal,
		const float3 nc, const float3 nt, const bool dispersion) {
	if (!(requestedEvent & SPECULAR))
		return BLACK;

	const float3 kt = Spectrum_Clamp(ktTexVal);
	const float3 kr = Spectrum_Clamp(krTexVal);

	const bool isKtBlack = Spectrum_IsBlack(kt);
	const bool isKrBlack = Spectrum_IsBlack(kr);
	if (isKtBlack && isKrBlack)
		return BLACK;

	const bool entering = (CosTheta(localFixedDir) > 0.f);
	const float costheta = CosTheta(localFixedDir);

	// Decide to transmit or reflect
	float threshold;
	if ((requestedEvent & REFLECT) && !isKrBlack) {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = .5f;
		else
			threshold = 0.f;
	} else {
		if ((requestedEvent & TRANSMIT) && !isKtBlack)
			threshold = 1.f;
		else
			return BLACK;
	}

	float3 result;
	if (passThroughEvent < threshold) {
		// Transmit
	
		// Compute transmitted ray direction
		const float sini2 = SinTheta2(localFixedDir);
		
		float3 lkt;
		float lnc, lnt;
		if (dispersion) {
			// Select the wavelength to sample

			float u;
			float3 kt1, kt2;
			if (u0 < 1.f / 3.f) {
				u = 3.f * u0;
				// Between R and G sampling
				lnc = mix(nc.s0, nc.s1, u);
				lnt = mix(nt.s0, nt.s1, u);

				kt1.s0 = kt.s0;
				kt1.s1 = 0.f;
				kt1.s2 = 0.f;

				kt2.s0 = 0.f;
				kt2.s1 = kt.s1;
				kt2.s2 = 0.f;
			} else if (u0 < 2.f / 3.f) {
				u = 3.f * (u0 - 1.f / 3.f);
				// Between G and B sampling
				lnc = mix(nc.s1, nc.s2, u);
				lnt = mix(nt.s1, nt.s2, u);

				kt1.s0 = 0.f;
				kt1.s1 = kt.s1;
				kt1.s2 = 0.f;

				kt2.s0 = 0.f;
				kt2.s1 = 0.f;
				kt2.s2 = kt.s2;
			} else {
				u = 3.f * (u0 - 2.f / 3.f);
				// Between B and R sampling
				lnc = mix(nc.s2, nc.s0, u);
				lnt = mix(nt.s2, nt.s0, u);

				kt1.s0 = 0.f;
				kt1.s1 = 0.f;
				kt1.s2 = kt.s2;

				kt2.s0 = kt.s0;
				kt2.s1 = 0.f;
				kt2.s2 = 0.f;
			}

			lkt = mix(kt1, kt2, u);
		} else {
			lnc = Spectrum_Filter(nc);
			lnt = Spectrum_Filter(nt);
			lkt = kt;
		}

		const float ntc = lnt / lnc;
		const float eta = entering ? (lnc / lnt) : ntc;
		const float eta2 = eta * eta;
		const float sint2 = eta2 * sini2;
		
		// Handle total internal reflection for transmission
		if (sint2 >= 1.f)
			return BLACK;

		const float cost = sqrt(fmax(0.f, 1.f - sint2)) * (entering ? -1.f : 1.f);
		*localSampledDir = (float3)(-eta * localFixedDir.x, -eta * localFixedDir.y, cost);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;
		if (dispersion)
			*pdfW *= 1.f / 3.f;

		float ce;
		//if (!hitPoint.fromLight)
			ce = (1.f - FresnelCauchy_Evaluate(ntc, cost)) * eta2;
		//else
		//	ce = (1.f - FresnelCauchy_Evaluate(ntc, costheta));

		result = lkt * ce;
	} else {
		// Reflect
		
		*localSampledDir = (float3)(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);
		*absCosSampledDir = fabs(CosTheta(*localSampledDir));

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		if (dispersion) {
			const float3 ntc = nt / nc;
			result.s0 = kr.s0 * FresnelCauchy_Evaluate(ntc.s0, costheta);
			result.s1 = kr.s1 * FresnelCauchy_Evaluate(ntc.s1, costheta);
			result.s2 = kr.s2 * FresnelCauchy_Evaluate(ntc.s2, costheta);
		} else {
			const float ntc = Spectrum_Filter(nt) / Spectrum_Filter(nc);
			result = kr * FresnelCauchy_Evaluate(ntc, costheta);
		}
	}

	return result / *pdfW;
}

#endif

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

float3 GlassMaterial_WaveLength2RGB(const float waveLength) {
	float r, g, b;
	if ((waveLength >= 380.f) && (waveLength < 440.f)) {
		r = -(waveLength - 440.f) / (440 - 380.f);
		g = 0.f;
		b = 1.f;
	} else if ((waveLength >= 440.f) && (waveLength < 490.f)) {
		r = 0.f;
		g = (waveLength - 440.f) / (490.f - 440.f);
		b = 1.f;
	} else if ((waveLength >= 490.f) && (waveLength < 510.f)) {
		r = 0.f;
		g = 1.f;
		b = -(waveLength - 510.f) / (510.f - 490.f);
	} else if ((waveLength >= 510.f) && (waveLength < 580.f)) {
		r = (waveLength - 510.f) / (580.f - 510.f);
		g = 1.f;
		b = 0.f;
	} else if ((waveLength >= 580.f) && (waveLength < 645.f)) {
		r = 1.f;
		g = -(waveLength - 645.f) / (645 - 580.f);
		b = 0.f;
	} else if ((waveLength >= 645.f) && (waveLength < 780.f)) {
		r = 1.f;
		g = 0.f;
		b = 0.f;
	} else
		return BLACK;

	// The intensity fall off near the upper and lower limits
	float factor;
	if ((waveLength >= 380.f) && (waveLength < 420.f))
		factor = .3f + .7f * (waveLength - 380.f) / (420.f - 380.f);
	else if ((waveLength >= 420) && (waveLength < 700))
		factor = 1.f;
	else
		factor = .3f + .7f * (780.f - waveLength) / (780.f - 700.f);

	const float3 result = (float3)(r, g, b) * factor;

	/*
	Spectrum white;
	for (u_int i = 380; i < 780; ++i)
		white += WaveLength2RGB(i);
	white *= 1.f / 400.f;
	cout << std::setprecision(std::numeric_limits<float>::digits10 + 1) << white.c[0] << ", " << white.c[1] << ", " << white.c[2] << "\n";
	 
	 Result: 0.5652729, 0.36875, 0.265375
	 */

	// To normalize the output
	const float3 normFactor = (float3)(1.f / .5652729f, 1.f / .36875f, 1.f / .265375f);
	
	return result * normFactor;
}

float GlassMaterial_WaveLength2IOR(const float waveLength, const float3 IORs) {
	// Using Lagrange Interpolating Polynomial to interpolate IOR values
	//
	// The used points are (440, B), (510, G), (645, R)
	// Lagrange Interpolating Polynomial:
	//  f(x) =
	//    y1 * ((x - x2)(x - x3)) / ((x1 - x2)(x1 - x3)) +
	//    y2 * ((x - x1)(x - x3)) / ((x2 - x1)(x2 - x3)) +
	//    y3 * ((x - x1)(x - x2)) / ((x3 - x1)(x3 - x2))

	const float x1 = 440.f;
	const float y1 = IORs.s2;
	const float x2 = 510.f;
	const float y2 = IORs.s1;
	const float x3 = 645.f;
	const float y3 = IORs.s0;
	
	const float fx =
		y1 * ((waveLength - x2) * (waveLength - x3)) / ((x1 - x2) * (x1 - x3)) +
		y2 * ((waveLength - x1) * (waveLength - x3)) / ((x2 - x1) * (x2 - x3)) +
		y3 * ((waveLength - x1) * (waveLength - x2)) / ((x3 - x1) * (x3 - x2));
	
	return fx;
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
			const float waveLength = mix(380.f, 780.f, u0);

			lnc = GlassMaterial_WaveLength2IOR(waveLength, nc);
			lnt = GlassMaterial_WaveLength2IOR(waveLength, nt);

			lkt = kt * GlassMaterial_WaveLength2RGB(waveLength);
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

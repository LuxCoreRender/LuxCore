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

#include "slg/textures/fresnel/fresneltexture.h"
#include "slg/materials/carpaint.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// CarPaint material
//
// LuxRender carpaint material porting.
//------------------------------------------------------------------------------

CarPaintMaterial::CarPaintMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Texture *kd, const Texture *ks1, const Texture *ks2, const Texture *ks3, const Texture *m1, const Texture *m2, const Texture *m3,
		const Texture *r1, const Texture *r2, const Texture *r3, const Texture *ka, const Texture *d) :
			Material(frontTransp, backTransp, emitted, bump), Kd(kd), Ks1(ks1), Ks2(ks2), Ks3(ks3), M1(m1), M2(m2), M3(m3),
			R1(r1), R2(r2), R3(r3),	Ka(ka), depth(d) {
	ComputeGlossiness(M1, M2, M3);
}

Spectrum CarPaintMaterial::Albedo(const HitPoint &hitPoint) const {
	return Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

Spectrum CarPaintMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	Vector H = Normalize(localLightDir + localEyeDir);
	if (H == Vector(0.f))
	{
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
		return Spectrum();
	}
	if (H.z < 0.f)
		H = -H;

	float pdf = 0.f;
	int n = 1; // already counts the diffuse layer

	// Absorption
	const float cosi = fabsf(localLightDir.z);
	const float coso = fabsf(localEyeDir.z);
	const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float d = depth->GetFloatValue(hitPoint);
	const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// Diffuse layer
	Spectrum result = absorption * Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * INV_PI * fabsf(localLightDir.z);

	// 1st glossy layer
	const Spectrum ks1 = Ks1->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m1 = M1->GetFloatValue(hitPoint);
	if (ks1.Filter() > 0.f && m1 > 0.f)
	{
		const float rough1 = m1 * m1;
		const float r1 = R1->GetFloatValue(hitPoint);
		result += (SchlickDistribution_D(rough1, H, 0.f) * SchlickDistribution_G(rough1, localLightDir, localEyeDir) / (4.f * coso)) *
				(ks1 * FresnelTexture::SchlickEvaluate(r1, Dot(localEyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough1, H, 0.f);
		++n;
	}
	const Spectrum ks2 = Ks2->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m2 = M2->GetFloatValue(hitPoint);
	if (ks2.Filter() > 0.f && m2 > 0.f)
	{
		const float rough2 = m2 * m2;
		const float r2 = R2->GetFloatValue(hitPoint);
		result += (SchlickDistribution_D(rough2, H, 0.f) * SchlickDistribution_G(rough2, localLightDir, localEyeDir) / (4.f * coso)) *
				(ks2 * FresnelTexture::SchlickEvaluate(r2, Dot(localEyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough2, H, 0.f);
		++n;
	}
	const Spectrum ks3 = Ks3->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m3 = M3->GetFloatValue(hitPoint);
	if (ks3.Filter() > 0.f && m3 > 0.f)
	{
		const float rough3 = m3 * m3;
		const float r3 = R3->GetFloatValue(hitPoint);
		result += (SchlickDistribution_D(rough3, H, 0.f) * SchlickDistribution_G(rough3, localLightDir, localEyeDir) / (4.f * coso)) *
				(ks3 * FresnelTexture::SchlickEvaluate(r3, Dot(localEyeDir, H)));
		pdf += SchlickDistribution_Pdf(rough3, H, 0.f);
		++n;
	}

	// Front face: coating+base
	*event = GLOSSY | REFLECT;

	// Finish pdf computation
	pdf /= 4.f * AbsDot(localLightDir, H);
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;
	if (directPdfW)
		*directPdfW = (pdf + fabsf(localSampledDir.z) * INV_PI) / n;
	if (reversePdfW)
		*reversePdfW = (pdf + fabsf(localFixedDir.z) * INV_PI) / n;

	return result;
}

Spectrum CarPaintMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	// Test presence of components
	int n = 1; // already count the diffuse layer
	int sampled = 0; // sampled layer
	Spectrum result(0.f);
	float pdf = 0.f;
	bool l1 = false, l2 = false, l3 = false;
	// 1st glossy layer
	const Spectrum ks1 = Ks1->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m1 = M1->GetFloatValue(hitPoint);
	if (ks1.Filter() > 0.f && m1 > 0.f)
	{
		l1 = true;
		++n;
	}
	// 2nd glossy layer
	const Spectrum ks2 = Ks2->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m2 = M2->GetFloatValue(hitPoint);
	if (ks2.Filter() > 0.f && m2 > 0.f)
	{
		l2 = true;
		++n;
	}
	// 3rd glossy layer
	const Spectrum ks3 = Ks3->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m3 = M3->GetFloatValue(hitPoint);
	if (ks3.Filter() > 0.f && m3 > 0.f) {
		l3 = true;
		++n;
	}

	Vector wh;
	float cosWH;
	if (passThroughEvent < 1.f / n) {
		// Sample diffuse layer
		*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, &pdf);
		if (fabsf(CosTheta(*localSampledDir)) < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();

		// Absorption
		const float cosi = fabsf(localFixedDir.z);
		const float coso = fabsf(localSampledDir->z);
		const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
		const float d = depth->GetFloatValue(hitPoint);
		const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Evaluate base BSDF
		result = absorption * Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * pdf;

		wh = Normalize(*localSampledDir + localFixedDir);
		if (wh.z < 0.f)
			wh = -wh;
		cosWH = AbsDot(localFixedDir, wh);
	} else if (passThroughEvent < 2.f / n && l1) {
		// Sample 1st glossy layer
		sampled = 1;
		const float rough1 = m1 * m1;
		float d;
		SchlickDistribution_SampleH(rough1, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = Dot(localFixedDir, wh);
		*localSampledDir = 2.f * cosWH * wh - localFixedDir;
		cosWH = fabsf(cosWH);

		if ((localSampledDir->z < DEFAULT_COS_EPSILON_STATIC) ||
			(localFixedDir.z * localSampledDir->z < 0.f))
			return Spectrum();

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f)
			return Spectrum();

		result = ks1 * FresnelTexture::SchlickEvaluate(R1->GetFloatValue(hitPoint), cosWH);

		const float G = SchlickDistribution_G(rough1, localFixedDir, *localSampledDir);
		if (!hitPoint.fromLight)
			//CoatingF(sw, *wi, wo, f_);
			result *= d * G / (4.f * fabsf(localFixedDir.z));
		else
			//CoatingF(sw, wo, *wi, f_);
			result *= d * G / (4.f * fabsf(localSampledDir->z));
	} else if ((passThroughEvent < 2.f / n  ||
		(!l1 && passThroughEvent < 3.f / n)) && l2) {
		// Sample 2nd glossy layer
		sampled = 2;
		const float rough2 = m2 * m2;
		float d;
		SchlickDistribution_SampleH(rough2, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = Dot(localFixedDir, wh);
		*localSampledDir = 2.f * cosWH * wh - localFixedDir;
		cosWH = fabsf(cosWH);

		if ((localSampledDir->z < DEFAULT_COS_EPSILON_STATIC) ||
			(localFixedDir.z * localSampledDir->z < 0.f))
			return Spectrum();

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f)
			return Spectrum();

		result = ks2 * FresnelTexture::SchlickEvaluate(R2->GetFloatValue(hitPoint), cosWH);

		const float G = SchlickDistribution_G(rough2, localFixedDir, *localSampledDir);
		if (!hitPoint.fromLight)
			//CoatingF(sw, *wi, wo, f_);
			result *= d * G / (4.f * fabsf(localFixedDir.z));
		else
			//CoatingF(sw, wo, *wi, f_);
			result *= d * G / (4.f * fabsf(localSampledDir->z));
	} else if (l3) {
		// Sample 3rd glossy layer
		sampled = 3;
		const float rough3 = m3 * m3;
		float d;
		SchlickDistribution_SampleH(rough3, 0.f, u0, u1, &wh, &d, &pdf);
		cosWH = Dot(localFixedDir, wh);
		*localSampledDir = 2.f * cosWH * wh - localFixedDir;
		cosWH = fabsf(cosWH);

		if ((localSampledDir->z < DEFAULT_COS_EPSILON_STATIC) ||
			(localFixedDir.z * localSampledDir->z < 0.f))
			return Spectrum();

		pdf /= 4.f * cosWH;
		if (pdf <= 0.f)
			return Spectrum();

		result = ks3 * FresnelTexture::SchlickEvaluate(R3->GetFloatValue(hitPoint), cosWH);

		const float G = SchlickDistribution_G(rough3, localFixedDir, *localSampledDir);
		if (!hitPoint.fromLight)
			//CoatingF(sw, *wi, wo, f_);
			result *= d * G / (4.f * fabsf(localFixedDir.z));
		else
			//CoatingF(sw, wo, *wi, f_);
			result *= d * G / (4.f * fabsf(localSampledDir->z));
	} else {
		// Sampling issue
		return Spectrum();
	}
	*event = GLOSSY | REFLECT;
	// Add other components
	// Diffuse
	if (sampled != 0) {
		// Absorption
		const float cosi = fabsf(localFixedDir.z);
		const float coso = fabsf(localSampledDir->z);
		const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
		const float d = depth->GetFloatValue(hitPoint);
		const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

		const float pdf0 = fabsf((hitPoint.fromLight ? localFixedDir.z : localSampledDir->z) * INV_PI);
		pdf += pdf0;
		result += absorption * Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * pdf0;
	}
	// 1st glossy
	if (l1 && sampled != 1) {
		const float rough1 = m1 * m1;
		const float d1 = SchlickDistribution_D(rough1, wh, 0.f);
		const float pdf1 = SchlickDistribution_Pdf(rough1, wh, 0.f) / (4.f * cosWH);
		if (pdf1 > 0.f) {
			result += ks1 * (d1 *
				SchlickDistribution_G(rough1, localFixedDir, *localSampledDir) /
				(4.f * (hitPoint.fromLight ? fabsf(localSampledDir->z) : fabsf(localFixedDir.z)))) *
				FresnelTexture::SchlickEvaluate(R1->GetFloatValue(hitPoint), cosWH);
			pdf += pdf1;
		}
	}
	// 2nd glossy
	if (l2 && sampled != 2) {
		const float rough2 = m2 * m2;
		const float d2 = SchlickDistribution_D(rough2, wh, 0.f);
		const float pdf2 = SchlickDistribution_Pdf(rough2, wh, 0.f) / (4.f * cosWH);
		if (pdf2 > 0.f) {
			result += ks2 * (d2 *
				SchlickDistribution_G(rough2, localFixedDir, *localSampledDir) /
				(4.f * (hitPoint.fromLight ? fabsf(localSampledDir->z) : fabsf(localFixedDir.z)))) *
				FresnelTexture::SchlickEvaluate(R2->GetFloatValue(hitPoint), cosWH);
			pdf += pdf2;
		}
	}
	// 3rd glossy
	if (l3 && sampled != 3) {
		const float rough3 = m3 * m3;
		const float d3 = SchlickDistribution_D(rough3, wh, 0.f);
		const float pdf3 = SchlickDistribution_Pdf(rough3, wh, 0.f) / (4.f * cosWH);
		if (pdf3 > 0.f) {
			result += ks3 * (d3 *
				SchlickDistribution_G(rough3, localFixedDir, *localSampledDir) /
				(4.f * (hitPoint.fromLight ? fabsf(localSampledDir->z) : fabsf(localFixedDir.z)))) *
				FresnelTexture::SchlickEvaluate(R3->GetFloatValue(hitPoint), cosWH);
			pdf += pdf3;
		}
	}
	// Adjust pdf and result
	*pdfW = pdf / n;
	return result / *pdfW;
}

void CarPaintMaterial::Pdf(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir,
	float *directPdfW, float *reversePdfW) const {
	Vector H = Normalize(localLightDir + localEyeDir);
	if (H == Vector(0.f))
	{
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
		return;
	}
	if (H.z < 0.f)
		H = -H;

	float pdf = 0.f;
	int n = 1; // already counts the diffuse layer

	// First specular lobe
	const Spectrum ks1 = Ks1->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m1 = M1->GetFloatValue(hitPoint);
	if (ks1.Filter() > 0.f && m1 > 0.f)
	{
		const float rough1 = m1 * m1;
		pdf += SchlickDistribution_Pdf(rough1, H, 0.f);
		++n;
	}

	// Second specular lobe
	const Spectrum ks2 = Ks2->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m2 = M2->GetFloatValue(hitPoint);
	if (ks2.Filter() > 0.f && m2 > 0.f)
	{
		const float rough2 = m2 * m2;
		pdf += SchlickDistribution_Pdf(rough2, H, 0.f);
		++n;
	}

	// Third specular lobe
	const Spectrum ks3 = Ks3->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float m3 = M3->GetFloatValue(hitPoint);
	if (ks3.Filter() > 0.f && m3 > 0.f)
	{
		const float rough3 = m3 * m3;
		pdf += SchlickDistribution_Pdf(rough3, H, 0.f);
		++n;
	}

	// Finish pdf computation
	pdf /= 4.f * AbsDot(localLightDir, H);
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;
	if (directPdfW)
		*directPdfW = (pdf + fabsf(localSampledDir.z) * INV_PI) / n;
	if (reversePdfW)
		*reversePdfW = (pdf + fabsf(localFixedDir.z) * INV_PI) / n;
}

void CarPaintMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kd->AddReferencedTextures(referencedTexs);
	Ks1->AddReferencedTextures(referencedTexs);
	Ks2->AddReferencedTextures(referencedTexs);
	Ks3->AddReferencedTextures(referencedTexs);
	M1->AddReferencedTextures(referencedTexs);
	M2->AddReferencedTextures(referencedTexs);
	M3->AddReferencedTextures(referencedTexs);
	R1->AddReferencedTextures(referencedTexs);
	R2->AddReferencedTextures(referencedTexs);
	R3->AddReferencedTextures(referencedTexs);
	Ka->AddReferencedTextures(referencedTexs);
	depth->AddReferencedTextures(referencedTexs);
}

void CarPaintMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	bool updateGlossiness = false;
	if (Kd == oldTex)
		Kd = newTex;
	if (Ks1 == oldTex)
		Ks1 = newTex;
	if (Ks2 == oldTex)
		Ks2 = newTex;
	if (Ks3 == oldTex)
		Ks3 = newTex;
	if (M1 == oldTex) {
		M1 = newTex;
		updateGlossiness = true;
	}
	if (M2 == oldTex) {
		M2 = newTex;
		updateGlossiness = true;
	}
	if (M3 == oldTex) {
		M3 = newTex;
		updateGlossiness = true;
	}
	if (R1 == oldTex)
		R1 = newTex;
	if (R2 == oldTex)
		R2 = newTex;
	if (R3 == oldTex)
		R3 = newTex;
	if (Ka == oldTex)
		Ka = newTex;
	if (depth == oldTex)
		depth = newTex;
	
	if (updateGlossiness)
		ComputeGlossiness(M1, M2, M3);
}

Properties CarPaintMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("carpaint"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ks1")(Ks1->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ks2")(Ks2->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ks3")(Ks3->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".m1")(M1->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".m2")(M2->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".m3")(M3->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".r1")(R1->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".r2")(R2->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".r3")(R3->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ka")(Ka->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".d")(depth->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}

const struct CarPaintMaterial::CarPaintData CarPaintMaterial::data[8] = {
  {"ford f8",
   {0.0012f, 0.0015f, 0.0018f},
   {0.0049f, 0.0076f, 0.0120f},
   {0.0100f, 0.0130f, 0.0180f},
   {0.0070f, 0.0065f, 0.0077f},
    0.1500f, 0.0870f, 0.9000f,
    0.3200f, 0.1100f, 0.0130f},
  {"polaris silber",
   {0.0550f, 0.0630f, 0.0710f},
   {0.0650f, 0.0820f, 0.0880f},
   {0.1100f, 0.1100f, 0.1300f},
   {0.0080f, 0.0130f, 0.0150f},
    1.0000f, 0.9200f, 0.9000f,
    0.3800f, 0.1700f, 0.0130f},
  {"opel titan",
   {0.0110f, 0.0130f, 0.0150f},
   {0.0570f, 0.0660f, 0.0780f},
   {0.1100f, 0.1200f, 0.1300f},
   {0.0095f, 0.0140f, 0.0160f},
    0.8500f, 0.8600f, 0.9000f,
    0.3800f, 0.1700f, 0.0140f},
  {"bmw339",
   {0.0120f, 0.0150f, 0.0160f},
   {0.0620f, 0.0760f, 0.0800f},
   {0.1100f, 0.1200f, 0.1200f},
   {0.0083f, 0.0150f, 0.0160f},
    0.9200f, 0.8700f, 0.9000f,
    0.3900f, 0.1700f, 0.0130f},
  {"2k acrylack",
   {0.4200f, 0.3200f, 0.1000f},
   {0.0000f, 0.0000f, 0.0000f},
   {0.0280f, 0.0260f, 0.0060f},
   {0.0170f, 0.0075f, 0.0041f},
    1.0000f, 0.9000f, 0.1700f,
    0.8800f, 0.8000f, 0.0150f},
  {"white",
   {0.6100f, 0.6300f, 0.5500f},
   {2.6e-06f, 0.00031f, 3.1e-08f},
   {0.0130f, 0.0110f, 0.0083f},
   {0.0490f, 0.0420f, 0.0370f},
    0.0490f, 0.4500f, 0.1700f,
    1.0000f, 0.1500f, 0.0150f},
  {"blue",
   {0.0079f, 0.0230f, 0.1000f},
   {0.0011f, 0.0015f, 0.0019f},
   {0.0250f, 0.0300f, 0.0430f},
   {0.0590f, 0.0740f, 0.0820f},
    1.0000f, 0.0940f, 0.1700f,
    0.1500f, 0.0430f, 0.0200f},
  {"blue matte",
   {0.0099f, 0.0360f, 0.1200f},
   {0.0032f, 0.0045f, 0.0059f},
   {0.1800f, 0.2300f, 0.2800f},
   {0.0400f, 0.0490f, 0.0510f},
    1.0000f, 0.0460f, 0.1700f,
    0.1600f, 0.0750f, 0.0340f}
};

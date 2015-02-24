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

#include <boost/format.hpp>

#include "slg/lights/infinitelight.h"
#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight() :
	imageMap(NULL), mapping(1.f, 1.f, 0.f, 0.f) {
}

InfiniteLight::~InfiniteLight() {
	delete imageMapDistribution;
}

void InfiniteLight::Preprocess() {
	if (imageMap->GetChannelCount() == 1)
		imageMapDistribution = new Distribution2D(imageMap->GetPixels(), imageMap->GetWidth(), imageMap->GetHeight());
	else {
		const float *pixels = imageMap->GetPixels();
		float *data = new float[imageMap->GetWidth() * imageMap->GetHeight()];
		for (u_int y = 0; y < imageMap->GetHeight(); ++y) {
			for (u_int x = 0; x < imageMap->GetWidth(); ++x) {
				const u_int index = x + y * imageMap->GetWidth();
				const float *pixel = &pixels[index * imageMap->GetChannelCount()];
				
				data[index] = Spectrum(pixel).Y();
			}
		}

		imageMapDistribution = new Distribution2D(data, imageMap->GetWidth(), imageMap->GetHeight());
		delete data;
	}
}

void InfiniteLight::GetPreprocessedData(const Distribution2D **imageMapDistributionData) const {
	if (imageMapDistributionData)
		*imageMapDistributionData = imageMapDistribution;
	
}

float InfiniteLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return gain.Y() * imageMap->GetSpectrumMeanY() *
			(4.f * M_PI * M_PI * worldRadius * worldRadius);
}

Spectrum InfiniteLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);
	const UV uv(SphericalPhi(localDir) * INV_TWOPI, SphericalTheta(localDir) * INV_PI);

	const float distPdf = imageMapDistribution->Pdf(uv.u, uv.v);
	if (directPdfA)
		*directPdfA = distPdf / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = distPdf / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * imageMap->GetSpectrum(mapping.Map(uv));
}

Spectrum InfiniteLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	// Choose p1 on scene bounding sphere according importance sampling
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);

	const float phi = uv[0] * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	Point p1 = worldCenter + worldRadius * SphericalDirection(sinf(theta), cosf(theta), phi);

	// Choose p2 on scene bounding sphere
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize(lightToWorld * (p2 - p1));

	// Compute InfiniteLight ray weight
	*emissionPdfW = distPdf / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = distPdf / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir);
}

Spectrum InfiniteLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);

	const float phi = uv[0] * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	*dir = Normalize(lightToWorld * SphericalDirection(sinf(theta), cosf(theta), phi));

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));

	const Point emisPoint(p + (*distance) * (*dir));
	const Normal emisNormal(Normalize(worldCenter - emisPoint));

	const float cosAtLight = Dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();
	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = distPdf / (4.f * M_PI);

	if (emissionPdfW)
		*emissionPdfW = distPdf / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	return gain * imageMap->GetSpectrum(mapping.Map(UV(uv[0], uv[1])));
}

Properties InfiniteLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("infinite"));
	props.Set(Property(prefix + ".file")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imageMap)).str() + ".exr"));
	props.Set(Property(prefix + ".shift")(mapping.uDelta, mapping.vDelta));

	return props;
}

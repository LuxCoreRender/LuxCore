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

#include "slg/textures/cloud.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Cloud texture
//------------------------------------------------------------------------------

CloudTexture::CloudTexture(const TextureMapping3D *mp, const float r, const float noiseScale, const float t,
	const float sharp, const float v, const float baseflatness, const u_int octaves, const float o, const float offset,
	const u_int numspheres, const float spheresize) : radius(r), numSpheres(numspheres), sphereSize(spheresize),
													  sharpness(sharp), baseFlatness(baseflatness), variability(v),
													  omega(o), firstNoiseScale(noiseScale), noiseOffset(offset),
													  turbulenceAmount(t), numOctaves(octaves), mapping(mp) {
	
	cumulus = numSpheres > 0;
	baseFadeDistance = 1.f - baseFlatness;

	// Centre of main hemisphere shape
	sphereCentre = Point(.5f, .5f, 1.f / 3.f);

	if (cumulus) {
		RandomGenerator rng(static_cast<unsigned long>(std::numeric_limits<unsigned long>::max() * noiseOffset));
		// Create spheres for cumulus shape.
		// Each one should be on the edge of the hemisphere
		spheres = new CumulusSphere[numSpheres];
		
		for (u_int i = 0; i < numSpheres; ++i) {
			spheres[i].radius = (CloudRand(rng, 10) / 2.f + 0.5f) *
				sphereSize;
			Vector onEdge(radius / 2.f * CloudRand(rng, 1000), 0.f, 0.f);
			const float angley = -180.f * CloudRand(rng, 1000);
			const float anglez = 360.f * CloudRand(rng, 1000);
			onEdge = RotateZ(anglez) * (RotateY(angley) * onEdge);
			Point finalPosition(sphereCentre + onEdge);
			finalPosition += Turbulence(finalPosition +
				Vector(noiseOffset * 4.f, 0.f, 0.f),
				radius * 0.7f, 2) * radius * 1.5f;
			spheres[i].position = finalPosition;
		}
	} else
		spheres = NULL;
}

float CloudTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point P(mapping->Map(hitPoint));
	const float amount = CloudShape(P + turbulenceAmount * Turbulence(P, firstNoiseScale, numOctaves));
	const float finalValue = powf(amount * powf(10.f, .7f), sharpness);

	return min(finalValue, 1.f);
}

Spectrum CloudTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Vector CloudTexture::Turbulence(const Point &p, const float noiseScale, const u_int octaves) const {
	Point noiseCoords[3];
	noiseCoords[0] = p / noiseScale;
	noiseCoords[1] = noiseCoords[0] + Vector(noiseOffset, noiseOffset, noiseOffset);
	noiseCoords[2] = noiseCoords[1] + Vector(noiseOffset, noiseOffset, noiseOffset);

	float noiseAmount = 1.f;

	if (variability < 1.f)
		noiseAmount = Lerp(variability, 1.f, NoiseMask(p + Vector(noiseOffset * 4.f, 0.f, 0.f)));

	noiseAmount = Clamp(noiseAmount, 0.f, 1.f);

	Vector turbulence;

	turbulence.x = CloudNoise(noiseCoords[0], omega, octaves) - 0.15f;
	turbulence.y = CloudNoise(noiseCoords[1], omega, octaves) - 0.15f;
	turbulence.z = -CloudNoise(noiseCoords[2], omega, octaves);
	if (p.z < sphereCentre.z + baseFadeDistance)
		turbulence.z *= (p.z - sphereCentre.z) / (2.f * baseFadeDistance);

	turbulence *= noiseAmount;	
		
	return turbulence;
}

Vector CloudTexture::Turbulence(const Vector &v, const float noiseScale, const u_int octaves) const {
	return Turbulence(Point(v.x, v.y, v.z), noiseScale, octaves);
}

float CloudTexture::CloudNoise(const Point &p, const float omegaValue, const u_int octaves) const {
	// Compute sum of octaves of noise
	float sum = 0.f, lambda = 1.f, o = 1.f;
	for (u_int i = 0; i < octaves; ++i) {
		sum += o * Noise(lambda * p);
		lambda *= 1.99f;
		o *= omegaValue;
	}
	return sum;
}

float CloudTexture::CloudShape(const Point &p) const {
	if (cumulus) {
		if (SphereFunction(p))		//shows cumulus spheres
			return 1.f;
		else
			return 0.f;
	}

	const Vector fromCentre(p - sphereCentre);

	float amount = 1.f - fromCentre.Length() / radius;
	if (amount < 0.f)
		return 0.f;

	// The base below the cloud's height fades out
	if (p.z < sphereCentre.z) {
		if (p.z < sphereCentre.z - radius * 0.4f)
			return 0.f;

		amount *= 1.f - cosf((fromCentre.z + baseFadeDistance) /
			baseFadeDistance * M_PI * 0.5f);
	}
	return max(amount, 0.f);
}

bool CloudTexture::SphereFunction(const Point &p) const {
// Returns whether a point is inside one of the cumulus spheres
	for (u_int i = 0; i < numSpheres; ++i) {
		if ((p - spheres[i].position).Length() < spheres[i].radius)
			return true;
	}
	return false;
}

Properties CloudTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("cloud"));
	props.Set(Property("scene.textures." + name + ".radius")(radius));
	props.Set(Property("scene.textures." + name + ".noisescale")(firstNoiseScale));
	props.Set(Property("scene.textures." + name + ".turbulence")(turbulenceAmount));
	props.Set(Property("scene.textures." + name + ".sharpness")(sharpness));
	props.Set(Property("scene.textures." + name + ".noiseoffset")(noiseOffset));
	props.Set(Property("scene.textures." + name + ".spheres")(numSpheres));
	props.Set(Property("scene.textures." + name + ".octaves")(numOctaves));
	props.Set(Property("scene.textures." + name + ".variability")(variability));
	props.Set(Property("scene.textures." + name + ".baseflatness")(baseFlatness));
	props.Set(Property("scene.textures." + name + ".spheresize")(sphereSize));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

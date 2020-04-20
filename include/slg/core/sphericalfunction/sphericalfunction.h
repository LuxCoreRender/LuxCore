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

#ifndef _SLG_SPHERICALFUNCTION_H
#define _SLG_SPHERICALFUNCTION_H

#include "luxrays/utils/mcdistribution.h"
#include "slg/core/sphericalfunction/photometricdataies.h"
#include "slg/textures/texture.h"

namespace slg {

/**
 * A simple interface for functions on a sphere.
 */
class SphericalFunction {
public:
	virtual ~SphericalFunction() { }

	/**
	 * Evaluates this function for the given direction.
	 * 
	 * @param w A normalized direction.
	 *
	 * @return The function value for the given direction.
	 */
	luxrays::Spectrum Evaluate(const luxrays::Vector &w) const {
		return Evaluate(luxrays::SphericalPhi(w), luxrays::SphericalTheta(w));
	}

	/**
	 * Evaluates this function for the given direction.
	 * 
	 * @param phi   The angle in the xy plane.
	 * @param theta The angle from the z axis.
	 *
	 * @return The function value for the given direction.
	 */
	virtual luxrays::Spectrum Evaluate(const float phi, const float theta) const = 0;
};

/**
 * A basic spherical function that is 1 everywhere.
 */
class NopSphericalFunction : public SphericalFunction {
public:
	virtual luxrays::Spectrum Evaluate(const float phi, const float theta) const {
		return luxrays::Spectrum(1.f);
	}
};

/**
 * A spherical function that obtains its function values from an ImageMap.
 */
class ImageMapSphericalFunction : public SphericalFunction {
public:
	ImageMapSphericalFunction();
	ImageMapSphericalFunction(const ImageMap *imgMap);

	void SetImageMap(const ImageMap *imgMap);
	const ImageMap *GetImageMap() const { return imgMap; }
	
	virtual luxrays::Spectrum Evaluate(const float phi, const float theta) const;

protected:
	const ImageMap *imgMap;
};

/**
 * A spherical function that composes multiple spherical functions
 * by multiplying their results.
 */
class CompositeSphericalFunction : public SphericalFunction {
public:
	CompositeSphericalFunction() { }
	virtual ~CompositeSphericalFunction() {
		for (u_int i = 0; i < funcs.size(); ++i)
			delete funcs[i];
	}

	void Add(const SphericalFunction *aFunc) {
		funcs.push_back(aFunc);
	}

	virtual luxrays::Spectrum Evaluate(const float phi, const float theta) const {
		luxrays::Spectrum ret(1.f);
		for (u_int i = 0; i < funcs.size(); ++i)
			ret *= funcs[i]->Evaluate(phi, theta);
		return ret;
	}

private:
	std::vector<const SphericalFunction *> funcs;
};

/**
 * A spherical function that allows efficient sampling.
 */
class SampleableSphericalFunction : public SphericalFunction {
public:
	SampleableSphericalFunction(const SphericalFunction *aFunc, 
		const u_int xRes = 512, const u_int yRes = 256);
	~SampleableSphericalFunction();

	virtual luxrays::Spectrum Evaluate(const float phi, const float theta) const;

	/**
	 * Samples this spherical function.
	 *
	 * @param u1  The first random value.
	 * @param u2  The second random value.
	 * @param w   The address to store the sampled direction in.
	 * @param pdf The address to store the pdf (w.r.t. solid angle) of the 
	 *            sample direction in.
	 *
	 * @return The function value of the sampled direction.
	 */
	luxrays::Spectrum Sample(const float u1, const float u2, luxrays::Vector *w, float *pdf) const;

	/**
	 * Computes the pdf for sampling the given direction.
	 *
	 * @param w The direction.
	 *
	 * @return The pdf (w.r.t. solid angle) for the direction.
	 */
	float Pdf(const luxrays::Vector &w) const;

	/**
	 * Returns the average function value over the entire sphere.
	 *
	 * @return The average function value.
	 */
	float Average() const;

	const SphericalFunction *GetFunc() const { return func; }
	const luxrays::Distribution2D *GetDistribution2D() const { return uvDistrib; }

private:
	luxrays::Distribution2D *uvDistrib;
	const SphericalFunction *func;
	float average;
};

/**
 * A spherical function based on measured IES data.
 */
class IESSphericalFunction : public ImageMapSphericalFunction {
public:
	IESSphericalFunction(const PhotometricDataIES &data, const bool flipZ,
			const u_int xRes = 512, const u_int yRes = 256);
	~IESSphericalFunction();

	static ImageMap *IES2ImageMap(const PhotometricDataIES &data, const bool flipZ,
			const u_int xRes = 512, const u_int yRes = 256);
};

}

#endif //_SLG_SPHERICALFUNCTION_H

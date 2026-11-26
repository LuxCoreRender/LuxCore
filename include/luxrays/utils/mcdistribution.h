/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_MCDISTRIBUTION_H
#define _LUXRAYS_MCDISTRIBUTION_H

#include <memory>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <span>

#include "luxrays/utils/mc.h"
#include "luxrays/utils/serializationutils.h"
#include "luxrays/usings.h"

namespace luxrays {

/**
 * A utility class evaluating a regularly sampled 1D function.
 */
class Function1D {
public:
	/**
	 * Creates a 1D function from the given data.
	 * It is assumed that the given function is sampled regularly sampled in
	 * the interval [0,1] (ex. 0.1, 0.3, 0.5, 0.7, 0.9 for 5 samples).
	 *
	 * @param s The values of the function.
	 */
	Function1D(std::span<float> source) {
		func.reserve(source.size());

		std::ranges::copy(source.begin(), source.end(), std::back_inserter(func));
	}

	/**
	 * Evaluates the function at the given position.
	 * 
	 * @param x The x value to evaluate the function at.
	 *
	 * @return The function value at the given position.
	 */
	float Eval(float x) const {
		auto count = func.size();
		float pos = Clamp(x, 0.f, 1.f) * count + .5f;
		auto off1 = static_cast<decltype(count)>(pos);
		auto off2 = std::min(count-1, off1 + 1);
		float d = pos - off1;
		return func[off1] * (1.f - d) * func[off2] * d;
	}

	// Function1D Data
	/*
	 * The function values.
	 */
	std::vector<float> func;
};

/**
 * A utility class for sampling from a regularly sampled 1D distribution.
 */
class Distribution1D {
public:
	/**
	 * Creates a 1D distribution for the given function.
	 * It is assumed that the given function is sampled regularly sampled in
	 * the interval [0,1] (ex. 0.1, 0.3, 0.5, 0.7, 0.9 for 5 samples).
	 *
	 * @param f The values of the function.
	 * @param n The number of samples.
	 */
	Distribution1D(std::span<float> data);

	/**
	 * Samples a point from this distribution.
	 * The pdf is computed so that int(u=0..1, pdf(u)*du) = 1
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample
	 *            should be stored.
	 * @param off Optional parameter to get the offset of the value
	 *
	 * @return The x value of the sample (i.e. the x in f(x)).
	 */ 
	float SampleContinuous(float u, float *pdf, u_int *off = nullptr) const;

	/**
	 * Samples an interval from this distribution.
	 * The pdf is computed so that sum(i=0..n-1, pdf(i)) = 1
	 * with n the number of intervals
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample
	 *            should be stored.
	 * @param du  Optional parameter to get the remaining offset
	 *
	 * @return The index of the sampled interval.
	 */ 
	u_int SampleDiscrete(float u, float *pdf, float *du = nullptr) const;
	/**
	 * The pdf associated to a given interval
	 * 
	 * @param offset The interval number in the [0,n) range
	 *
	 * @return The pdf so that sum(i=0..n-1, pdf(i)) = 1
	 */
	float PdfDiscrete(u_int offset) const { return func[offset] * invCount; }
	/**
	 * The pdf associated to a given point
	 * 
	 * @param offset The point position in the [0,1) range
	 *
	 * @return The pdf so that int(u=0..1, pdf(u)*du) = 1
	 */
	float Pdf(float u, float *du = nullptr) const;

	float Average() const { return funcInt; }
	u_int Offset(float u) const {
		return Min(count - 1, Floor2UInt(u * count));
	}

	const u_int GetCount() const { return count; }
	const float *GetFuncs() const { return &func[0]; }
	const float *GetCDFs() const { return &cdf[0]; }

	friend class boost::serialization::access;

private:
	// Used by serialization
	Distribution1D() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & func;
		ar & cdf;
		ar & funcInt;
		ar & invCount;
		ar & count;
	}

	// Distribution1D Private Data
	/*
	 * The function and its cdf.
	 */
	std::vector<float> func, cdf;
	/**
	 * The function integral (assuming it is regularly sampled with an interval of 1),
	 * the inverted function integral and the inverted count.
	 */
	float funcInt, invCount;
	/*
	 * The number of function values. The number of cdf values is count+1.
	 */
	u_int count;
};

class Distribution2D {
public:
	// Distribution2D Public Methods
	Distribution2D(std::span<float> data, u_int nu, u_int nv);

	void SampleContinuous(float u0, float u1, float uv[2],
		float *pdf) const;
	void SampleDiscrete(float u0, float u1, u_int uv[2], float *pdf,
			float *du0 = nullptr, float *du1 = nullptr) const;

	float Pdf(float u, float v,
			float *du = nullptr, float *dv = nullptr,
			u_int *offsetU = nullptr, u_int *offsetV = nullptr) const;

	float Average() const { return pMarginal->Average(); }

	const u_int GetWidth() const { return pConditionalV[0]->GetCount(); }
	const u_int GetHeight() const { return pMarginal->GetCount(); }
	Distribution1DRPtr GetMarginalDistribution() const { return pMarginal; }
	Distribution1DRPtr GetConditionalDistribution(const u_int i) const {
		return pConditionalV[i];
	}

	friend class boost::serialization::access;

private:
	// Used by serialization
	Distribution2D() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & pConditionalV;
		ar & pMarginal;
	}

	// Distribution2D Private Data
	std::vector<std::unique_ptr<Distribution1D>> pConditionalV;
	std::unique_ptr<Distribution1D> pMarginal;
};

/**
 * A utility class for evaluating an irregularly sampled 1D function.
 */
class IrregularFunction1D {
public:
	/**
	 * Creates a 1D function from the given data.
	 * It is assumed that the given x values are ordered, starting with the
	 * smallest value. The function value is clamped at the edges. It is
	 * assumed there are no duplicate sample locations.
	 *
	 * @param aX   The sample locations of the function.
	 * @param aFx  The values of the function.
	 * @param aN   The number of samples.
	 */
	IrregularFunction1D(std::span<float> aX, std::span<float> aFx) :
		xFunc(aX.begin(), aX.end()), yFunc(aFx.begin(), aFx.end())
	{
		if (aX.size() != aFx.size()) {
			throw std::runtime_error("IrregularFunction1D: misaligned arguments.");
		}
	}

	~IrregularFunction1D() {}

	/**
	 * Evaluates the function at the given position.
	 * 
	 * @param x The x value to evaluate the function at.
	 *
	 * @return The function value at the given position.
	 */
	float Eval(float x) const {
		auto count = xFunc.size();
		if (x <= xFunc[0])
			return yFunc[0];
		if (x >= xFunc[count - 1])
			return yFunc[count - 1];

		auto upper = std::upper_bound(xFunc.begin(), xFunc.end(), x);
		//const u_int offset = static_cast<u_int>(ptr - xFunc - 1);
		const size_t offset = std::distance(xFunc.begin(), upper) - 1;

		float d = (x - xFunc[offset]) / (xFunc[offset + 1] - xFunc[offset]);

		return Lerp(d, yFunc[offset], yFunc[offset + 1]);
	}

	/**
	 * Returns the index of the given position.
	 * 
	 * @param x The x value to get the index of.
	 * @param d The address to store the offset from the index in.
	 *
	 * @return The index of the given position.
	 */
	int IndexOf(float x, float *d) const {
		auto count = xFunc.size();
		if (x <= xFunc[0]) {
			*d = 0.f;
			return 0;
		}
		if (x >= xFunc[count - 1]) {
			*d = 0.f;
			return count - 1;
		}

		auto upper = std::upper_bound(xFunc.begin(), xFunc.end(), x);
		auto offset = std::distance(xFunc.begin(), upper) - 1;

		*d = (x - xFunc[offset]) / (xFunc[offset + 1] - xFunc[offset]);
		return offset;
	}

private:
	// IrregularFunction1D Data
	/*
	 * The sample locations and the function values.
	 */
	std::vector<float> xFunc, yFunc;
};

/**
 * A utility class for sampling from a irregularly sampled 1D distribution.
 */
class IrregularDistribution1D {
public:
	/**
	 * Creates a 1D distribution for the given function.
	 * It is assumed that the given x values are ordered, starting with the
	 * smallest value.
	 *
	 * @param aX0 The start of the sample interval.
	 * @param aX1 The end of the sample interval.
	 * @param aX  The sample locations of the function.
	 * @param aFx The values of the function.
	 * @param aN  The number of samples.
	 */
	IrregularDistribution1D(
		float aX0, float aX1,
		std::span<float> aX,
		std::span<float> aFx
	) :
		xFunc(aX.begin(), aX.end()),
		yFunc(aFx.begin(), aFx.end()),
		xCdf(aX.size()),
		yCdf(aX.size())
	{
		if (aX.size() != aFx.size()) {
			throw std::runtime_error("IrregularDistribution1D: misaligned arguments.");
		}

		count = aX.size();
		x0 = aX0;
		x1 = aX1;

		// Compute integrals of step function
		xCdf[0] = aX0;
		for (int i = 1; i < count; ++i)
			xCdf[i] = ( xFunc[i-1] + xFunc[i] ) * .5f;
		xCdf[count] = aX1;
		yCdf[0] = 0.f;
		for (int i = 1; i < count+1; ++i) {
			yCdf[i] = yCdf[i-1] + std::max( 1e-3f, yFunc[i-1] ) * ( xCdf[i] - xCdf[i-1] );
		}
		funcInt = yCdf[count];
		// Transform step function integral into cdf
		for (int i = 1; i < count+1; ++i)
			yCdf[i] /= funcInt;

		invFuncInt = 1.f / funcInt;
		invCount = 1.f / count;
	}


	/**
	 * Samples from this distribution.
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample should 
	 *            be stored.
	 *
	 * @return The x value of the sample (i.e. the x in f(x)).
	 */ 
	std::tuple<float, float>
	Sample(float u) const {
		auto count = xFunc.size();
		// Find surrounding cdf segments
		if (u >= yCdf[count]) {
			auto pdf = xFunc[count] * invFuncInt;
			return std::make_tuple(xCdf[count], pdf);
		}
		if (u <= yCdf[0]) {
			auto pdf = xFunc[0] * invFuncInt;
			return std::make_tuple(xCdf[0], pdf);
		}
		auto upper = std::upper_bound(yCdf.begin(), yCdf.end(), u);
		auto offset = std::distance(yCdf.begin(), upper) - 1;
		// Return offset along current cdf segment
		float du = (u - yCdf[offset]) / (yCdf[offset + 1] - yCdf[offset]);
		auto pdf = xFunc[offset] * invFuncInt;
		return std::make_tuple(Lerp(du, xCdf[offset], xCdf[offset + 1]), pdf);
	}

	/**
	 * Evaluates the function at the given position.
	 * 
	 * @param x The x value to evaluate the function at.
	 *
	 * @return The function value at the given position.
	 */
	float Eval(float x) const {
		if (x <= xFunc[0])
			return yFunc[0];
		if (x >= xFunc[count - 1])
			return yFunc[count - 1];

		auto upper = std::upper_bound(xFunc.begin(), xFunc.end(), x);
		auto offset = std::distance(xFunc.begin(), upper) - 1;

		float d = (x - xFunc[offset]) / (xFunc[offset + 1] - xFunc[offset]);

		return Lerp(d, yFunc[offset], yFunc[offset + 1]);
	}

	/**
	 * Returns the index of the given position.
	 * 
	 * @param x The x value to get the index of.
	 * @param d The address to store the offset from the index in.
	 *
	 * @return The index of the given position.
	 */
	std::tuple<size_t, float>
	IndexOf(const float x) const {
		if (x <= xFunc[0]) {
			return std::tuple<size_t, float>(0, 0.f);
		}

		if (x >= xFunc[xFunc.size() - 1]) {
			return std::tuple<size_t, float>(xFunc.size() - 1, 0.f);
		}

		auto upper = std::upper_bound(xFunc.begin(), xFunc.end(), x);
		auto offset = std::distance(xFunc.begin(), upper) - 1;
		float d = (x - xFunc[offset]) / (xFunc[offset + 1] - xFunc[offset]);

		return std::make_tuple(offset, d);
	}

	// IrregularDistribution1D Data
	/**
	 * The function interval.
	 */
	float x0, x1;
	/*
	 * The sample locations and the function values.
	 */
	std::vector<float> xFunc, yFunc;
	/*
	 * The sample locations of the cdf and the cdf values.
	 */
	std::vector<float> xCdf, yCdf;
	/**
	 * The function integral (of the scaled function!),
	 * the inverted function integral and the inverted count.
	 */
	float funcInt, invFuncInt, invCount;
	/*
	 * The number of function values. The number of cdf values is count+1.
	 */
	int count;
};

}

BOOST_CLASS_VERSION(luxrays::Distribution1D, 1)
BOOST_CLASS_VERSION(luxrays::Distribution2D, 1)

BOOST_CLASS_EXPORT_KEY(luxrays::Distribution1D)
BOOST_CLASS_EXPORT_KEY(luxrays::Distribution2D)

#endif //_LUXRAYS_MCDISTRIBUTION_H
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

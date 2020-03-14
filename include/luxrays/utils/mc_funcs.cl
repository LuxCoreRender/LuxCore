#line 2 "mc_funcs.cl"

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

OPENCL_FORCE_INLINE void ConcentricSampleDisk(const float u0, const float u1, float *dx, float *dy) {
	float r, theta;
	// Map uniform random numbers to $[-1,1]^2$
	const float sx = 2.f * u0 - 1.f;
	const float sy = 2.f * u1 - 1.f;
	// Map square to $(r,\theta)$
	// Handle degeneracy at the origin
	if (sx == 0.f && sy == 0.f) {
		*dx = 0.f;
		*dy = 0.f;
		return;
	}
	if (sx >= -sy) {
		if (sx > sy) {
			// Handle first region of disk
			r = sx;
			if (sy > 0.f)
				theta = sy / r;
			else
				theta = 8.f + sy / r;
		} else {
			// Handle second region of disk
			r = sy;
			theta = 2.f - sx / r;
		}
	} else {
		if (sx <= sy) {
			// Handle third region of disk
			r = -sx;
			theta = 4.f - sy / r;
		} else {
			// Handle fourth region of disk
			r = -sy;
			theta = 6.f + sx / r;
		}
	}
	theta *= M_PI_F / 4.f;
	*dx = r * cos(theta);
	*dy = r * sin(theta);
}

OPENCL_FORCE_INLINE float3 CosineSampleHemisphere(const float u0, const float u1) {
	float x, y;
	ConcentricSampleDisk(u0, u1, &x, &y);

	const float z = sqrt(fmax(0.f, 1.f - x * x - y * y));

	return (float3)(x, y, z);
}

OPENCL_FORCE_INLINE float3 CosineSampleHemisphereWithPdf(const float u0, const float u1, float *pdfW) {
	float x, y;
	ConcentricSampleDisk(u0, u1, &x, &y);

	const float z = sqrt(fmax(0.f, 1.f - x * x - y * y));

	*pdfW = z * M_1_PI_F;

	return (float3)(x, y, z);
}

OPENCL_FORCE_INLINE float3 UniformSampleHemisphere(const float u1, const float u2) {
	const float z = u1;
	const float r = sqrt(fmax(0.f, 1.f - z*z));
	const float phi = 2.f * M_PI_F * u2;
	const float x = r * cos(phi);
	const float y = r * sin(phi);

	return (float3)(x, y, z);
}

OPENCL_FORCE_INLINE float3 UniformSampleSphere(const float u1, const float u2) {
	const float z = 1.f - 2.f * u1;
	const float r = sqrt(fmax(0.f, 1.f - z * z));
	const float phi = 2.f * M_PI_F * u2;
	const float x = r * cos(phi);
	const float y = r * sin(phi);

	return (float3)(x, y, z);
}

OPENCL_FORCE_INLINE float UniformSpherePdf() {
	return 1.f / (4.f * M_PI_F);
}

OPENCL_FORCE_INLINE float3 UniformSampleConeLocal(const float u0, const float u1, float costhetamax) {
	const float costheta = mix(1.f, costhetamax, u0);
	const float u0x = (1.f - costhetamax) * u0;
	const float sintheta = sqrt(u0x * (2.f - u0x));
	const float phi = u1 * 2.f * M_PI_F;

	return (float3)(cos(phi) * sintheta, sin(phi) * sintheta, costheta);
}

OPENCL_FORCE_INLINE float3 UniformSampleCone(const float u0, const float u1, const float costhetamax,
	const float3 x, const float3 y, const float3 z) {
	const float costheta = mix(1.f, costhetamax, u0);
	const float u0x = (1.f - costhetamax) * u0;
	const float sintheta = sqrt(u0x * (2.f - u0x));
	const float phi = u1 * 2.f * M_PI_F;

	const float kx = cos(phi) * sintheta;
	const float ky = sin(phi) * sintheta;
	const float kz = costheta;

	return (float3)(kx * x.x + ky * y.x + kz * z.x,
			kx * x.y + ky * y.y + kz * z.y,
			kx * x.z + ky * y.z + kz * z.z);
}

OPENCL_FORCE_INLINE float UniformConePdf(const float costhetamax) {
	return 1.f / (2.f * M_PI_F * (1.f - costhetamax));
}

OPENCL_FORCE_INLINE float PowerHeuristic(const float fPdf, const float gPdf) {
	const float f = fPdf;
	const float g = gPdf;

	return (f * f) / (f * f + g * g);
}

OPENCL_FORCE_INLINE float PdfWtoA(const float pdfW, const float dist, const float cosThere) {
    return pdfW * fabs(cosThere) / (dist * dist);
}

OPENCL_FORCE_INLINE float PdfAtoW(const float pdfA, const float dist, const float cosThere) {
    return pdfA * dist * dist / fabs(cosThere);
}

//------------------------------------------------------------------------------
// Distribution1D
//------------------------------------------------------------------------------

// Implementation of std::upper_bound()
OPENCL_FORCE_INLINE __global const float *std_upper_bound(__global const float* restrict first,
		__global const float* restrict last, const float val) {
	__global const float* restrict it;
	uint count = last - first;
	uint step;

	while (count > 0) {
		it = first;
		step = count / 2;
		it += step;

		if (!(val < *it)) {
			first = ++it;
			count -= step + 1;
		} else
			count = step;
	}

	return first;
}

//__global const float *std_upper_bound(__global const float* restrict first, __global const float* restrict last, const float val) {
//	__global const float *it = first;
//
//	while ((it <= last) && (*it <= val)) {
//		it++;
//	}
//
//	return it;
//}

OPENCL_FORCE_INLINE float Distribution1D_SampleContinuous(__global const float* restrict distribution1D, const float u,
		float *pdf, uint *off) {
	const uint count = as_uint(distribution1D[0]);
	__global const float* restrict func = &distribution1D[1];
	__global const float* restrict cdf = &distribution1D[1 + count];

	// Find surrounding CDF segments and offset
	if (u <= cdf[0]) {
		*pdf = func[0];
		if (off)
			*off = 0;
		return 0.f;
	}
	if (u >= cdf[count]) {
		*pdf = func[count - 1];
		if (off)
			*off = count - 1;
		return 1.f;
	}

	__global const float* restrict ptr = std_upper_bound(cdf, cdf + count + 1, u);
	const uint offset = ptr - cdf - 1;

	// Compute offset along CDF segment
	const float du = (u - cdf[offset]) /
			(cdf[offset + 1] - cdf[offset]);

	// Compute PDF for sampled offset
	*pdf = func[offset];

	// Save offset
	if (off)
		*off = offset;

	return (offset + du) / count;
	
	// Return x in [0,1) corresponding to sample
	//
	// Note: if du is very near to 1 than offset + du = offset + 1 and this
	// causes a lot of problems because the Pdf((offset + du) * invCount) will be
	// different from the Pdf returned here.
	// So I use this Min() as work around.
	const float result = fmin((offset + du) / count, MachineEpsilon_PreviousFloat(((offset + 1) / count)));
	
	assert (*pdf == Pdf(result));

	return result;
}

OPENCL_FORCE_INLINE uint Distribution1D_SampleDiscreteExt(__global const float *distribution1D,
		const float u, float *pdf, float *du) {
	const uint count = as_uint(distribution1D[0]);
	__global const float* restrict func = &distribution1D[1];
	__global const float* restrict cdf = &distribution1D[1 + count];

	// Find surrounding CDF segments and offset
	if (u <= cdf[0]) {
		*pdf = func[0] / count;
		return 0;
	}
	if (u >= cdf[count]) {
		*pdf = func[count - 1] / count;
		return count - 1;
	}

	__global const float* restrict ptr = std_upper_bound(cdf, cdf + count + 1, u);
	const uint offset = ptr - cdf - 1;

	// Compute offset along CDF segment
	if (du)
		*du = (u - cdf[offset]) /
			(cdf[offset + 1] - cdf[offset]);

	// Compute PDF for sampled offset
	*pdf = func[offset] / count;

	return offset;
}

OPENCL_FORCE_INLINE uint Distribution1D_SampleDiscrete(__global const float *distribution1D,
		const float u, float *pdf) {
	return Distribution1D_SampleDiscreteExt(distribution1D, u, pdf, NULL);
}

OPENCL_FORCE_INLINE uint Distribution1D_Offset(__global const float* restrict distribution1D, const float u) {
	const uint count = as_uint(distribution1D[0]);

	return min(count - 1, Floor2UInt(u * count));
}

OPENCL_FORCE_INLINE float Distribution1D_PdfDiscrete(__global const float* restrict distribution1D, const uint offset) {
	const uint count = as_uint(distribution1D[0]);
	__global const float* restrict func = &distribution1D[1];

	return func[offset] / count;
}

OPENCL_FORCE_INLINE float Distribution1D_Pdf(__global const float* restrict distribution1D,
		const float u, float *du) {
	const uint count = as_uint(distribution1D[0]);
	__global const float* restrict func = &distribution1D[1];
	__global const float* restrict cdf = &distribution1D[1 + count];
	
	const uint offset = Distribution1D_Offset(distribution1D, u);

	if (du)
		*du = u * count - cdf[offset];
	
	return func[offset];
}

//------------------------------------------------------------------------------
// Distribution2D
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE void Distribution2D_SampleContinuous(__global const float* restrict distribution2D,
		const float u0, const float u1, float2 *uv, float *pdf) {
	const uint width = as_uint(distribution2D[0]);
	const uint height = as_uint(distribution2D[1]);
	__global const float* restrict marginal = &distribution2D[2];
	// size of Distribution1D: size of counts + size of func + size of cdf
	const uint marginalSize = 1 + height + height + 1;
	// size of Distribution1D: size of counts + size of func + size of cdf
	const uint conditionalSize = 1 + width + width + 1;

	float pdf1;
	uint index;
	(*uv).s1 = Distribution1D_SampleContinuous(marginal, u1, &pdf1, &index);

	float pdf0;
	__global const float* restrict conditional = &distribution2D[2 + marginalSize + index * conditionalSize];
	(*uv).s0 = Distribution1D_SampleContinuous(conditional, u0, &pdf0, NULL);

	*pdf = pdf0 * pdf1;
}

OPENCL_FORCE_INLINE float Distribution2D_PdfExt(__global const float* restrict distribution2D,
		const float u, const float v,
		float *du, float *dv,
		uint *offsetU, uint *offsetV) {
	const uint width = as_uint(distribution2D[0]);
	const uint height = as_uint(distribution2D[1]);
	__global const float* restrict marginal = &distribution2D[2];
	// size of Distribution1D: size of counts + size of func + size of cdf
	const uint marginalSize = 1 + height + height + 1;
	// size of Distribution1D: size of counts + size of func + size of cdf
	const uint conditionalSize = 1 + width + width + 1;

	const uint index = Distribution1D_Offset(marginal, v);
	__global const float *conditional = &distribution2D[2 + marginalSize + index * conditionalSize];

	if (offsetV)
		*offsetV = index;
	if (offsetU)
		*offsetU = Distribution1D_Offset(conditional, u);
	
	return Distribution1D_Pdf(conditional, u, du) * Distribution1D_Pdf(marginal, v, dv);
}

OPENCL_FORCE_INLINE float Distribution2D_Pdf(__global const float* restrict distribution2D,
		const float u, const float v) {
	return Distribution2D_PdfExt(distribution2D, u, v, NULL, NULL, NULL, NULL);
}

OPENCL_FORCE_INLINE void Distribution2D_SampleDiscrete(__global const float* restrict distribution2D,
		float u0, float u1, uint uv[2], float *pdf,
		float *du0, float *du1) {
	const uint width = as_uint(distribution2D[0]);
	const uint height = as_uint(distribution2D[1]);
	__global const float* restrict marginal = &distribution2D[2];
	// size of Distribution1D: size of counts + size of func + size of cdf
	const uint marginalSize = 1 + height + height + 1;
	// size of Distribution1D: size of counts + size of func + size of cdf
	const uint conditionalSize = 1 + width + width + 1;

	float pdfs[2];
	uv[1] = Distribution1D_SampleDiscreteExt(marginal, u1, &pdfs[1], du1);
	__global const float *conditional = &distribution2D[2 + marginalSize + uv[1] * conditionalSize];

	uv[0] = Distribution1D_SampleDiscreteExt(conditional, u0, &pdfs[0], du0);

	*pdf = pdfs[0] * pdfs[1];
}

#line 2 "pathguiding_funcs.cl"

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

//------------------------------------------------------------------------------
// Path guiding (PATHOCL, path.guiding.enable)
//
// A GPU variant of "Practical path guiding" (Mueller et al. 2017) without a
// host side tree: a hash table of spatial cells (their size follows the
// distance from the camera, split by the dominant axis of the surface
// normal), each with a histogram of the incident radiance over
// PATHGUIDING_BIN_COUNT equal area direction bins.
//
// Training: the paths record their guidable vertices (PathGuidingRecord). At
// the splat, (radiance gathered after the vertex) / (throughput * pdf), an
// estimate of the incident radiance integrated over the bin, goes to the
// training histogram of the cell. PathGuiding_Rebuild turns the training
// histograms into the sampling CDFs at 2, 4, 8, ... samples per pixel, up to
// path.guiding.trainingspp, then the training stops.
//
// Sampling: a vertex whose cell has a CDF samples the BSDF with probability
// PATHGUIDING_BSDF_FRACTION and the cell histogram otherwise. The pdf of the
// mix is used everywhere, including the MIS weights of direct light
// sampling. Every direction the BSDF samples keeps a non zero pdf, so the
// estimator stays unbiased whatever the histograms are.
//------------------------------------------------------------------------------

#if defined(PARAM_PATH_GUIDING)

#define PATHGUIDING_BSDF_FRACTION .5f
// Probes of the open addressing hash table
#define PATHGUIDING_PROBE_COUNT 8u
// Training samples a cell needs to get a new sampling CDF at a rebuild
#define PATHGUIDING_MIN_TRAINING_SAMPLES 32u
// Glossy BSDFs sharper than this are not guided (as the hybrid back/forward
// glossiness threshold)
#define PATHGUIDING_MIN_GLOSSINESS .05f
// Solid angle pdf of a bin with probability 1
#define PATHGUIDING_BIN_PDF (PATHGUIDING_BIN_COUNT * (.25f * M_1_PI_F))

#define PATHGUIDING_CDF_PARAM_DECL , __global const float *guidingCDF
#define PATHGUIDING_CDF_PARAM , guidingCDF

//------------------------------------------------------------------------------
// Cells
//------------------------------------------------------------------------------

// MurmurHash3 finalizer
OPENCL_FORCE_INLINE uint PathGuiding_Mix(uint h) {
	h ^= h >> 16;
	h *= 0x85ebca6bu;
	h ^= h >> 13;
	h *= 0xc2b2ae35u;
	h ^= h >> 16;

	return h;
}

// n points to the side of the surface the path comes from
OPENCL_FORCE_INLINE uint PathGuiding_CellKey(__constant const GPUTaskConfiguration* restrict taskConfig,
		const float3 p, const float3 n) {
	// The cell size is a power of 2 close to the distance from the camera
	// times cellScale
	const float3 cameraPos = VLOAD3F(&taskConfig->pathTracer.guiding.cameraPos.x);
	const float distance = fmax(length(p - cameraPos), 1e-3f);
	const int level = (int)floor(log2(distance * taskConfig->pathTracer.guiding.cellScale));
	const float invCellSize = exp2(-(float)level);

	// The dominant axis of the normal and its sign: the two sides of a wall
	// are different cells
	const float3 absN = fabs(n);
	const uint axis = ((absN.x >= absN.y) && (absN.x >= absN.z)) ? 0u : ((absN.y >= absN.z) ? 1u : 2u);
	const float axisN = (axis == 0u) ? n.x : ((axis == 1u) ? n.y : n.z);
	const uint side = axis * 2u + ((axisN < 0.f) ? 1u : 0u);

	uint h = PathGuiding_Mix((uint)(int)floor(p.x * invCellSize));
	h = PathGuiding_Mix(h ^ (uint)(int)floor(p.y * invCellSize));
	h = PathGuiding_Mix(h ^ (uint)(int)floor(p.z * invCellSize));
	h = PathGuiding_Mix(h ^ (((uint)level << 3) | side));

	// 0 marks the empty slots
	return h ? h : 1u;
}

// Returns the cell with this key, NULL_INDEX if it is not in the table (and
// can not be inserted)
OPENCL_FORCE_INLINE uint PathGuiding_FindCell(__global uint *cellKeys, const uint key, const bool insert) {
	const uint start = key & (PATHGUIDING_CELL_COUNT - 1u);
	for (uint i = 0; i < PATHGUIDING_PROBE_COUNT; ++i) {
		const uint cell = (start + i) & (PATHGUIDING_CELL_COUNT - 1u);
		uint cellKey = cellKeys[cell];
		if (cellKey == 0u) {
			if (!insert)
				return NULL_INDEX;

			cellKey = atomic_cmpxchg(&cellKeys[cell], 0u, key);
			if (cellKey == 0u)
				return cell;
		}

		if (cellKey == key)
			return cell;
	}

	return NULL_INDEX;
}

OPENCL_FORCE_INLINE bool PathGuiding_IsGuidable(__global const BSDF *bsdf
		MATERIALS_PARAM_DECL) {
	if (bsdf->isVolume)
		return false;

	// The Disney Evaluate() returns f * cos^2 and Sample() f * cos (PLAN
	// T4.5): a guided sample would not estimate the BSDF sampling integral
	if (mats[bsdf->materialIndex].type == DISNEY)
		return false;

	const BSDFEvent events = BSDF_GetEventTypes(bsdf MATERIALS_PARAM);
	if (events & SPECULAR)
		return false;
	if (events & DIFFUSE)
		return true;

	return (events & GLOSSY) && (BSDF_GetGlossiness(bsdf MATERIALS_PARAM) >= PATHGUIDING_MIN_GLOSSINESS);
}

// The cell of a path vertex, NULL_INDEX if the vertex is not guidable
OPENCL_FORCE_INLINE uint PathGuiding_GetCell(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global const BSDF *bsdf, __global uint *cellKeys, const bool insert
		MATERIALS_PARAM_DECL) {
	if (!PathGuiding_IsGuidable(bsdf MATERIALS_PARAM))
		return NULL_INDEX;

	const float3 p = VLOAD3F(&bsdf->hitPoint.p.x);
	float3 n = VLOAD3F(&bsdf->hitPoint.geometryN.x);
	if (dot(n, VLOAD3F(&bsdf->hitPoint.fixedDir.x)) < 0.f)
		n = -n;

	return PathGuiding_FindCell(cellKeys, PathGuiding_CellKey(taskConfig, p, n), insert);
}

// The cell of a path vertex if MK_GENERATE_NEXT_VERTEX_RAY samples the BSDF
// there, NULL_INDEX otherwise. MK_DL_SAMPLE_BSDF and
// MK_GENERATE_NEXT_VERTEX_RAY must agree on the guided vertices.
OPENCL_FORCE_INLINE uint PathGuiding_GetVertexCell(__constant const GPUTaskConfiguration* restrict taskConfig,
		__global const BSDF *bsdf, __global const SampleResult *sampleResult,
		__global const EyePathInfo *pathInfo, __global uint *cellKeys, const bool insert
		MATERIALS_PARAM_DECL) {
	// Not for the shadow catcher, and not when the path goes on through
	// shadow transparency (see MK_GENERATE_NEXT_VERTEX_RAY)
	if (BSDF_IsShadowCatcher(bsdf MATERIALS_PARAM) ||
			(!sampleResult->firstPathVertex && !pathInfo->isNearlyS &&
				!Spectrum_IsBlack(BSDF_GetPassThroughShadowTransparency(bsdf MATERIALS_PARAM))))
		return NULL_INDEX;

	return PathGuiding_GetCell(taskConfig, bsdf, cellKeys, insert
			MATERIALS_PARAM);
}

// The sampling CDF of a cell in a CDF buffer, NULL if the vertex is not
// guided
OPENCL_FORCE_INLINE __global const float *PathGuiding_GetCellCDF(__global const float *cdfs,
		const uint cdfBuffer, const uint cell) {
	if (cell == NULL_INDEX)
		return NULL;

	// The last entry is 1 for a cell with a CDF and 0 otherwise
	__global const float *cdf = &cdfs[(cdfBuffer * PATHGUIDING_CELL_COUNT + cell) * PATHGUIDING_BIN_COUNT];
	return (cdf[PATHGUIDING_BIN_COUNT - 1u] > 0.f) ? cdf : NULL;
}

//------------------------------------------------------------------------------
// Directions: equal area bins, cos(theta) x phi in world space
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE uint PathGuiding_DirToBin(const float3 dir) {
	const float cosTheta = clamp(dir.z, -1.f, 1.f);
	float phi = atan2(dir.y, dir.x);
	if (phi < 0.f)
		phi += 2.f * M_PI_F;

	const uint i = min((uint)((cosTheta + 1.f) * (.5f * PATHGUIDING_DIR_RES)), PATHGUIDING_DIR_RES - 1u);
	const uint j = min((uint)(phi * (.5f * M_1_PI_F * PATHGUIDING_DIR_RES)), PATHGUIDING_DIR_RES - 1u);

	return i * PATHGUIDING_DIR_RES + j;
}

OPENCL_FORCE_INLINE float PathGuiding_Pdf(__global const float *cdf, const float3 dir) {
	const uint bin = PathGuiding_DirToBin(dir);
	const float binProbability = cdf[bin] - ((bin > 0u) ? cdf[bin - 1u] : 0.f);

	return binProbability * PATHGUIDING_BIN_PDF;
}

OPENCL_FORCE_INLINE float3 PathGuiding_Sample(__global const float *cdf, const float u0, const float u1) {
	// The first bin with a CDF value greater than u0
	uint first = 0u;
	uint last = PATHGUIDING_BIN_COUNT - 1u;
	while (first < last) {
		const uint middle = (first + last) >> 1;
		if (cdf[middle] <= u0)
			first = middle + 1u;
		else
			last = middle;
	}

	const float cdfStart = (first > 0u) ? cdf[first - 1u] : 0.f;
	const float du = clamp((u0 - cdfStart) / (cdf[first] - cdfStart), 0.f, 1.f);

	const float cosTheta = -1.f + ((first / PATHGUIDING_DIR_RES) + du) * (2.f / PATHGUIDING_DIR_RES);
	const float phi = ((first % PATHGUIDING_DIR_RES) + u1) * (2.f * M_PI_F / PATHGUIDING_DIR_RES);
	const float sinTheta = sqrt(fmax(0.f, 1.f - cosTheta * cosTheta));

	return MAKE_FLOAT3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

//------------------------------------------------------------------------------
// Sampling
//------------------------------------------------------------------------------

// The directions BSDF_Sample() can return (in the shading frame): not at
// glancing angles, on the side of fixedDir for a reflection and on the other
// side for a transmission
OPENCL_FORCE_INLINE bool PathGuiding_IsBSDFSampleDir(__global const BSDF *bsdf,
		const float3 localFixedDir, const float3 localSampledDir
		MATERIALS_PARAM_DECL) {
	if ((fabs(CosTheta(localFixedDir)) < DEFAULT_COS_EPSILON_STATIC) ||
			(fabs(CosTheta(localSampledDir)) < DEFAULT_COS_EPSILON_STATIC))
		return false;

	const BSDFEvent events = BSDF_GetEventTypes(bsdf MATERIALS_PARAM);
	return (CosTheta(localFixedDir) * CosTheta(localSampledDir) > 0.f) ?
		(events & REFLECT) : (events & TRANSMIT);
}

// The directions BSDF_Evaluate() (direct light sampling) does not reject
// with its geometric and interpolated normal tests (the same tests)
OPENCL_FORCE_INLINE bool PathGuiding_IsBSDFEvaluateDir(__global const BSDF *bsdf,
		const float3 lightDir
		MATERIALS_PARAM_DECL) {
	const float3 eyeDir = VLOAD3F(&bsdf->hitPoint.fixedDir.x);
	const float3 geometryN = VLOAD3F(&bsdf->hitPoint.geometryN.x);
	const float3 interpolatedN = VLOAD3F(&bsdf->hitPoint.interpolatedN.x);

	const float dotLightDirNG = dot(lightDir, geometryN);
	const float dotEyeDirNG = dot(eyeDir, geometryN);
	if ((fabs(dotLightDirNG) < DEFAULT_COS_EPSILON_STATIC) ||
			(fabs(dotEyeDirNG) < DEFAULT_COS_EPSILON_STATIC))
		return false;

	const BSDFEvent events = BSDF_GetEventTypes(bsdf MATERIALS_PARAM);
	const float sideTest = dotEyeDirNG * dotLightDirNG;
	if (((sideTest > 0.f) && !(events & REFLECT)) ||
			((sideTest < 0.f) && !(events & TRANSMIT)))
		return false;

	const float sideTestIS = dot(eyeDir, interpolatedN) * dot(lightDir, interpolatedN);
	return !(((sideTestIS > 0.f) && !(events & REFLECT)) ||
			((sideTestIS < 0.f) && !(events & TRANSMIT)));
}

// The pdf of dir in the MIS weights between direct light sampling and BSDF
// sampling at a guided vertex, from the BSDF pdf bsdfPdfW: the pdf of the
// mix where both BSDF_Sample() (and the guide) and BSDF_Evaluate() reach
// dir. Elsewhere one of the two techniques never adds anything and the BSDF
// pdf is kept, as without guiding.
OPENCL_FORCE_INLINE float PathGuiding_MISPdf(__global const BSDF *bsdf, __global const float *cdf,
		const float3 dir, const float bsdfPdfW
		MATERIALS_PARAM_DECL) {
	__global const Frame *frame = &bsdf->frame;
	if (!PathGuiding_IsBSDFSampleDir(bsdf, Frame_ToLocal(frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x)),
				Frame_ToLocal(frame, dir)
				MATERIALS_PARAM) ||
			!PathGuiding_IsBSDFEvaluateDir(bsdf, dir
				MATERIALS_PARAM))
		return bsdfPdfW;

	return PATHGUIDING_BSDF_FRACTION * bsdfPdfW +
			(1.f - PATHGUIDING_BSDF_FRACTION) * PathGuiding_Pdf(cdf, dir);
}

// f * |cos| of the BSDF_Sample() estimator for sampledDir (the eye path
// value, and the pdf of BSDF_Sample() in *pdfW). A guided sample must
// estimate the same integral as a BSDF sample, and BSDF_Evaluate() is a
// different function: it rejects some directions BSDF_Sample() returns (the
// geometric and interpolated normal tests) and accepts some it never returns
// (Matte, Glossy2, Disney use |cos(sampledDir)| on both sides of the shading
// normal). This is Material_Evaluate() on the BSDF_Sample() directions, with
// the shadow terminator factor BSDF_Sample() applies.
OPENCL_FORCE_INLINE float3 PathGuiding_EvaluateBSDF(__global const BSDF *bsdf,
		const float3 sampledDir, BSDFEvent *event, float *pdfW
		MATERIALS_PARAM_DECL) {
	__global const Frame *frame = &bsdf->frame;
	const float3 localFixedDir = Frame_ToLocal(frame, VLOAD3F(&bsdf->hitPoint.fixedDir.x));
	const float3 localSampledDir = Frame_ToLocal(frame, sampledDir);
	if (!PathGuiding_IsBSDFSampleDir(bsdf, localFixedDir, localSampledDir
			MATERIALS_PARAM))
		return BLACK;

	float3 result = Material_Evaluate(bsdf->materialIndex, &bsdf->hitPoint,
			localSampledDir, localFixedDir, event, pdfW
			MATERIALS_PARAM);
	if (Spectrum_IsBlack(result))
		return BLACK;

	const float3 shadeN = VLOAD3F(&bsdf->hitPoint.shadeN.x);
	const float3 interpolatedN = VLOAD3F(&bsdf->hitPoint.interpolatedN.x);
	if ((*event & REFLECT) &&
			(*event & (DIFFUSE | GLOSSY)) &&
			((shadeN.x != interpolatedN.x) || (shadeN.y != interpolatedN.y) || (shadeN.z != interpolatedN.z)))
		result *= BSDF_ShadowTerminatorAvoidanceFactor(BSDF_GetLandingInterpolatedN(bsdf),
				BSDF_GetLandingShadeN(bsdf), sampledDir);

	return result;
}

// As BSDF_Sample() for a guided vertex: samples the BSDF with probability
// PATHGUIDING_BSDF_FRACTION and the cell CDF otherwise. Returns
// f * |cos| / pdf of the mix, and the pdf for the MIS weights (see
// PathGuiding_MISPdf()) in *pdfW.
OPENCL_FORCE_INLINE float3 PathGuiding_SampleBSDF(__global const BSDF *bsdf, __global const float *cdf,
		const float u0, const float u1,
		float3 *sampledDir, float *pdfW, float *absCosSampledDir, BSDFEvent *event
		MATERIALS_PARAM_DECL) {
	float bsdfPdfW;
	// f * |cos|
	float3 f;
	*event = NONE;
	if (u0 < PATHGUIDING_BSDF_FRACTION) {
		const float3 bsdfSample = BSDF_Sample(bsdf, u0 * (1.f / PATHGUIDING_BSDF_FRACTION), u1,
				sampledDir, &bsdfPdfW, absCosSampledDir, event
				MATERIALS_PARAM);
		if (Spectrum_IsBlack(bsdfSample))
			return BLACK;

		f = bsdfSample * bsdfPdfW;
	} else {
		*sampledDir = PathGuiding_Sample(cdf,
				(u0 - PATHGUIDING_BSDF_FRACTION) * (1.f / (1.f - PATHGUIDING_BSDF_FRACTION)), u1);
		f = PathGuiding_EvaluateBSDF(bsdf, *sampledDir, event, &bsdfPdfW
				MATERIALS_PARAM);
		if (Spectrum_IsBlack(f))
			return BLACK;

		*absCosSampledDir = fabs(dot(*sampledDir, VLOAD3F(&bsdf->hitPoint.shadeN.x)));
	}

	const float mixPdfW = PATHGUIDING_BSDF_FRACTION * bsdfPdfW +
			(1.f - PATHGUIDING_BSDF_FRACTION) * PathGuiding_Pdf(cdf, *sampledDir);
	*pdfW = PathGuiding_MISPdf(bsdf, cdf, *sampledDir, bsdfPdfW
			MATERIALS_PARAM);

	return f / mixPdfW;
}

//------------------------------------------------------------------------------
// Training
//------------------------------------------------------------------------------

// Luminance of the radiance a path has gathered so far
OPENCL_FORCE_INLINE float PathGuiding_RadianceY(__constant const Film* restrict film,
		__global const SampleResult *sampleResult) {
	float radianceY = 0.f;
	for (uint i = 0; i < film->radianceGroupCount; ++i)
		radianceY += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[i].c));

	return radianceY;
}

// throughput is the path throughput after the vertex, pdfW the pdf of dir
OPENCL_FORCE_INLINE void PathGuiding_Record(__global PathGuidingRecord *records,
		__global PathGuidingPathState *pathState, const uint gid,
		const uint cell, const float3 dir, const float radianceY,
		const float3 throughput, const float pdfW) {
	const uint count = pathState->recordCount;
	const float throughputY = Spectrum_Y(throughput);
	if ((count >= PATHGUIDING_MAX_RECORDS) || !(throughputY > 0.f) || !(pdfW > 0.f))
		return;

	__global PathGuidingRecord *record = &records[gid * PATHGUIDING_MAX_RECORDS + count];
	record->cellBin = cell * PATHGUIDING_BIN_COUNT + PathGuiding_DirToBin(dir);
	record->radiance = radianceY;
	record->scale = 1.f / (throughputY * pdfW);

	pathState->recordCount = count + 1;
}

// At the splat: adds the recorded vertices of the path to the training
// histograms
OPENCL_FORCE_INLINE void PathGuiding_Train(__global const PathGuidingRecord *records,
		__global PathGuidingPathState *pathState, __global float *trainBins, __global uint *trainCounts,
		const uint gid, const float radianceY) {
	const uint count = pathState->recordCount;
	for (uint i = 0; i < count; ++i) {
		__global const PathGuidingRecord *record = &records[gid * PATHGUIDING_MAX_RECORDS + i];
		const float value = (radianceY - record->radiance) * record->scale;
		if ((value > 0.f) && (value < INFINITY))
			AtomicAdd(&trainBins[record->cellBin], value);
		atomic_inc(&trainCounts[record->cellBin / PATHGUIDING_BIN_COUNT]);
	}

	pathState->recordCount = 0;
}

// One thread per cell: writes the CDFs of CDF buffer cdfBuffer. A cell with
// enough training samples gets a new CDF, the others keep the one of the
// other buffer. The training histograms are cleared. The paths switch to
// cdfBuffer at their next MK_HIT_OBJECT: the paths still on the other buffer
// (between MK_HIT_OBJECT and MK_GENERATE_NEXT_VERTEX_RAY, a few iterations)
// must be done before the next rebuild, which writes it.
__kernel void PathGuiding_Rebuild(__global float *trainBins, __global uint *trainCounts,
		__global float *cdfs, __global uint *guidingState,
		const uint cdfBuffer, const uint training) {
	const size_t cell = get_global_id(0);
	if (cell >= PATHGUIDING_CELL_COUNT)
		return;

	if (cell == 0) {
		guidingState[PATHGUIDING_STATE_TRAINING] = training;
		guidingState[PATHGUIDING_STATE_CDF_BUFFER] = cdfBuffer;
	}

	__global float *bins = &trainBins[cell * PATHGUIDING_BIN_COUNT];
	__global float *cdf = &cdfs[(cdfBuffer * PATHGUIDING_CELL_COUNT + cell) * PATHGUIDING_BIN_COUNT];
	__global const float *oldCDF = &cdfs[((cdfBuffer ^ 1u) * PATHGUIDING_CELL_COUNT + cell) * PATHGUIDING_BIN_COUNT];

	bool rebuilt = false;
	if (trainCounts[cell] >= PATHGUIDING_MIN_TRAINING_SAMPLES) {
		float sum = 0.f;
		for (uint i = 0; i < PATHGUIDING_BIN_COUNT; ++i)
			sum += bins[i];

		if (sum > 0.f) {
			// A floor of 1% of the average keeps every bin reachable
			const float floorValue = .01f * sum / PATHGUIDING_BIN_COUNT;
			const float invTotal = 1.f / (sum + floorValue * PATHGUIDING_BIN_COUNT);

			float c = 0.f;
			for (uint i = 0; i < PATHGUIDING_BIN_COUNT - 1u; ++i) {
				c += (bins[i] + floorValue) * invTotal;
				cdf[i] = fmin(c, 1.f);
			}
			cdf[PATHGUIDING_BIN_COUNT - 1u] = 1.f;
			rebuilt = true;
		}
	}

	if (!rebuilt) {
		for (uint i = 0; i < PATHGUIDING_BIN_COUNT; ++i)
			cdf[i] = oldCDF[i];
	}

	for (uint i = 0; i < PATHGUIDING_BIN_COUNT; ++i)
		bins[i] = 0.f;
	trainCounts[cell] = 0u;
}

#else

#define PATHGUIDING_CDF_PARAM_DECL
#define PATHGUIDING_CDF_PARAM

#endif

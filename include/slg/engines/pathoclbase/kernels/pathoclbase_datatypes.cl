#line 2 "pathoclstatebase_datatypes.cl"

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
// Some OpenCL specific definition
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif

//------------------------------------------------------------------------------
// GPUTask data types
//------------------------------------------------------------------------------

typedef enum {
	// Micro-kernel states
	MK_RT_NEXT_VERTEX = 0,
	MK_HIT_NOTHING = 1,
	MK_HIT_OBJECT = 2,
	MK_DL_ILLUMINATE = 3,
	MK_DL_SAMPLE_BSDF = 4,
	MK_RT_DL = 5,
	MK_GENERATE_NEXT_VERTEX_RAY = 6,
	MK_SPLAT_SAMPLE = 7,
	MK_NEXT_SAMPLE = 8,
	MK_GENERATE_CAMERA_RAY = 9,
	MK_DONE = 10
} PathState;

// Work queues: each micro-kernel runs only on the paths queued for it. The
// QueuePaths kernel fills the queue of a state right before its micro-kernel
// and all queues are reset at the end of an iteration.
typedef enum {
	PATH_QUEUE_HIT_NOTHING = 0,
	PATH_QUEUE_HIT_OBJECT = 1,
	PATH_QUEUE_DL_ILLUMINATE = 2,
	PATH_QUEUE_DL_SAMPLE_BSDF = 3,
	PATH_QUEUE_GENERATE_NEXT_VERTEX_RAY = 4,
	PATH_QUEUE_SPLAT_SAMPLE = 5,
	PATH_QUEUE_NEXT_SAMPLE = 6,
	PATH_QUEUE_GENERATE_CAMERA_RAY = 7,
	PATH_QUEUE_RT_NEXT_VERTEX = 8,
	PATH_QUEUE_RT_DL = 9,
	PATH_QUEUE_COUNT = 10
} PathQueue;

typedef struct {
	union {
		struct {
			// Must be a power of 2
			unsigned int previewResolutionReduction, previewResolutionReductionStep;
			unsigned int resolutionReduction;
		} rtpathocl;
	} renderEngine;

	Scene scene;
	Sampler sampler;
	PathTracer pathTracer;
	Filter pixelFilter;
	Film film;
} GPUTaskConfiguration;

typedef struct {
	unsigned int lightIndex;
	float pickPdf;

	float directPdfW;

	// Radiance to add to the result if light source is visible
	// Note: it doesn't include the pathThroughput
	Spectrum lightRadiance;
	// This is used only if Film channel IRRADIANCE is enabled and
	// only for the first path vertex
	Spectrum lightIrradiance;

	unsigned int lightID;
} DirectLightIlluminateInfo;

// The state used to keep track of the rendered path. The PathState itself
// is kept in a separate array (pathStates) so the state checks done by every
// micro-kernel read contiguous memory.
typedef struct {
	Spectrum throughput;
	BSDF bsdf; // Variable size structure

	Seed seedPassThroughEvent;
	
	int albedoToDo, photonGICacheEnabledOnLastHit,
			photonGICausticCacheUsed, photonGIShowIndirectPathMixUsed,
			// The shadow transparency lag used by Scene_Intersect()
			throughShadowTransparency;
} GPUTaskState;

typedef enum {
	ILLUMINATED, SHADOWED, NOT_VISIBLE
} DirectLightResult;

typedef struct {
	// Used to store some intermediate result
	DirectLightIlluminateInfo illumInfo;

	Seed seedPassThroughEvent;

	DirectLightResult directLightResult;

	// The shadow transparency flag used by Scene_Intersect()
	int throughShadowTransparency;
} GPUTaskDirectLight;

typedef struct {
	// The task seed
	Seed seed;

	// Space for temporary storage
	BSDF tmpBsdf; // Variable size structure

	// This is used by TriangleLight_Illuminate() to temporary store the
	// point on the light sources.
	// Also used by Scene_Intersect() for evaluating volume textures.
	HitPoint tmpHitPoint;
	
	// This is used by DirectLight_BSDFSampling()
	PathDepthInfo tmpPathDepthInfo;
} GPUTask;

typedef struct {
	unsigned int sampleCount;
} GPUTaskStats;

//------------------------------------------------------------------------------
// Light tracing data types
//
// With the hybrid back/forward path tracing, the eye paths skip the caustic
// paths and the light paths add them: the light path vertices reached
// through nearly specular vertices are connected to the camera (as
// PathTracer::RenderLightSample()). Each light task has one ray in flight,
// stored in the ray buffer after the eye path rays.
//------------------------------------------------------------------------------

typedef enum {
	// Start a new light path (no ray in flight)
	LT_GENERATE = 0,
	// The ray to the next light path vertex
	LT_RT_NEXT_VERTEX = 1,
	// The ray connecting a light path vertex to the camera
	LT_RT_EYE_CONNECTION = 2
} LightPathState;

typedef struct {
	Seed seed;
	LightPathState state;

	unsigned int lightID;
	float time;
	// The lens point the vertices are connected to
	Point lensPoint;
	Spectrum flux;

	PathDepthInfo depth;
	PathVolumeInfo volume;
	BSDFEvent lastBSDFEvent;
	// LightPathInfo::IsSpecularPath()
	int isNearlyS;

	// The ray to trace after the eye connection (as Ray::Update())
	Point nextVertexRayOrig;
	Vector nextVertexRayDir;
	int hasNextVertexRay;

	// The eye connection being traced
	Spectrum connectionRadiance;
	float connectionFilmX, connectionFilmY;
	PathVolumeInfo connectionVolume;
} LightPathTask;
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

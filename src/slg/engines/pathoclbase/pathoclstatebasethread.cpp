/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclstatebase.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

#define SOBOL_MAXDEPTH 6

//------------------------------------------------------------------------------
// PathOCLStateKernelBaseRenderThread
//------------------------------------------------------------------------------

PathOCLStateKernelBaseRenderThread::PathOCLStateKernelBaseRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLStateKernelBaseRenderEngine *re) :
		PathOCLBaseRenderThread(index, device, re) {
	// OpenCL kernels
	initKernel = NULL;
	advancePathsKernel_MK_RT_NEXT_VERTEX = NULL;
	advancePathsKernel_MK_HIT_NOTHING = NULL;
	advancePathsKernel_MK_HIT_OBJECT = NULL;
	advancePathsKernel_MK_RT_DL = NULL;
	advancePathsKernel_MK_DL_ILLUMINATE = NULL;
	advancePathsKernel_MK_DL_SAMPLE_BSDF = NULL;
	advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY = NULL;
	advancePathsKernel_MK_SPLAT_SAMPLE = NULL;
	advancePathsKernel_MK_NEXT_SAMPLE = NULL;
	advancePathsKernel_MK_GENERATE_CAMERA_RAY = NULL;

	// OpenCL memory buffers
	raysBuff = NULL;
	hitsBuff = NULL;
	tasksBuff = NULL;
	tasksDirectLightBuff = NULL;
	tasksStateBuff = NULL;
	samplesBuff = NULL;
	sampleDataBuff = NULL;
	taskStatsBuff = NULL;
	pathVolInfosBuff = NULL;
	directLightVolInfosBuff = NULL;
	pixelFilterBuff = NULL;

	gpuTaskStats = NULL;
}

PathOCLStateKernelBaseRenderThread::~PathOCLStateKernelBaseRenderThread() {
	delete initKernel;
	delete advancePathsKernel_MK_RT_NEXT_VERTEX;
	delete advancePathsKernel_MK_HIT_NOTHING;
	delete advancePathsKernel_MK_HIT_OBJECT;
	delete advancePathsKernel_MK_RT_DL;
	delete advancePathsKernel_MK_DL_ILLUMINATE;
	delete advancePathsKernel_MK_DL_SAMPLE_BSDF;
	delete advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY;
	delete advancePathsKernel_MK_SPLAT_SAMPLE;
	delete advancePathsKernel_MK_NEXT_SAMPLE;
	delete advancePathsKernel_MK_GENERATE_CAMERA_RAY;

	delete[] gpuTaskStats;
}

string PathOCLStateKernelBaseRenderThread::AdditionalKernelOptions() {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_MAX_PATH_DEPTH=" << engine->maxPathDepth.depth <<
			" -D PARAM_MAX_PATH_DEPTH_DIFFUSE=" << engine->maxPathDepth.diffuseDepth <<
			" -D PARAM_MAX_PATH_DEPTH_GLOSSY=" << engine->maxPathDepth.glossyDepth <<
			" -D PARAM_MAX_PATH_DEPTH_SPECULAR=" << engine->maxPathDepth.specularDepth <<
			" -D PARAM_RR_DEPTH=" << engine->rrDepth <<
			" -D PARAM_RR_CAP=" << engine->rrImportanceCap << "f" <<
			" -D PARAM_SQRT_VARIANCE_CLAMP_MAX_VALUE=" << engine->sqrtVarianceClampMaxValue << "f" <<
			" -D PARAM_PDF_CLAMP_VALUE=" << engine->pdfClampValue << "f"
			;

	const slg::ocl::Filter *filter = engine->oclPixelFilter;
	switch (filter->type) {
		case slg::ocl::FILTER_NONE:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=0"
					" -D PARAM_IMAGE_FILTER_WIDTH_X=.5f"
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=.5f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=0" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=0";
			break;
		case slg::ocl::FILTER_BOX:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=1" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->box.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->box.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->box.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->box.widthY * .5f + .5f);
			break;
		case slg::ocl::FILTER_GAUSSIAN:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=2" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->gaussian.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->gaussian.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->gaussian.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->gaussian.widthY * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_GAUSSIAN_ALPHA=" << filter->gaussian.alpha << "f";
			break;
		case slg::ocl::FILTER_MITCHELL:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=3" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->mitchell.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->mitchell.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->mitchell.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->mitchell.widthY * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << filter->mitchell.B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << filter->mitchell.C << "f";
			break;
		case slg::ocl::FILTER_MITCHELL_SS:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=4" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->mitchellss.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->mitchellss.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->mitchellss.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->mitchellss.widthY * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_B=" << filter->mitchellss.B << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_C=" << filter->mitchellss.C << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_A0=" << filter->mitchellss.a0 << "f" <<
					" -D PARAM_IMAGE_FILTER_MITCHELL_A1=" << filter->mitchellss.a1 << "f";
			break;
		case slg::ocl::FILTER_BLACKMANHARRIS:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=5" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_X=" << filter->blackmanharris.widthX << "f" <<
					" -D PARAM_IMAGE_FILTER_WIDTH_Y=" << filter->blackmanharris.widthY << "f" <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_X=" << Floor2Int(filter->blackmanharris.widthX * .5f + .5f) <<
					" -D PARAM_IMAGE_FILTER_PIXEL_WIDTH_Y=" << Floor2Int(filter->blackmanharris.widthY * .5f + .5f);
			break;
		default:
			throw runtime_error("Unknown pixel filter type: "  + boost::lexical_cast<string>(filter->type));
	}

	if (engine->useFastPixelFilter && (filter->type != slg::ocl::FILTER_NONE))
		ss << " -D PARAM_USE_FAST_PIXEL_FILTER";

	if (engine->usePixelAtomics)
		ss << " -D PARAM_USE_PIXEL_ATOMICS";

	if (engine->forceBlackBackground)
		ss << " -D PARAM_FORCE_BLACK_BACKGROUND";

	const slg::ocl::Sampler *sampler = engine->oclSampler;
	switch (sampler->type) {
		case slg::ocl::RANDOM:
			ss << " -D PARAM_SAMPLER_TYPE=0";
			break;
		case slg::ocl::METROPOLIS:
			ss << " -D PARAM_SAMPLER_TYPE=1" <<
					" -D PARAM_SAMPLER_METROPOLIS_LARGE_STEP_RATE=" << sampler->metropolis.largeMutationProbability << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_IMAGE_MUTATION_RANGE=" << sampler->metropolis.imageMutationRange << "f" <<
					" -D PARAM_SAMPLER_METROPOLIS_MAX_CONSECUTIVE_REJECT=" << sampler->metropolis.maxRejects;
			break;
		case slg::ocl::SOBOL: {
			RandomGenerator rndGen(engine->seedBase + threadIndex);
					
			ss << " -D PARAM_SAMPLER_TYPE=2" <<
					" -D PARAM_SAMPLER_SOBOL_RNG0=" << rndGen.floatValue() << "f" <<
					" -D PARAM_SAMPLER_SOBOL_RNG1=" << rndGen.floatValue() << "f" <<
					" -D PARAM_SAMPLER_SOBOL_STARTOFFSET=" << SOBOL_STARTOFFSET <<
					" -D PARAM_SAMPLER_SOBOL_MAXDEPTH=" << Max<u_int>(SOBOL_MAXDEPTH, engine->maxPathDepth.depth);
			break;
		}
		case slg::ocl::BIASPATHSAMPLER:
			ss << " -D PARAM_SAMPLER_TYPE=3";
			break;
		default:
			throw runtime_error("Unknown sampler type in PathOCLStateKernelBaseRenderThread::AdditionalKernelOptions(): " + boost::lexical_cast<string>(sampler->type));
	}
	
	return ss.str();
}

string PathOCLStateKernelBaseRenderThread::AdditionalKernelDefinitions() {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;

	if (engine->oclSampler->type == slg::ocl::SOBOL) {
		// Generate the Sobol vectors
		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Sobol table size: " << sampleDimensions * SOBOL_BITS);
		u_int *directions = new u_int[sampleDimensions * SOBOL_BITS];

		SobolGenerateDirectionVectors(directions, sampleDimensions);

		stringstream sobolTableSS;
		sobolTableSS << "#line 2 \"Sobol Table in pathoclthreadstatebase.cpp\"\n";
		sobolTableSS << "__constant uint SOBOL_DIRECTIONS[" << sampleDimensions * SOBOL_BITS << "] = {\n";
		for (u_int i = 0; i < sampleDimensions * SOBOL_BITS; ++i) {
			if (i != 0)
				sobolTableSS << ", ";
			sobolTableSS << directions[i] << "u";
		}
		sobolTableSS << "};\n";

		delete[] directions;

		return sobolTableSS.str();
	} else
		return "";
}

string PathOCLStateKernelBaseRenderThread::AdditionalKernelSources() {
	stringstream ssKernel;
	ssKernel <<
			slg::ocl::KernelSource_pathoclstatebase_datatypes <<
			slg::ocl::KernelSource_pathoclstatebase_funcs <<
			slg::ocl::KernelSource_pathoclstatebase_kernels_micro;
	
	return ssKernel.str();
}

void PathOCLStateKernelBaseRenderThread::CompileAdditionalKernels(cl::Program *program) {
	//--------------------------------------------------------------------------
	// Init kernel
	//--------------------------------------------------------------------------

	CompileKernel(program, &initKernel, &initWorkGroupSize, "Init");

	//--------------------------------------------------------------------------
	// AdvancePaths kernel (Micro-Kernels)
	//--------------------------------------------------------------------------

	CompileKernel(program, &advancePathsKernel_MK_RT_NEXT_VERTEX, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_NEXT_VERTEX");
	CompileKernel(program, &advancePathsKernel_MK_HIT_NOTHING, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_HIT_NOTHING");
	CompileKernel(program, &advancePathsKernel_MK_HIT_OBJECT, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_HIT_OBJECT");
	CompileKernel(program, &advancePathsKernel_MK_RT_DL, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_RT_DL");
	CompileKernel(program, &advancePathsKernel_MK_DL_ILLUMINATE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_DL_ILLUMINATE");
	CompileKernel(program, &advancePathsKernel_MK_DL_SAMPLE_BSDF, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_DL_SAMPLE_BSDF");
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_GENERATE_NEXT_VERTEX_RAY");
	CompileKernel(program, &advancePathsKernel_MK_SPLAT_SAMPLE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_SPLAT_SAMPLE");
	CompileKernel(program, &advancePathsKernel_MK_NEXT_SAMPLE, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_NEXT_SAMPLE");
	CompileKernel(program, &advancePathsKernel_MK_GENERATE_CAMERA_RAY, &advancePathsWorkGroupSize,
			"AdvancePaths_MK_GENERATE_CAMERA_RAY");
}

void PathOCLStateKernelBaseRenderThread::InitGPUTaskBuffer() {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;
	const bool hasPassThrough = engine->compiledScene->RequiresPassThrough();
	const size_t openCLBSDFSize = GetOpenCLBSDFSize();

	//--------------------------------------------------------------------------
	// Allocate tasksBuff
	//--------------------------------------------------------------------------
	
	// Add Seed memory size
	size_t gpuTaskSize = sizeof(slg::ocl::Seed);

	// Add temporary storage space
	
	// Add tmpBsdf memory size
	if (hasPassThrough || engine->compiledScene->HasVolumes())
		gpuTaskSize += openCLBSDFSize;

	// Add tmpHitPoint memory size
	gpuTaskSize += GetOpenCLHitPointSize();

	SLG_LOG("[PathOCLStateKernelBaseRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaskSize << "bytes");
	AllocOCLBufferRW(&tasksBuff, gpuTaskSize * taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate tasksDirectLightBuff
	//--------------------------------------------------------------------------

	size_t gpuDirectLightTaskSize = 
			sizeof(slg::ocl::pathoclstatebase::DirectLightIlluminateInfo) + 
			sizeof(BSDFEvent) + 
			sizeof(float) +
			sizeof(int);

	// Add rayPassThroughEvent memory size
	if (hasPassThrough)
		gpuDirectLightTaskSize += sizeof(float);

	SLG_LOG("[PathOCLStateKernelBaseRenderThread::" << threadIndex << "] Size of a GPUTask DirectLight: " << gpuDirectLightTaskSize << "bytes");
	AllocOCLBufferRW(&tasksDirectLightBuff, gpuDirectLightTaskSize * taskCount, "GPUTaskDirectLight");

	//--------------------------------------------------------------------------
	// Allocate tasksStateBuff
	//--------------------------------------------------------------------------

	size_t gpuTaksStateSize =
			sizeof(int) + // state
			sizeof(slg::ocl::pathoclstatebase::PathDepthInfo) + // depthInfo
			sizeof(Spectrum);

	// Add BSDF memory size
	gpuTaksStateSize += openCLBSDFSize;

	SLG_LOG("[PathOCLStateKernelBaseRenderThread::" << threadIndex << "] Size of a GPUTask State: " << gpuTaksStateSize << "bytes");
	AllocOCLBufferRW(&tasksStateBuff, gpuTaksStateSize * taskCount, "GPUTaskState");
}

void PathOCLStateKernelBaseRenderThread::InitSamplesBuffer() {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	//--------------------------------------------------------------------------
	// SampleResult size
	//--------------------------------------------------------------------------

	const size_t sampleResultSize = GetOpenCLSampleResultSize() +
		// pixelX and pixelY fields
		((engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE)) ?
			sizeof(u_int) * 2 : 0);

	//--------------------------------------------------------------------------
	// Sample size
	//--------------------------------------------------------------------------
	size_t sampleSize = sampleResultSize;

	// Add Sample memory size
	if (engine->oclSampler->type == slg::ocl::RANDOM) {
		// Nothing to add
	} else if (engine->oclSampler->type == slg::ocl::METROPOLIS) {
		sampleSize += 2 * sizeof(float) + 5 * sizeof(u_int) + sampleResultSize;		
	} else if (engine->oclSampler->type == slg::ocl::SOBOL) {
		sampleSize += sizeof(u_int);
	} else if (engine->oclSampler->type == slg::ocl::BIASPATHSAMPLER) {
		// Nothing to add
	} else
		throw runtime_error("Unknown sampler.type in PathOCLStateKernelBaseRenderThread::InitSamplesBuffer(): " + boost::lexical_cast<string>(engine->oclSampler->type));

	SLG_LOG("[PathOCLStateKernelBaseRenderThread::" << threadIndex << "] Size of a Sample: " << sampleSize << "bytes");
	AllocOCLBufferRW(&samplesBuff, sampleSize * taskCount, "Sample");
}

void PathOCLStateKernelBaseRenderThread::InitSampleDataBuffer() {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;
	const bool hasPassThrough = engine->compiledScene->RequiresPassThrough();

	const size_t eyePathVertexDimension =
		// IDX_SCREEN_X, IDX_SCREEN_Y, IDX_EYE_TIME
		3 +
		// IDX_EYE_PASSTROUGHT
		(hasPassThrough ? 1 : 0) +
		// IDX_DOF_X, IDX_DOF_Y
		2;
	const size_t PerPathVertexDimension =
		// IDX_PASSTHROUGH,
		(hasPassThrough ? 1 : 0) +
		// IDX_BSDF_X, IDX_BSDF_Y
		2 +
		// IDX_DIRECTLIGHT_X, IDX_DIRECTLIGHT_Y, IDX_DIRECTLIGHT_Z, IDX_DIRECTLIGHT_W, IDX_DIRECTLIGHT_A
		4 + (hasPassThrough ? 1 : 0) +
		// IDX_RR
		1;
	sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * engine->maxPathDepth.depth;

	size_t uDataSize;
	if ((engine->oclSampler->type == slg::ocl::RANDOM) ||
			(engine->oclSampler->type == slg::ocl::SOBOL) ||
			(engine->oclSampler->type == slg::ocl::BIASPATHSAMPLER)) {
		// Only IDX_SCREEN_X, IDX_SCREEN_Y
		uDataSize = sizeof(float) * 2;
		
		if (engine->oclSampler->type == slg::ocl::SOBOL) {
			// Limit the number of dimensions where I use Sobol sequence (after,
			// I switch to Random sampler.
			sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * Max<u_int>(SOBOL_MAXDEPTH, engine->maxPathDepth.depth);
		}
	} else if (engine->oclSampler->type == slg::ocl::METROPOLIS) {
		// Metropolis needs 2 sets of samples, the current and the proposed mutation
		uDataSize = 2 * sizeof(float) * sampleDimensions;
	} else
		throw runtime_error("Unknown sampler.type in PathOCLStateKernelBaseRenderThread::InitSampleDataBuffer(): " + boost::lexical_cast<string>(engine->oclSampler->type));

	SLG_LOG("[PathOCLStateKernelBaseRenderThread::" << threadIndex << "] Sample dimensions: " << sampleDimensions);
	SLG_LOG("[PathOCLStateKernelBaseRenderThread::" << threadIndex << "] Size of a SampleData: " << uDataSize << "bytes");

	// TOFIX
	AllocOCLBufferRW(&sampleDataBuff, uDataSize * taskCount + 1, "SampleData"); // +1 to avoid METROPOLIS + Intel\AMD OpenCL crash
}

void PathOCLStateKernelBaseRenderThread::AdditionalInit() {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	// In case renderEngine->taskCount has changed
	delete[] gpuTaskStats;
	gpuTaskStats = new slg::ocl::pathoclstatebase::GPUTaskStats[taskCount];
	for (u_int i = 0; i < taskCount; ++i)
		gpuTaskStats[i].sampleCount = 0;

	//--------------------------------------------------------------------------
	// Allocate Ray/RayHit buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&raysBuff, sizeof(Ray) * taskCount, "Ray");
	AllocOCLBufferRW(&hitsBuff, sizeof(RayHit) * taskCount, "RayHit");

	//--------------------------------------------------------------------------
	// Allocate GPU task buffers
	//--------------------------------------------------------------------------

	InitGPUTaskBuffer();

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::pathoclstatebase::GPUTaskStats) * taskCount, "GPUTask Stats");

	//--------------------------------------------------------------------------
	// Allocate sample buffers
	//--------------------------------------------------------------------------

	InitSamplesBuffer();

	//--------------------------------------------------------------------------
	// Allocate sample data buffers
	//--------------------------------------------------------------------------

	InitSampleDataBuffer();

	//--------------------------------------------------------------------------
	// Allocate volume info buffers if required
	//--------------------------------------------------------------------------

	if (engine->compiledScene->HasVolumes()) {
		AllocOCLBufferRW(&pathVolInfosBuff, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "PathVolumeInfo");
		AllocOCLBufferRW(&directLightVolInfosBuff, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "DirectLightVolumeInfo");
	}

	//--------------------------------------------------------------------------
	// Allocate GPU pixel filter distribution
	//--------------------------------------------------------------------------

	if (engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE)) {
		AllocOCLBufferRO(&pixelFilterBuff, engine->pixelFilterDistribution,
				sizeof(float) * engine->pixelFilterDistributionSize, "Pixel Filter Distribution");
	}
}

void PathOCLStateKernelBaseRenderThread::SetInitKernelArgs(const u_int filmIndex,
		const u_int cameraFilmWidth, const u_int cameraFilmHeight,
		const u_int cameraFilmOffsetX, const u_int cameraFilmOffsetY) {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	CompiledScene *cscene = engine->compiledScene;

	u_int argIndex = 0;
	initKernel->setArg(argIndex++, threadFilms[filmIndex]->film->GetWidth());
	initKernel->setArg(argIndex++, threadFilms[filmIndex]->film->GetHeight());
	const u_int *filmSubRegion = threadFilms[filmIndex]->film->GetSubRegion();
	initKernel->setArg(argIndex++, filmSubRegion[0]);
	initKernel->setArg(argIndex++, filmSubRegion[1]);
	initKernel->setArg(argIndex++, filmSubRegion[2]);
	initKernel->setArg(argIndex++, filmSubRegion[3]);

	// Camera film size can be used for tile rendering in order to have a different size and sub region
	// between the raster film and the camera film
	initKernel->setArg(argIndex++, cameraFilmWidth);
	initKernel->setArg(argIndex++, cameraFilmHeight);
	initKernel->setArg(argIndex++, cameraFilmOffsetX);
	initKernel->setArg(argIndex++, cameraFilmOffsetY);

	initKernel->setArg(argIndex++, engine->seedBase + threadIndex * engine->taskCount);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksDirectLightBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksStateBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), taskStatsBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), samplesBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleDataBuff);
	if (cscene->HasVolumes())
		initKernel->setArg(argIndex++, sizeof(cl::Buffer), pathVolInfosBuff);
	if (engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE))
		initKernel->setArg(argIndex++, sizeof(cl::Buffer), pixelFilterBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), raysBuff);
	initKernel->setArg(argIndex++, sizeof(cl::Buffer), cameraBuff);
}

void PathOCLStateKernelBaseRenderThread::SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel, const u_int filmIndex) {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	CompiledScene *cscene = engine->compiledScene;

	u_int argIndex = 0;
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksDirectLightBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), tasksStateBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), taskStatsBuff);
	if (engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE))
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pixelFilterBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), samplesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), sampleDataBuff);
	if (cscene->HasVolumes()) {
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), pathVolInfosBuff);
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), directLightVolInfosBuff);
	}
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), raysBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), hitsBuff);

	// Film parameters
	argIndex = threadFilms[filmIndex]->SetFilmKernelArgs(*advancePathsKernel, argIndex);

	// Scene parameters
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
	advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), materialsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), texturesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), scnObjsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), meshDescsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), vertsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), normalsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), uvsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), colsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), alphasBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), trianglesBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), cameraBuff);
	// Lights
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), envLightIndicesBuff);
	advancePathsKernel->setArg(argIndex++, (u_int)cscene->envLightIndices.size());
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), meshTriLightDefsOffsetBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), infiniteLightDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), lightsDistributionBuff);
	advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), infiniteLightSourcesDistributionBuff);

	// Images
	if (imageMapDescsBuff) {
		advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), imageMapDescsBuff);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			advancePathsKernel->setArg(argIndex++, sizeof(cl::Buffer), (imageMapsBuff[i]));
	}
}

void PathOCLStateKernelBaseRenderThread::SetAllAdvancePathsKernelArgs(const u_int filmIndex) {
	if (advancePathsKernel_MK_RT_NEXT_VERTEX)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_NEXT_VERTEX, filmIndex);
	if (advancePathsKernel_MK_HIT_NOTHING)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_NOTHING, filmIndex);
	if (advancePathsKernel_MK_HIT_OBJECT)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_OBJECT, filmIndex);
	if (advancePathsKernel_MK_RT_DL)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_DL, filmIndex);
	if (advancePathsKernel_MK_DL_ILLUMINATE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_ILLUMINATE, filmIndex);
	if (advancePathsKernel_MK_DL_SAMPLE_BSDF)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_SAMPLE_BSDF, filmIndex);
	if (advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, filmIndex);
	if (advancePathsKernel_MK_SPLAT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_SPLAT_SAMPLE, filmIndex);
	if (advancePathsKernel_MK_NEXT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_NEXT_SAMPLE, filmIndex);
	if (advancePathsKernel_MK_GENERATE_CAMERA_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_CAMERA_RAY, filmIndex);
}

void PathOCLStateKernelBaseRenderThread::SetAdditionalKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only non thread safe function in OpenCL 1.1 so
	// I need to use a mutex here
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;

	boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

	//--------------------------------------------------------------------------
	// advancePathsKernels
	//--------------------------------------------------------------------------

	SetAllAdvancePathsKernelArgs(0);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	SetInitKernelArgs(0, threadFilms[0]->film->GetWidth(), threadFilms[0]->film->GetHeight(), 0, 0);
}

void PathOCLStateKernelBaseRenderThread::Stop() {
	PathOCLBaseRenderThread::Stop();

	FreeOCLBuffer(&raysBuff);
	FreeOCLBuffer(&hitsBuff);
	FreeOCLBuffer(&tasksBuff);
	FreeOCLBuffer(&tasksDirectLightBuff);
	FreeOCLBuffer(&tasksStateBuff);
	FreeOCLBuffer(&samplesBuff);
	FreeOCLBuffer(&sampleDataBuff);
	FreeOCLBuffer(&taskStatsBuff);
	FreeOCLBuffer(&pathVolInfosBuff);
	FreeOCLBuffer(&directLightVolInfosBuff);
	FreeOCLBuffer(&pixelFilterBuff);
}

void PathOCLStateKernelBaseRenderThread::EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue) {
	PathOCLStateKernelBaseRenderEngine *engine = (PathOCLStateKernelBaseRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	// Micro kernels version
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_NEXT_VERTEX, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_NOTHING, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_OBJECT, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_DL, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_ILLUMINATE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_SAMPLE_BSDF, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_SPLAT_SAMPLE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_NEXT_SAMPLE, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_CAMERA_RAY, cl::NullRange,
			cl::NDRange(taskCount), cl::NDRange(advancePathsWorkGroupSize));
}

#endif

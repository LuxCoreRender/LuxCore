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
#include "slg/engines/pathocl/pathocl.h"
#include "slg/engines/pathocl/pathocl_datatypes.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

#define SOBOL_MAXDEPTH 6

//------------------------------------------------------------------------------
// PathOCLRenderThread
//------------------------------------------------------------------------------

PathOCLRenderThread::PathOCLRenderThread(const u_int index,
		OpenCLIntersectionDevice *device, PathOCLRenderEngine *re) :
		PathOCLBaseRenderThread(index, device, re) {
	gpuTaskStats = NULL;

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
}

PathOCLRenderThread::~PathOCLRenderThread() {
	if (editMode)
		EndSceneEdit(EditActionList());
	if (started)
		Stop();

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

void PathOCLRenderThread::GetThreadFilmSize(u_int *filmWidth, u_int *filmHeight) {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const Film *engineFilm = engine->film;
	*filmWidth = engineFilm->GetWidth();
	*filmHeight = engineFilm->GetHeight();
}

string PathOCLRenderThread::AdditionalKernelOptions() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

	stringstream ss;
	ss.precision(6);
	ss << scientific <<
			" -D PARAM_TASK_COUNT=" << engine->taskCount <<
			" -D PARAM_MAX_PATH_DEPTH=" << engine->maxPathDepth <<
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
		case slg::ocl::FILTER_BLACKMANHARRIS:
			ss << " -D PARAM_IMAGE_FILTER_TYPE=4" <<
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
					" -D PARAM_SAMPLER_SOBOL_MAXDEPTH=" << max(SOBOL_MAXDEPTH, engine->maxPathDepth);
			break;
		}
		default:
			throw runtime_error("Unknown sampler type: " + boost::lexical_cast<string>(sampler->type));
	}
	
	return ss.str();
}

string PathOCLRenderThread::AdditionalKernelDefinitions() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;

	if (engine->oclSampler->type == slg::ocl::SOBOL) {
		// Generate the Sobol vectors
		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Sobol table size: " << sampleDimensions * SOBOL_BITS);
		u_int *directions = new u_int[sampleDimensions * SOBOL_BITS];

		SobolGenerateDirectionVectors(directions, sampleDimensions);

		stringstream sobolTableSS;
		sobolTableSS << "#line 2 \"Sobol Table in pathoclthread.cpp\"\n";
		sobolTableSS << "__constant uint SOBOL_DIRECTIONS[" << sampleDimensions * SOBOL_BITS << "] = {\n";
		for (u_int i = 0; i < sampleDimensions * SOBOL_BITS; ++i) {
			if (i != 0)
				sobolTableSS << ", ";
			sobolTableSS << directions[i] << "u";
		}
		sobolTableSS << "};\n";

		delete directions;

		return sobolTableSS.str();
	} else
		return "";
}

string PathOCLRenderThread::AdditionalKernelSources() {
	stringstream ssKernel;
	ssKernel <<
			slg::ocl::KernelSource_pathocl_datatypes <<
			slg::ocl::KernelSource_pathocl_funcs <<
			slg::ocl::KernelSource_pathocl_kernels_micro;
	
	return ssKernel.str();
}

void PathOCLRenderThread::CompileAdditionalKernels(cl::Program *program) {
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

void PathOCLRenderThread::InitGPUTaskBuffer() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
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
	if ((engine->compiledScene->lightTypeCounts[TYPE_TRIANGLE] > 0) || engine->compiledScene->HasVolumes())
		gpuTaskSize += GetOpenCLHitPointSize();

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaskSize << "bytes");
	AllocOCLBufferRW(&tasksBuff, gpuTaskSize * taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate tasksDirectLightBuff
	//--------------------------------------------------------------------------

	size_t gpuDirectLightTaskSize = 
			sizeof(slg::ocl::pathocl::DirectLightIlluminateInfo) + 
			sizeof(BSDFEvent) + 
			sizeof(float);

	// Add rayPassThroughEvent memory size
	if (hasPassThrough)
		gpuDirectLightTaskSize += sizeof(float);

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask DirectLight: " << gpuDirectLightTaskSize << "bytes");
	AllocOCLBufferRW(&tasksDirectLightBuff, gpuDirectLightTaskSize * taskCount, "GPUTaskDirectLight");

	//--------------------------------------------------------------------------
	// Allocate tasksStateBuff
	//--------------------------------------------------------------------------

	size_t gpuTaksStateSize =
			sizeof(int) + // state
			sizeof(u_int) + // pathVertexCount
			sizeof(Spectrum);

	// Add BSDF memory size
	gpuTaksStateSize += openCLBSDFSize;

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a GPUTask State: " << gpuTaksStateSize << "bytes");
	AllocOCLBufferRW(&tasksStateBuff, gpuTaksStateSize * taskCount, "GPUTaskState");
}

void PathOCLRenderThread::InitSamplesBuffer() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
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
	} else
		throw runtime_error("Unknown sampler.type: " + boost::lexical_cast<string>(engine->oclSampler->type));

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a Sample: " << sampleSize << "bytes");
	AllocOCLBufferRW(&samplesBuff, sampleSize * taskCount, "Sample");
}

void PathOCLRenderThread::InitSampleDataBuffer() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;
	const bool hasPassThrough = engine->compiledScene->RequiresPassThrough();

	const size_t eyePathVertexDimension =
		// IDX_SCREEN_X, IDX_SCREEN_Y, IDX_EYE_TIME
		3 +
		// IDX_EYE_PASSTROUGHT
		(hasPassThrough ? 1 : 0) +
		// IDX_DOF_X, IDX_DOF_Y
		((engine->compiledScene->enableCameraDOF) ? 2 : 0);
	const size_t PerPathVertexDimension =
		// IDX_PASSTHROUGH,
		(hasPassThrough ? 1 : 0) +
		// IDX_BSDF_X, IDX_BSDF_Y
		2 +
		// IDX_DIRECTLIGHT_X, IDX_DIRECTLIGHT_Y, IDX_DIRECTLIGHT_Z, IDX_DIRECTLIGHT_W, IDX_DIRECTLIGHT_A
		4 + (hasPassThrough ? 1 : 0) +
		// IDX_RR
		1;
	sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * engine->maxPathDepth;

	size_t uDataSize;
	if ((engine->oclSampler->type == slg::ocl::RANDOM) ||
			(engine->oclSampler->type == slg::ocl::SOBOL)) {
		// Only IDX_SCREEN_X, IDX_SCREEN_Y
		uDataSize = sizeof(float) * 2;
		
		if (engine->oclSampler->type == slg::ocl::SOBOL) {
			// Limit the number of dimensions where I use Sobol sequence (after,
			// I switch to Random sampler.
			sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * max(SOBOL_MAXDEPTH, engine->maxPathDepth);
		}
	} else if (engine->oclSampler->type == slg::ocl::METROPOLIS) {
		// Metropolis needs 2 sets of samples, the current and the proposed mutation
		uDataSize = 2 * sizeof(float) * sampleDimensions;
	} else
		throw runtime_error("Unknown sampler.type: " + boost::lexical_cast<string>(engine->oclSampler->type));

	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Sample dimensions: " << sampleDimensions);
	SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Size of a SampleData: " << uDataSize << "bytes");

	// TOFIX
	AllocOCLBufferRW(&sampleDataBuff, uDataSize * taskCount + 1, "SampleData"); // +1 to avoid METROPOLIS + Intel\AMD OpenCL crash
}

void PathOCLRenderThread::AdditionalInit() {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	// In case renderEngine->taskCount has changed
	delete[] gpuTaskStats;
	gpuTaskStats = new slg::ocl::pathocl::GPUTaskStats[taskCount];
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

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::pathocl::GPUTaskStats) * taskCount, "GPUTask Stats");

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
		AllocOCLBufferRW(&directLightVolInfosBuff, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "directLightVolumeInfo");
	}

	//--------------------------------------------------------------------------
	// Allocate GPU pixel filter distribution
	//--------------------------------------------------------------------------

	if (engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE)) {
		AllocOCLBufferRO(&pixelFilterBuff, engine->pixelFilterDistribution,
				sizeof(float) * engine->pixelFilterDistributionSize, "Pixel Filter Distribution");
	}
}

void PathOCLRenderThread::SetAdvancePathsKernelArgs(cl::Kernel *advancePathsKernel) {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	CompiledScene *cscene = engine->compiledScene;

	u_int argIndex = 0;
	advancePathsKernel->setArg(argIndex++, *tasksBuff);
	advancePathsKernel->setArg(argIndex++, *tasksDirectLightBuff);
	advancePathsKernel->setArg(argIndex++, *tasksStateBuff);
	advancePathsKernel->setArg(argIndex++, *taskStatsBuff);
	if (engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE))
		advancePathsKernel->setArg(argIndex++, *pixelFilterBuff);
	advancePathsKernel->setArg(argIndex++, *samplesBuff);
	advancePathsKernel->setArg(argIndex++, *sampleDataBuff);
	if (cscene->HasVolumes()) {
		advancePathsKernel->setArg(argIndex++, *pathVolInfosBuff);
		advancePathsKernel->setArg(argIndex++, *directLightVolInfosBuff);
	}
	advancePathsKernel->setArg(argIndex++, *raysBuff);
	advancePathsKernel->setArg(argIndex++, *hitsBuff);

	// Film parameters
	argIndex = threadFilms[0]->SetFilmKernelArgs(*advancePathsKernel, argIndex);

	// Scene parameters
	if (cscene->hasInfiniteLights) {
		advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.x);
		advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.y);
		advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.center.z);
		advancePathsKernel->setArg(argIndex++, cscene->worldBSphere.rad);
	}
	advancePathsKernel->setArg(argIndex++, *materialsBuff);
	advancePathsKernel->setArg(argIndex++, *texturesBuff);
	advancePathsKernel->setArg(argIndex++, *meshMatsBuff);
	advancePathsKernel->setArg(argIndex++, *meshDescsBuff);
	advancePathsKernel->setArg(argIndex++, *vertsBuff);
	if (normalsBuff)
		advancePathsKernel->setArg(argIndex++, *normalsBuff);
	if (uvsBuff)
		advancePathsKernel->setArg(argIndex++, *uvsBuff);
	if (colsBuff)
		advancePathsKernel->setArg(argIndex++, *colsBuff);
	if (alphasBuff)
		advancePathsKernel->setArg(argIndex++, *alphasBuff);
	advancePathsKernel->setArg(argIndex++, *trianglesBuff);
	advancePathsKernel->setArg(argIndex++, *cameraBuff);
	// Lights
	advancePathsKernel->setArg(argIndex++, *lightsBuff);
	if (envLightIndicesBuff) {
		advancePathsKernel->setArg(argIndex++, *envLightIndicesBuff);
		advancePathsKernel->setArg(argIndex++, (u_int)cscene->envLightIndices.size());
	}
	advancePathsKernel->setArg(argIndex++, *meshTriLightDefsOffsetBuff);
	if (infiniteLightDistributionsBuff)
		advancePathsKernel->setArg(argIndex++, *infiniteLightDistributionsBuff);
	advancePathsKernel->setArg(argIndex++, *lightsDistributionBuff);

	// Images
	if (imageMapDescsBuff) {
		advancePathsKernel->setArg(argIndex++, *imageMapDescsBuff);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			advancePathsKernel->setArg(argIndex++, *(imageMapsBuff[i]));
	}
}

void PathOCLRenderThread::SetAdditionalKernelArgs() {
	// Set OpenCL kernel arguments

	// OpenCL kernel setArg() is the only non thread safe function in OpenCL 1.1 so
	// I need to use a mutex here
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	CompiledScene *cscene = engine->compiledScene;
	boost::unique_lock<boost::mutex> lock(engine->setKernelArgsMutex);

	//--------------------------------------------------------------------------
	// advancePathsKernel
	//--------------------------------------------------------------------------

	if (advancePathsKernel_MK_RT_NEXT_VERTEX)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_NEXT_VERTEX);
	if (advancePathsKernel_MK_HIT_NOTHING)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_NOTHING);
	if (advancePathsKernel_MK_HIT_OBJECT)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_HIT_OBJECT);
	if (advancePathsKernel_MK_RT_DL)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_RT_DL);
	if (advancePathsKernel_MK_DL_ILLUMINATE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_ILLUMINATE);
	if (advancePathsKernel_MK_DL_SAMPLE_BSDF)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_DL_SAMPLE_BSDF);
	if (advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY);
	if (advancePathsKernel_MK_SPLAT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_SPLAT_SAMPLE);
	if (advancePathsKernel_MK_NEXT_SAMPLE)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_NEXT_SAMPLE);
	if (advancePathsKernel_MK_GENERATE_CAMERA_RAY)
		SetAdvancePathsKernelArgs(advancePathsKernel_MK_GENERATE_CAMERA_RAY);

	//--------------------------------------------------------------------------
	// initKernel
	//--------------------------------------------------------------------------

	u_int argIndex = 0;
	initKernel->setArg(argIndex++, engine->seedBase + threadIndex * engine->taskCount);
	initKernel->setArg(argIndex++, *tasksBuff);
	initKernel->setArg(argIndex++, *tasksDirectLightBuff);
	initKernel->setArg(argIndex++, *tasksStateBuff);
	initKernel->setArg(argIndex++, *taskStatsBuff);
	initKernel->setArg(argIndex++, *samplesBuff);
	initKernel->setArg(argIndex++, *sampleDataBuff);
	if (cscene->HasVolumes())
		initKernel->setArg(argIndex++, *pathVolInfosBuff);
	if (engine->useFastPixelFilter && (engine->oclPixelFilter->type != slg::ocl::FILTER_NONE))
		initKernel->setArg(argIndex++, *pixelFilterBuff);
	initKernel->setArg(argIndex++, *raysBuff);
	initKernel->setArg(argIndex++, *cameraBuff);
	initKernel->setArg(argIndex++, threadFilms[0]->film->GetWidth());
	initKernel->setArg(argIndex++, threadFilms[0]->film->GetHeight());
}

void PathOCLRenderThread::Stop() {
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

void PathOCLRenderThread::EnqueueAdvancePathsKernel(cl::CommandQueue &oclQueue) {
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	// Micro kernels version
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_NEXT_VERTEX, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_NOTHING, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_HIT_OBJECT, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_RT_DL, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_ILLUMINATE, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_DL_SAMPLE_BSDF, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_NEXT_VERTEX_RAY, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_SPLAT_SAMPLE, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_NEXT_SAMPLE, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
	oclQueue.enqueueNDRangeKernel(*advancePathsKernel_MK_GENERATE_CAMERA_RAY, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(taskCount, advancePathsWorkGroupSize)),
			cl::NDRange(advancePathsWorkGroupSize));
}

void PathOCLRenderThread::RenderThreadImpl() {
	//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread started");

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
	PathOCLRenderEngine *engine = (PathOCLRenderEngine *)renderEngine;
	const u_int taskCount = engine->taskCount;

	try {
		//----------------------------------------------------------------------
		// Execute initialization kernels
		//----------------------------------------------------------------------

		// Clear the frame buffer
		const u_int filmPixelCount = threadFilms[0]->film->GetWidth() * threadFilms[0]->film->GetHeight();
		oclQueue.enqueueNDRangeKernel(*filmClearKernel, cl::NullRange,
			cl::NDRange(RoundUp<u_int>(filmPixelCount, filmClearWorkGroupSize)),
			cl::NDRange(filmClearWorkGroupSize));

		// Initialize the tasks buffer
		oclQueue.enqueueNDRangeKernel(*initKernel, cl::NullRange,
				cl::NDRange(RoundUp<u_int>(engine->taskCount, initWorkGroupSize)),
				cl::NDRange(initWorkGroupSize));

		//----------------------------------------------------------------------
		// Rendering loop
		//----------------------------------------------------------------------

		// signed int in order to avoid problems with underflows (when computing
		// iterations - 1)
		int iterations = 1;
		u_int totalIterations = 0;
		const u_int haltDebug = engine->renderConfig->GetProperty("batch.haltdebug").
			Get<u_int>();

		double startTime = WallClockTime();
		bool done = false;
		while (!boost::this_thread::interruption_requested() && !done) {
			/*if (threadIndex == 0)
				cerr << "[DEBUG] =================================";*/

			// Async. transfer of the Film buffers
			threadFilms[0]->TransferFilm(oclQueue);

			// Async. transfer of GPU task statistics
			oclQueue.enqueueReadBuffer(
				*(taskStatsBuff),
				CL_FALSE,
				0,
				sizeof(slg::ocl::pathocl::GPUTaskStats) * taskCount,
				gpuTaskStats);

			// Decide the target refresh time based on screen refresh interval
			const u_int screenRefreshInterval = engine->renderConfig->GetProperty("screen.refresh.interval").Get<u_int>();
			double targetTime;
			if (screenRefreshInterval <= 100)
				targetTime = 0.025; // 25 ms
			else if (screenRefreshInterval <= 500)
				targetTime = 0.050; // 50 ms
			else
				targetTime = 0.075; // 75 ms

			for (;;) {
				cl::Event event;

				const double t1 = WallClockTime();
				for (int i = 0; i < iterations; ++i) {
					// Trace rays
					intersectionDevice->EnqueueTraceRayBuffer(*raysBuff,
							*(hitsBuff), taskCount, NULL, (i == 0) ? &event : NULL);

					// Advance to next path state
					EnqueueAdvancePathsKernel(oclQueue);
				}
				oclQueue.flush();
				totalIterations += (u_int)iterations;

				event.wait();
				const double t2 = WallClockTime();

				/*if (threadIndex == 0)
					cerr << "[DEBUG] Delta time: " << (t2 - t1) * 1000.0 <<
							"ms (screenRefreshInterval: " << screenRefreshInterval <<
							" iterations: " << iterations << ")\n";*/

				// Check if I have to adjust the number of kernel enqueued (only
				// if haltDebug is not enabled)
				if (haltDebug == 0u) {
					if (t2 - t1 > targetTime)
						iterations = Max(iterations - 1, 1);
					else
						iterations = Min(iterations + 1, 128);
				}

				// Check if it is time to refresh the screen
				if (((t2 - startTime) * 1000.0 > (double)screenRefreshInterval) ||
						boost::this_thread::interruption_requested())
					break;

				if ((haltDebug > 0u) && (totalIterations >= haltDebug)) {
					done = true;
					break;
				}
			}

			startTime = WallClockTime();
		}

		//SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (boost::thread_interrupted) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread halted");
	} catch (cl::Error &err) {
		SLG_LOG("[PathOCLRenderThread::" << threadIndex << "] Rendering thread ERROR: " << err.what() <<
				"(" << oclErrorString(err.err()) << ")");
	}

	threadFilms[0]->TransferFilm(oclQueue);
	oclQueue.finish();
}

#endif

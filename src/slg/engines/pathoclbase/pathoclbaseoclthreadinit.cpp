/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxcore/cfg.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"
#include "slg/utils/pathinfo.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread initialization methods
//------------------------------------------------------------------------------

size_t PathOCLBaseOCLRenderThread::GetOpenCLHitPointSize() const {
	// HitPoint memory size
	size_t hitPointSize = sizeof(Vector) +
			sizeof(Point) +
			3 * sizeof(Normal) +
			EXTMESH_MAX_DATA_COUNT * sizeof(UV) +
			sizeof(Transform);
	hitPointSize += EXTMESH_MAX_DATA_COUNT * sizeof(Spectrum);
	hitPointSize += EXTMESH_MAX_DATA_COUNT * sizeof(float);
	// passThroughEvent
	hitPointSize += sizeof(float);
	// Fields dpdu, dpdv, dndu, dndv
	hitPointSize += 2 * sizeof(Vector) + 2 * sizeof(Normal);
	// Volume fields
	hitPointSize += 2 * sizeof(u_int) + 2 * sizeof(u_int);
	// Object ID
	if (renderEngine->compiledScene->IsTextureCompiled(OBJECTID_TEX) ||
			renderEngine->compiledScene->IsTextureCompiled(OBJECTID_COLOR_TEX) ||
			renderEngine->compiledScene->IsTextureCompiled(OBJECTID_NORMALIZED_TEX))
		hitPointSize += sizeof(u_int);
	// intoObject
	hitPointSize += sizeof(int);
	// throughShadowTransparency
	hitPointSize += sizeof(int);

	return hitPointSize;
}

size_t PathOCLBaseOCLRenderThread::GetOpenCLBSDFSize() const {
	// Add BSDF memory size
	size_t bsdfSize = GetOpenCLHitPointSize();
	// Add BSDF.materialIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.sceneObjectIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.triangleLightSourceIndex memory size
	bsdfSize += sizeof(u_int);
	// Add BSDF.Frame memory size
	bsdfSize += sizeof(slg::ocl::Frame);
	// Add BSDF.isVolume memory size
	bsdfSize += sizeof(int);

	return bsdfSize;
}

size_t PathOCLBaseOCLRenderThread::GetEyePathInfoSize() const {
	// Add PathDepthInfo memory size
	size_t eyePathInfoSize = sizeof(slg::ocl::PathDepthInfo);
	// Add PathVolumeInfo memory size
	eyePathInfoSize += sizeof(slg::ocl::PathVolumeInfo);

	// Add isPassThroughPath memory size
	eyePathInfoSize += sizeof(int);

	// Add lastBSDFEvent memory size
	eyePathInfoSize += sizeof(slg::ocl::BSDFEvent);
	// Add lastBSDFPdfW memory size
	eyePathInfoSize += sizeof(float);
	// Add lastGlossiness memory size
	eyePathInfoSize += sizeof(float);
	// Add lastShadeN memory size
	eyePathInfoSize += sizeof(slg::ocl::Normal);
	// Add lastFromVolume memory size
	eyePathInfoSize += sizeof(int);

	// Add isNearlyCaustic memory size
	eyePathInfoSize += sizeof(int);
	// Add isNearlyS memory size
	eyePathInfoSize += sizeof(int);
	// Add isNearlySD memory size
	eyePathInfoSize += sizeof(int);
	// Add isNearlySDS memory size
	eyePathInfoSize += sizeof(int);

	return eyePathInfoSize;
}

void PathOCLBaseOCLRenderThread::AllocOCLBuffer(const cl_mem_flags clFlags, cl::Buffer **buff,
		void *src, const size_t size, const string &desc) {
	intersectionDevice->AllocBuffer(clFlags, buff, src, size, desc);
}

void PathOCLBaseOCLRenderThread::AllocOCLBufferRO(cl::Buffer **buff, void *src, const size_t size, const string &desc) {
	intersectionDevice->AllocBufferRO(buff, src, size, desc);
}

void PathOCLBaseOCLRenderThread::AllocOCLBufferRW(cl::Buffer **buff, const size_t size, const string &desc) {
	intersectionDevice->AllocBufferRW(buff, size, desc);
}

void PathOCLBaseOCLRenderThread::FreeOCLBuffer(cl::Buffer **buff) {
	intersectionDevice->FreeBuffer(buff);
}

void PathOCLBaseOCLRenderThread::InitFilm() {
	if (threadFilms.size() == 0)
		IncThreadFilms();

	u_int threadFilmWidth, threadFilmHeight, threadFilmSubRegion[4];
	GetThreadFilmSize(&threadFilmWidth, &threadFilmHeight, threadFilmSubRegion);

	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->Init(renderEngine->film, threadFilmWidth, threadFilmHeight,
			threadFilmSubRegion);
}

void PathOCLBaseOCLRenderThread::InitCamera() {
	AllocOCLBufferRO(&cameraBuff, &renderEngine->compiledScene->camera,
			sizeof(slg::ocl::Camera), "Camera");
}

void PathOCLBaseOCLRenderThread::InitGeometry() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->normals.size() > 0)
		AllocOCLBufferRO(&normalsBuff, &cscene->normals[0],
				sizeof(Normal) * cscene->normals.size(), "Normals");
	else
		FreeOCLBuffer(&normalsBuff);

	if (cscene->uvs.size() > 0)
		AllocOCLBufferRO(&uvsBuff, &cscene->uvs[0],
			sizeof(UV) * cscene->uvs.size(), "UVs");
	else
		FreeOCLBuffer(&uvsBuff);

	if (cscene->cols.size() > 0)
		AllocOCLBufferRO(&colsBuff, &cscene->cols[0],
			sizeof(Spectrum) * cscene->cols.size(), "Colors");
	else
		FreeOCLBuffer(&colsBuff);

	if (cscene->alphas.size() > 0)
		AllocOCLBufferRO(&alphasBuff, &cscene->alphas[0],
			sizeof(float) * cscene->alphas.size(), "Alphas");
	else
		FreeOCLBuffer(&alphasBuff);

	AllocOCLBufferRO(&triNormalsBuff, &cscene->triNormals[0],
				sizeof(Normal) * cscene->triNormals.size(), "Triangle normals");

	AllocOCLBufferRO(&vertsBuff, &cscene->verts[0],
		sizeof(Point) * cscene->verts.size(), "Vertices");

	AllocOCLBufferRO(&trianglesBuff, &cscene->tris[0],
		sizeof(Triangle) * cscene->tris.size(), "Triangles");

	if (cscene->interpolatedTransforms.size() > 0) {
		AllocOCLBufferRO(&interpolatedTransformsBuff, &cscene->interpolatedTransforms[0],
			sizeof(luxrays::ocl::InterpolatedTransform) * cscene->interpolatedTransforms.size(), "Interpolated transformations");
	} else
		FreeOCLBuffer(&interpolatedTransformsBuff);

	AllocOCLBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
			sizeof(slg::ocl::ExtMesh) * cscene->meshDescs.size(), "Mesh description");
}

void PathOCLBaseOCLRenderThread::InitMaterials() {
	const size_t materialsCount = renderEngine->compiledScene->mats.size();
	AllocOCLBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
			sizeof(slg::ocl::Material) * materialsCount, "Materials");
}

void PathOCLBaseOCLRenderThread::InitSceneObjects() {
	const u_int sceneObjsCount = renderEngine->compiledScene->sceneObjs.size();
	AllocOCLBufferRO(&scnObjsBuff, &renderEngine->compiledScene->sceneObjs[0],
			sizeof(slg::ocl::SceneObject) * sceneObjsCount, "Scene objects");
}

void PathOCLBaseOCLRenderThread::InitTextures() {
	const size_t texturesCount = renderEngine->compiledScene->texs.size();
	AllocOCLBufferRO(&texturesBuff, &renderEngine->compiledScene->texs[0],
			sizeof(slg::ocl::Texture) * texturesCount, "Textures");
}

void PathOCLBaseOCLRenderThread::InitLights() {
	CompiledScene *cscene = renderEngine->compiledScene;

	AllocOCLBufferRO(&lightsBuff, &cscene->lightDefs[0],
		sizeof(slg::ocl::LightSource) * cscene->lightDefs.size(), "Lights");
	if (cscene->envLightIndices.size() > 0) {
		AllocOCLBufferRO(&envLightIndicesBuff, &cscene->envLightIndices[0],
				sizeof(u_int) * cscene->envLightIndices.size(), "Env. light indices");
	} else
		FreeOCLBuffer(&envLightIndicesBuff);

	AllocOCLBufferRO(&lightIndexOffsetByMeshIndexBuff, &cscene->lightIndexOffsetByMeshIndex[0],
		sizeof(u_int) * cscene->lightIndexOffsetByMeshIndex.size(), "Light offsets (Part I)");
	AllocOCLBufferRO(&lightIndexByTriIndexBuff, &cscene->lightIndexByTriIndex[0],
		sizeof(u_int) * cscene->lightIndexByTriIndex.size(), "Light offsets (Part II)");

	if (cscene->envLightDistributions.size() > 0) {
		AllocOCLBufferRO(&envLightDistributionsBuff, &cscene->envLightDistributions[0],
			sizeof(float) * cscene->envLightDistributions.size(), "Env. light distributions");
	} else
		FreeOCLBuffer(&envLightDistributionsBuff);

	AllocOCLBufferRO(&lightsDistributionBuff, cscene->lightsDistribution,
		cscene->lightsDistributionSize, "LightsDistribution");
	AllocOCLBufferRO(&infiniteLightSourcesDistributionBuff, cscene->infiniteLightSourcesDistribution,
		cscene->infiniteLightSourcesDistributionSize, "InfiniteLightSourcesDistribution");
	if (cscene->dlscAllEntries.size() > 0) {
		AllocOCLBufferRO(&dlscAllEntriesBuff, &cscene->dlscAllEntries[0],
			cscene->dlscAllEntries.size() * sizeof(slg::ocl::DLSCacheEntry), "DLSC all entries");
		AllocOCLBufferRO(&dlscDistributionsBuff, &cscene->dlscDistributions[0],
			cscene->dlscDistributions.size() * sizeof(float), "DLSC distributions table");
		AllocOCLBufferRO(&dlscBVHNodesBuff, &cscene->dlscBVHArrayNode[0],
			cscene->dlscBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "DLSC BVH nodes");
	} else {
		FreeOCLBuffer(&dlscAllEntriesBuff);
		FreeOCLBuffer(&dlscDistributionsBuff);
		FreeOCLBuffer(&dlscBVHNodesBuff);
	}
	
	if (cscene->elvcAllEntries.size() > 0) {
		AllocOCLBufferRO(&elvcAllEntriesBuff, &cscene->elvcAllEntries[0],
			cscene->elvcAllEntries.size() * sizeof(slg::ocl::ELVCacheEntry), "ELVC all entries");
		AllocOCLBufferRO(&elvcDistributionsBuff, &cscene->elvcDistributions[0],
			cscene->elvcDistributions.size() * sizeof(float), "ELVC distributions table");
		AllocOCLBufferRO(&elvcBVHNodesBuff, &cscene->elvcBVHArrayNode[0],
			cscene->elvcBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "ELVC BVH nodes");
	} else {
		FreeOCLBuffer(&elvcAllEntriesBuff);
		FreeOCLBuffer(&elvcDistributionsBuff);
		FreeOCLBuffer(&elvcBVHNodesBuff);
	}
}

void PathOCLBaseOCLRenderThread::InitPhotonGI() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->pgicRadiancePhotons.size() > 0) {
		AllocOCLBufferRO(&pgicRadiancePhotonsBuff, &cscene->pgicRadiancePhotons[0],
			cscene->pgicRadiancePhotons.size() * sizeof(slg::ocl::RadiancePhoton), "PhotonGI indirect cache all entries");
		AllocOCLBufferRO(&pgicRadiancePhotonsBVHNodesBuff, &cscene->pgicRadiancePhotonsBVHArrayNode[0],
			cscene->pgicRadiancePhotonsBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "PhotonGI indirect cache BVH nodes");
	} else {
		FreeOCLBuffer(&pgicRadiancePhotonsBuff);
		FreeOCLBuffer(&pgicRadiancePhotonsBVHNodesBuff);
	}

	if (cscene->pgicCausticPhotons.size() > 0) {
		AllocOCLBufferRO(&pgicCausticPhotonsBuff, &cscene->pgicCausticPhotons[0],
			cscene->pgicCausticPhotons.size() * sizeof(slg::ocl::Photon), "PhotonGI caustic cache all entries");
		AllocOCLBufferRO(&pgicCausticPhotonsBVHNodesBuff, &cscene->pgicCausticPhotonsBVHArrayNode[0],
			cscene->pgicCausticPhotonsBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "PhotonGI caustic cache BVH nodes");
	} else {
		FreeOCLBuffer(&pgicCausticPhotonsBuff);
		FreeOCLBuffer(&pgicCausticPhotonsBVHNodesBuff);
	}
}

void PathOCLBaseOCLRenderThread::InitImageMaps() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->imageMapDescs.size() > 0) {
		AllocOCLBufferRO(&imageMapDescsBuff, &cscene->imageMapDescs[0],
				sizeof(slg::ocl::ImageMap) * cscene->imageMapDescs.size(), "ImageMap descriptions");

		// Free unused pages
		for (u_int i = cscene->imageMapMemBlocks.size(); i < imageMapsBuff.size(); ++i)
			FreeOCLBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(cscene->imageMapMemBlocks.size(), NULL);

		for (u_int i = 0; i < imageMapsBuff.size(); ++i) {
			AllocOCLBufferRO(&(imageMapsBuff[i]), &(cscene->imageMapMemBlocks[i][0]),
					sizeof(float) * cscene->imageMapMemBlocks[i].size(), "ImageMaps");
		}
	} else {
		FreeOCLBuffer(&imageMapDescsBuff);
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			FreeOCLBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(0);
	}
}

void PathOCLBaseOCLRenderThread::InitGPUTaskBuffer() {
	const u_int taskCount = renderEngine->taskCount;
	const size_t openCLBSDFSize = GetOpenCLBSDFSize();

	//--------------------------------------------------------------------------
	// Allocate tasksConfigBuff
	//--------------------------------------------------------------------------

	AllocOCLBufferRO(&taskConfigBuff, &renderEngine->taskConfig, sizeof(slg::ocl::pathoclbase::GPUTaskConfiguration), "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate tasksBuff
	//--------------------------------------------------------------------------
	
	// Add Seed memory size
	size_t gpuTaskSize = sizeof(slg::ocl::Seed);

	// Add temporary storage space
	
	// Add tmpBsdf memory size
	gpuTaskSize += openCLBSDFSize;

	// Add tmpHitPoint memory size
	gpuTaskSize += GetOpenCLHitPointSize();

	// Add tmpPathDepthInfo memory size
	gpuTaskSize += sizeof(slg::ocl::PathDepthInfo);

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaskSize << "bytes");
	AllocOCLBufferRW(&tasksBuff, gpuTaskSize * taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate tasksDirectLightBuff
	//--------------------------------------------------------------------------

	size_t gpuDirectLightTaskSize = 
			sizeof(slg::ocl::pathoclbase::DirectLightIlluminateInfo) + 
			sizeof(int) + // directLightResult
			sizeof(int);  // throughShadowTransparency

	// Add seedPassThroughEvent memory size
	gpuDirectLightTaskSize += sizeof(ocl::Seed);

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a GPUTask DirectLight: " << gpuDirectLightTaskSize << "bytes");
	AllocOCLBufferRW(&tasksDirectLightBuff, gpuDirectLightTaskSize * taskCount, "GPUTaskDirectLight");

	//--------------------------------------------------------------------------
	// Allocate tasksStateBuff
	//--------------------------------------------------------------------------

	size_t gpuTaskStateSize =
			sizeof(int) + // state
			sizeof(slg::ocl::PathDepthInfo) + // depthInfo
			sizeof(Spectrum) + // throughput
			sizeof(int) + // albedoToDo
			sizeof(int) + // photonGICacheEnabledOnLastHit
			sizeof(int) + // photonGICausticCacheUsed
			sizeof(int) + // photonGIShowIndirectPathMixUsed
			sizeof(int);  // throughShadowTransparency

	// Add seedPassThroughEvent memory size
	gpuTaskStateSize += sizeof(ocl::Seed);

	// Add BSDF memory size
	gpuTaskStateSize += openCLBSDFSize;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a GPUTask State: " << gpuTaskStateSize << "bytes");
	AllocOCLBufferRW(&tasksStateBuff, gpuTaskStateSize * taskCount, "GPUTaskState");
}

void PathOCLBaseOCLRenderThread::InitSamplerSharedDataBuffer() {
	const u_int *subRegion = renderEngine->film->GetSubRegion();
	const u_int filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);

	size_t size = 0;
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		size += sizeof(slg::ocl::RandomSamplerSharedData);
	} else if (renderEngine->oclSampler->type == slg::ocl::METROPOLIS) {
		// Nothing
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		size += sizeof(slg::ocl::SobolSamplerSharedData);

		// Plus the a pass field for each pixel
		size += sizeof(u_int) * filmRegionPixelCount;

		// Plus the Sobol directions array
		size += sizeof(u_int) * renderEngine->pathTracer.eyeSampleSize * SOBOL_BITS;
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		size += sizeof(slg::ocl::TilePathSamplerSharedData);

		switch (renderEngine->GetType()) {
			case TILEPATHOCL:
				size += sizeof(u_int) * renderEngine->pathTracer.eyeSampleSize * SOBOL_BITS;
				break;
			case RTPATHOCL:
				break;
			default:
				throw runtime_error("Unknown render engine in PathOCLBaseRenderThread::InitSamplerSharedDataBuffer(): " +
						boost::lexical_cast<string>(renderEngine->GetType()));
		}
	} else
		throw runtime_error("Unknown sampler.type in PathOCLBaseRenderThread::InitSamplerSharedDataBuffer(): " +
				boost::lexical_cast<string>(renderEngine->oclSampler->type));

	if (size == 0)
		FreeOCLBuffer(&samplerSharedDataBuff);
	else
		AllocOCLBufferRW(&samplerSharedDataBuff, size, "SamplerSharedData");

	// Initialize the sampler shared data
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		slg::ocl::RandomSamplerSharedData rssd;
		rssd.pixelBucketIndex = 0; // Initialized by OpenCL kernel
		rssd.adaptiveStrength = renderEngine->oclSampler->random.adaptiveStrength;
		rssd.adaptiveUserImportanceWeight = renderEngine->oclSampler->random.adaptiveUserImportanceWeight;

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		oclQueue.enqueueWriteBuffer(*samplerSharedDataBuff, CL_TRUE, 0, size, &rssd);
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		char *buffer = new char[size];

		// Initialize SobolSamplerSharedData fields
		slg::ocl::SobolSamplerSharedData *sssd = (slg::ocl::SobolSamplerSharedData *)buffer;

		sssd->seedBase = renderEngine->seedBase;
		sssd->pixelBucketIndex = 0; // Initialized by OpenCL kernel
		sssd->adaptiveStrength = renderEngine->oclSampler->sobol.adaptiveStrength;
		sssd->adaptiveUserImportanceWeight = renderEngine->oclSampler->sobol.adaptiveUserImportanceWeight;
		sssd->filmRegionPixelCount = filmRegionPixelCount;

		// Initialize all pass values. The pass buffer is attached at the
		// end of slg::ocl::SobolSamplerSharedData
		u_int *passBuffer = (u_int *)(buffer + sizeof(slg::ocl::SobolSamplerSharedData));
		fill(passBuffer, passBuffer + filmRegionPixelCount, SOBOL_STARTOFFSET);

		// Initialize the Sobol directions array values. The pass buffer is attached at the
		// end of slg::ocl::SobolSamplerSharedData + all pass values

		u_int *sobolDirections = (u_int *)(buffer + sizeof(slg::ocl::SobolSamplerSharedData) + sizeof(u_int) * filmRegionPixelCount);
		SobolSequence::GenerateDirectionVectors(sobolDirections, renderEngine->pathTracer.eyeSampleSize);

		// Write the data
		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		oclQueue.enqueueWriteBuffer(*samplerSharedDataBuff, CL_TRUE, 0, size, buffer);
		
		delete[] buffer;
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		// TilePathSamplerSharedData is updated in PathOCLBaseOCLRenderThread::UpdateSamplerData()
		
		switch (renderEngine->GetType()) {
			case TILEPATHOCL: {
				char *buffer = new char[size];

				// Initialize the Sobol directions array values
				u_int *sobolDirections = (u_int *)(buffer + sizeof(slg::ocl::TilePathSamplerSharedData));
				SobolSequence::GenerateDirectionVectors(sobolDirections, renderEngine->pathTracer.eyeSampleSize);

				cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
				oclQueue.enqueueWriteBuffer(*samplerSharedDataBuff, CL_TRUE, 0, size, &buffer[0]);
				delete [] buffer;
				break;
			}
			case RTPATHOCL:
				break;
			default:
				throw runtime_error("Unknown render engine in PathOCLBaseRenderThread::InitSamplerSharedDataBuffer(): " +
						boost::lexical_cast<string>(renderEngine->GetType()));
		}
	}
}

void PathOCLBaseOCLRenderThread::InitSamplesBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	//--------------------------------------------------------------------------
	// Sample size
	//--------------------------------------------------------------------------

	size_t sampleSize = 0;

	// Add Sample memory size
	switch (renderEngine->oclSampler->type) {
		case slg::ocl::RANDOM: {
			// pixelIndexBase, pixelIndexOffset and pixelIndexRandomStart fields
			sampleSize += sizeof(slg::ocl::RandomSample);
			break;
		}
		case  slg::ocl::METROPOLIS: {
			const size_t sampleResultSize = sizeof(slg::ocl::SampleResult);
			sampleSize += 2 * sizeof(float) + 5 * sizeof(u_int) + sampleResultSize;	
			break;
		}
		case slg::ocl::SOBOL: {
			sampleSize += sizeof(slg::ocl::SobolSample);
			break;
		}
		case slg::ocl::TILEPATHSAMPLER: {
			sampleSize += sizeof(slg::ocl::TilePathSample);
			break;
		}
		default:
			throw runtime_error("Unknown sampler.type in PathOCLBaseRenderThread::InitSamplesBuffer(): " +
					boost::lexical_cast<string>(renderEngine->oclSampler->type));
	}

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a Sample: " << sampleSize << "bytes");
	AllocOCLBufferRW(&samplesBuff, sampleSize * taskCount, "Sample");
}

void PathOCLBaseOCLRenderThread::InitSampleResultsBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	const size_t sampleResultSize = sizeof(slg::ocl::SampleResult);

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a SampleResult: " << sampleResultSize << "bytes");
	AllocOCLBufferRW(&sampleResultsBuff, sampleResultSize * taskCount, "Sample");
}

void PathOCLBaseOCLRenderThread::InitSampleDataBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	size_t uDataSize;
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		// To store IDX_SCREEN_X and IDX_SCREEN_Y
		uDataSize = 2 * sizeof(float);
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		// To store IDX_SCREEN_X and IDX_SCREEN_Y
		uDataSize = 2 * sizeof(float);
	} else if (renderEngine->oclSampler->type == slg::ocl::METROPOLIS) {
		// Metropolis needs 2 sets of samples, the current and the proposed mutation
		uDataSize = 2 * sizeof(float) * renderEngine->pathTracer.eyeSampleSize;
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		// To store IDX_SCREEN_X and IDX_SCREEN_Y
		uDataSize = 2 * sizeof(float);
	} else
		throw runtime_error("Unknown sampler.type in PathOCLBaseRenderThread::InitSampleDataBuffer(): " + boost::lexical_cast<string>(renderEngine->oclSampler->type));

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a SampleData: " << uDataSize << "bytes");

	AllocOCLBufferRW(&sampleDataBuff, uDataSize * taskCount, "SampleData");
}

void PathOCLBaseOCLRenderThread::InitRender() {
	//--------------------------------------------------------------------------
	// Film definition
	//--------------------------------------------------------------------------

	InitFilm();

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	InitCamera();

	//--------------------------------------------------------------------------
	// Scene geometry
	//--------------------------------------------------------------------------

	InitGeometry();

	//--------------------------------------------------------------------------
	// Image maps
	//--------------------------------------------------------------------------

	InitImageMaps();

	//--------------------------------------------------------------------------
	// Texture definitions
	//--------------------------------------------------------------------------

	InitTextures();

	//--------------------------------------------------------------------------
	// Material definitions
	//--------------------------------------------------------------------------

	InitMaterials();

	//--------------------------------------------------------------------------
	// Mesh <=> Material links
	//--------------------------------------------------------------------------

	InitSceneObjects();

	//--------------------------------------------------------------------------
	// Light definitions
	//--------------------------------------------------------------------------

	InitLights();

	//--------------------------------------------------------------------------
	// Light definitions
	//--------------------------------------------------------------------------

	InitPhotonGI();

	//--------------------------------------------------------------------------
	// GPUTaskStats
	//--------------------------------------------------------------------------

	const u_int taskCount = renderEngine->taskCount;

	// In case renderEngine->taskCount has changed
	delete[] gpuTaskStats;
	gpuTaskStats = new slg::ocl::pathoclbase::GPUTaskStats[taskCount];
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

	AllocOCLBufferRW(&taskStatsBuff, sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount, "GPUTask Stats");

	//--------------------------------------------------------------------------
	// Allocate sampler shared data buffer
	//--------------------------------------------------------------------------

	InitSamplerSharedDataBuffer();

	//--------------------------------------------------------------------------
	// Allocate sample buffers
	//--------------------------------------------------------------------------

	InitSamplesBuffer();

	//--------------------------------------------------------------------------
	// Allocate sample data buffers
	//--------------------------------------------------------------------------

	InitSampleDataBuffer();

	//--------------------------------------------------------------------------
	// Allocate sample result buffers
	//--------------------------------------------------------------------------

	InitSampleResultsBuffer();

	//--------------------------------------------------------------------------
	// Allocate volume info buffers if required
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&eyePathInfosBuff, GetEyePathInfoSize() * taskCount, "PathInfo");

	//--------------------------------------------------------------------------
	// Allocate volume info buffers if required
	//--------------------------------------------------------------------------

	AllocOCLBufferRW(&directLightVolInfosBuff, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "DirectLightVolumeInfo");

	//--------------------------------------------------------------------------
	// Allocate GPU pixel filter distribution
	//--------------------------------------------------------------------------

	AllocOCLBufferRO(&pixelFilterBuff, renderEngine->pixelFilterDistribution,
			renderEngine->pixelFilterDistributionSize, "Pixel Filter Distribution");

	//--------------------------------------------------------------------------
	// Compile kernels
	//--------------------------------------------------------------------------

	InitKernels();

	//--------------------------------------------------------------------------
	// Initialize
	//--------------------------------------------------------------------------

	// Set kernel arguments
	SetKernelArgs();

	cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();

	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms)
		threadFilm->ClearFilm(oclQueue, *filmClearKernel, filmClearWorkGroupSize);

	oclQueue.finish();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

#endif

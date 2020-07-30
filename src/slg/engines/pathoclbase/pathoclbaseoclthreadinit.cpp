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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "luxcore/cfg.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/devices/ocldevice.h"
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
	CompiledScene *cscene = renderEngine->compiledScene;

	intersectionDevice->AllocBufferRO(&cameraBuff, &cscene->camera,
			sizeof(slg::ocl::Camera), "Camera");
	if (cscene->cameraBokehDistribution)
		intersectionDevice->AllocBufferRO(&cameraBokehDistributionBuff, cscene->cameraBokehDistribution,
				cscene->cameraBokehDistributionSize, "CameraBokehDistribution");
	else
		intersectionDevice->FreeBuffer(&cameraBokehDistributionBuff);
}

void PathOCLBaseOCLRenderThread::InitGeometry() {
	CompiledScene *cscene = renderEngine->compiledScene;

	const BufferType memTypeFlags = renderEngine->ctx->GetUseOutOfCoreBuffers() ?
		((BufferType)(BUFFER_TYPE_READ_ONLY | BUFFER_TYPE_OUT_OF_CORE)) :
		BUFFER_TYPE_READ_ONLY;

	if (cscene->normals.size() > 0)
		intersectionDevice->AllocBuffer(&normalsBuff,
				memTypeFlags,
				&cscene->normals[0],
				sizeof(Normal) * cscene->normals.size(), "Normals");
	else
		intersectionDevice->FreeBuffer(&normalsBuff);

	if (cscene->uvs.size() > 0)
		intersectionDevice->AllocBuffer(&uvsBuff,
				memTypeFlags,
				&cscene->uvs[0],
				sizeof(UV) * cscene->uvs.size(), "UVs");
	else
		intersectionDevice->FreeBuffer(&uvsBuff);

	if (cscene->cols.size() > 0)
		intersectionDevice->AllocBuffer(&colsBuff,
				memTypeFlags,
				&cscene->cols[0],
				sizeof(Spectrum) * cscene->cols.size(), "Colors");
	else
		intersectionDevice->FreeBuffer(&colsBuff);

	if (cscene->alphas.size() > 0)
		intersectionDevice->AllocBuffer(&alphasBuff,
				memTypeFlags,
				&cscene->alphas[0],
				sizeof(float) * cscene->alphas.size(), "Alphas");
	else
		intersectionDevice->FreeBuffer(&alphasBuff);

	if (cscene->vertexAOVs.size() > 0)
		intersectionDevice->AllocBuffer(&vertexAOVBuff,
				memTypeFlags,
				&cscene->vertexAOVs[0],
				sizeof(float) * cscene->vertexAOVs.size(), "Vertex AOVs");
	else
		intersectionDevice->FreeBuffer(&vertexAOVBuff);

	if (cscene->triAOVs.size() > 0)
		intersectionDevice->AllocBuffer(&triAOVBuff,
				memTypeFlags,
				&cscene->triAOVs[0],
				sizeof(float) * cscene->triAOVs.size(), "Triangle AOVs");
	else
		intersectionDevice->FreeBuffer(&triAOVBuff);

	intersectionDevice->AllocBuffer(&triNormalsBuff,
			memTypeFlags,
			&cscene->triNormals[0],
			sizeof(Normal) * cscene->triNormals.size(), "Triangle normals");

	intersectionDevice->AllocBuffer(&vertsBuff,
			memTypeFlags,
			&cscene->verts[0],
			sizeof(Point) * cscene->verts.size(), "Vertices");

	intersectionDevice->AllocBuffer(&trianglesBuff,
			memTypeFlags,
			&cscene->tris[0],
			sizeof(Triangle) * cscene->tris.size(), "Triangles");

	if (cscene->interpolatedTransforms.size() > 0) {
		intersectionDevice->AllocBuffer(&interpolatedTransformsBuff,
				memTypeFlags,
				&cscene->interpolatedTransforms[0],
				sizeof(luxrays::ocl::InterpolatedTransform) * cscene->interpolatedTransforms.size(), "Interpolated transformations");
	} else
		intersectionDevice->FreeBuffer(&interpolatedTransformsBuff);

	intersectionDevice->AllocBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
			sizeof(slg::ocl::ExtMesh) * cscene->meshDescs.size(), "Mesh description");
}

void PathOCLBaseOCLRenderThread::InitMaterials() {
	const size_t materialsCount = renderEngine->compiledScene->mats.size();
	intersectionDevice->AllocBufferRO(&materialsBuff, &renderEngine->compiledScene->mats[0],
			sizeof(slg::ocl::Material) * materialsCount, "Materials");

	intersectionDevice->AllocBufferRO(&materialEvalOpsBuff, &renderEngine->compiledScene->matEvalOps[0],
			sizeof(slg::ocl::MaterialEvalOp) * renderEngine->compiledScene->matEvalOps.size(), "Material evaluation ops");

	const u_int taskCount = renderEngine->taskCount;
	intersectionDevice->AllocBufferRW(&materialEvalStackBuff, 
			nullptr, sizeof(float) * renderEngine->compiledScene->maxMaterialEvalStackSize *
			taskCount, "Material evaluation stacks");

}

void PathOCLBaseOCLRenderThread::InitSceneObjects() {
	const BufferType memTypeFlags = renderEngine->ctx->GetUseOutOfCoreBuffers() ?
		((BufferType)(BUFFER_TYPE_READ_ONLY | BUFFER_TYPE_OUT_OF_CORE)) :
		BUFFER_TYPE_READ_ONLY;

	const u_int sceneObjsCount = renderEngine->compiledScene->sceneObjs.size();
	intersectionDevice->AllocBuffer(&scnObjsBuff, memTypeFlags,
			&renderEngine->compiledScene->sceneObjs[0],
			sizeof(slg::ocl::SceneObject) * sceneObjsCount, "Scene objects");
}

void PathOCLBaseOCLRenderThread::InitTextures() {
	const size_t texturesCount = renderEngine->compiledScene->texs.size();
	intersectionDevice->AllocBufferRO(&texturesBuff, &renderEngine->compiledScene->texs[0],
			sizeof(slg::ocl::Texture) * texturesCount, "Textures");

	intersectionDevice->AllocBufferRO(&textureEvalOpsBuff, &renderEngine->compiledScene->texEvalOps[0],
			sizeof(slg::ocl::TextureEvalOp) * renderEngine->compiledScene->texEvalOps.size(), "Texture evaluation ops");

	const u_int taskCount = renderEngine->taskCount;
	intersectionDevice->AllocBufferRW(&textureEvalStackBuff, 
			nullptr, sizeof(float) * renderEngine->compiledScene->maxTextureEvalStackSize *
			taskCount, "Texture evaluation stacks");
}

void PathOCLBaseOCLRenderThread::InitLights() {
	CompiledScene *cscene = renderEngine->compiledScene;

	intersectionDevice->AllocBufferRO(&lightsBuff, &cscene->lightDefs[0],
		sizeof(slg::ocl::LightSource) * cscene->lightDefs.size(), "Lights");
	if (cscene->envLightIndices.size() > 0) {
		intersectionDevice->AllocBufferRO(&envLightIndicesBuff, &cscene->envLightIndices[0],
				sizeof(u_int) * cscene->envLightIndices.size(), "Env. light indices");
	} else
		intersectionDevice->FreeBuffer(&envLightIndicesBuff);

	intersectionDevice->AllocBufferRO(&lightIndexOffsetByMeshIndexBuff, &cscene->lightIndexOffsetByMeshIndex[0],
		sizeof(u_int) * cscene->lightIndexOffsetByMeshIndex.size(), "Light offsets (Part I)");
	intersectionDevice->AllocBufferRO(&lightIndexByTriIndexBuff, &cscene->lightIndexByTriIndex[0],
		sizeof(u_int) * cscene->lightIndexByTriIndex.size(), "Light offsets (Part II)");

	if (cscene->envLightDistributions.size() > 0) {
		intersectionDevice->AllocBufferRO(&envLightDistributionsBuff, &cscene->envLightDistributions[0],
			sizeof(float) * cscene->envLightDistributions.size(), "Env. light distributions");
	} else
		intersectionDevice->FreeBuffer(&envLightDistributionsBuff);

	intersectionDevice->AllocBufferRO(&lightsDistributionBuff, cscene->lightsDistribution,
		cscene->lightsDistributionSize, "LightsDistribution");
	intersectionDevice->AllocBufferRO(&infiniteLightSourcesDistributionBuff, cscene->infiniteLightSourcesDistribution,
		cscene->infiniteLightSourcesDistributionSize, "InfiniteLightSourcesDistribution");
	if (cscene->dlscAllEntries.size() > 0) {
		intersectionDevice->AllocBufferRO(&dlscAllEntriesBuff, &cscene->dlscAllEntries[0],
			cscene->dlscAllEntries.size() * sizeof(slg::ocl::DLSCacheEntry), "DLSC all entries");
		intersectionDevice->AllocBufferRO(&dlscDistributionsBuff, &cscene->dlscDistributions[0],
			cscene->dlscDistributions.size() * sizeof(float), "DLSC distributions table");
		intersectionDevice->AllocBufferRO(&dlscBVHNodesBuff, &cscene->dlscBVHArrayNode[0],
			cscene->dlscBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "DLSC BVH nodes");
	} else {
		intersectionDevice->FreeBuffer(&dlscAllEntriesBuff);
		intersectionDevice->FreeBuffer(&dlscDistributionsBuff);
		intersectionDevice->FreeBuffer(&dlscBVHNodesBuff);
	}
	
	if (cscene->elvcAllEntries.size() > 0) {
		intersectionDevice->AllocBufferRO(&elvcAllEntriesBuff, &cscene->elvcAllEntries[0],
			cscene->elvcAllEntries.size() * sizeof(slg::ocl::ELVCacheEntry), "ELVC all entries");
		intersectionDevice->AllocBufferRO(&elvcDistributionsBuff, &cscene->elvcDistributions[0],
			cscene->elvcDistributions.size() * sizeof(float), "ELVC distributions table");
		if (cscene->elvcTileDistributionOffsets.size() > 0) {
			intersectionDevice->AllocBufferRO(&elvcTileDistributionOffsetsBuff, &cscene->elvcTileDistributionOffsets[0],
					cscene->elvcTileDistributionOffsets.size() * sizeof(u_int), "ELVC tile distribution offsets table");
		} else
			intersectionDevice->FreeBuffer(&elvcTileDistributionOffsetsBuff);
		intersectionDevice->AllocBufferRO(&elvcBVHNodesBuff, &cscene->elvcBVHArrayNode[0],
			cscene->elvcBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "ELVC BVH nodes");
	} else {
		intersectionDevice->FreeBuffer(&elvcAllEntriesBuff);
		intersectionDevice->FreeBuffer(&elvcDistributionsBuff);
		intersectionDevice->FreeBuffer(&elvcTileDistributionOffsetsBuff);
		intersectionDevice->FreeBuffer(&elvcBVHNodesBuff);
	}
}

void PathOCLBaseOCLRenderThread::InitPhotonGI() {
	CompiledScene *cscene = renderEngine->compiledScene;

	const BufferType memTypeFlags = renderEngine->ctx->GetUseOutOfCoreBuffers() ?
		((BufferType)(BUFFER_TYPE_READ_ONLY | BUFFER_TYPE_OUT_OF_CORE)) :
		BUFFER_TYPE_READ_ONLY;

	if (cscene->pgicRadiancePhotons.size() > 0) {
		intersectionDevice->AllocBuffer(&pgicRadiancePhotonsBuff, memTypeFlags, &cscene->pgicRadiancePhotons[0],
			cscene->pgicRadiancePhotons.size() * sizeof(slg::ocl::RadiancePhoton), "PhotonGI indirect cache all entries");
		intersectionDevice->AllocBuffer(&pgicRadiancePhotonsValuesBuff, memTypeFlags, &cscene->pgicRadiancePhotonsValues[0],
			cscene->pgicRadiancePhotonsValues.size() * sizeof(slg::ocl::Spectrum), "PhotonGI indirect cache all entry values");
		intersectionDevice->AllocBuffer(&pgicRadiancePhotonsBVHNodesBuff, memTypeFlags, &cscene->pgicRadiancePhotonsBVHArrayNode[0],
			cscene->pgicRadiancePhotonsBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "PhotonGI indirect cache BVH nodes");
	} else {
		intersectionDevice->FreeBuffer(&pgicRadiancePhotonsBuff);
		intersectionDevice->FreeBuffer(&pgicRadiancePhotonsValuesBuff);
		intersectionDevice->FreeBuffer(&pgicRadiancePhotonsBVHNodesBuff);
	}

	if (cscene->pgicCausticPhotons.size() > 0) {
		intersectionDevice->AllocBuffer(&pgicCausticPhotonsBuff, memTypeFlags, &cscene->pgicCausticPhotons[0],
			cscene->pgicCausticPhotons.size() * sizeof(slg::ocl::Photon), "PhotonGI caustic cache all entries");
		intersectionDevice->AllocBuffer(&pgicCausticPhotonsBVHNodesBuff, memTypeFlags, &cscene->pgicCausticPhotonsBVHArrayNode[0],
			cscene->pgicCausticPhotonsBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "PhotonGI caustic cache BVH nodes");
	} else {
		intersectionDevice->FreeBuffer(&pgicCausticPhotonsBuff);
		intersectionDevice->FreeBuffer(&pgicCausticPhotonsBVHNodesBuff);
	}
}

void PathOCLBaseOCLRenderThread::InitImageMaps() {
	CompiledScene *cscene = renderEngine->compiledScene;

	if (cscene->imageMapDescs.size() > 0) {
		intersectionDevice->AllocBufferRO(&imageMapDescsBuff,
				&cscene->imageMapDescs[0],
				sizeof(slg::ocl::ImageMap) * cscene->imageMapDescs.size(), "ImageMap descriptions");

		// Free unused pages
		for (u_int i = cscene->imageMapMemBlocks.size(); i < imageMapsBuff.size(); ++i)
			intersectionDevice->FreeBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(cscene->imageMapMemBlocks.size(), NULL);

		const BufferType memTypeFlags = renderEngine->ctx->GetUseOutOfCoreBuffers() ?
			((BufferType)(BUFFER_TYPE_READ_ONLY | BUFFER_TYPE_OUT_OF_CORE)) :
			BUFFER_TYPE_READ_ONLY;

		for (u_int i = 0; i < imageMapsBuff.size(); ++i) {
			intersectionDevice->AllocBuffer(&(imageMapsBuff[i]),
					memTypeFlags,
					&(cscene->imageMapMemBlocks[i][0]),
					sizeof(float) * cscene->imageMapMemBlocks[i].size(), "ImageMaps");
		}
	} else {
		intersectionDevice->FreeBuffer(&imageMapDescsBuff);
		for (u_int i = 0; i < imageMapsBuff.size(); ++i)
			intersectionDevice->FreeBuffer(&imageMapsBuff[i]);
		imageMapsBuff.resize(0);
	}
}

void PathOCLBaseOCLRenderThread::InitGPUTaskBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	//--------------------------------------------------------------------------
	// Allocate tasksConfigBuff
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRO(&taskConfigBuff, &renderEngine->taskConfig, sizeof(slg::ocl::pathoclbase::GPUTaskConfiguration), "GPUTaskConfiguration");

	//--------------------------------------------------------------------------
	// Allocate tasksBuff
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRW(&tasksBuff, nullptr, sizeof(slg::ocl::pathoclbase::GPUTask) * taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate tasksDirectLightBuff
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRW(&tasksDirectLightBuff, nullptr, sizeof(slg::ocl::pathoclbase::GPUTaskDirectLight) * taskCount, "GPUTaskDirectLight");

	//--------------------------------------------------------------------------
	// Allocate tasksStateBuff
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRW(&tasksStateBuff, nullptr, sizeof(slg::ocl::pathoclbase::GPUTaskState) * taskCount, "GPUTaskState");
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
		intersectionDevice->FreeBuffer(&samplerSharedDataBuff);
	else
		intersectionDevice->AllocBufferRW(&samplerSharedDataBuff, nullptr, size, "SamplerSharedData");

	// Initialize the sampler shared data
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		slg::ocl::RandomSamplerSharedData rssd;
		rssd.bucketIndex = 0;

		intersectionDevice->EnqueueWriteBuffer(samplerSharedDataBuff, CL_TRUE, size, &rssd);
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		char *buffer = new char[size];

		// Initialize SobolSamplerSharedData fields
		slg::ocl::SobolSamplerSharedData *sssd = (slg::ocl::SobolSamplerSharedData *)buffer;

		sssd->seedBase = renderEngine->seedBase;
		sssd->bucketIndex = 0;
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
		intersectionDevice->EnqueueWriteBuffer(samplerSharedDataBuff, CL_TRUE, size, buffer);
		
		delete[] buffer;
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		// TilePathSamplerSharedData is updated in PathOCLBaseOCLRenderThread::UpdateSamplerData()
		
		switch (renderEngine->GetType()) {
			case TILEPATHOCL: {
				char *buffer = new char[size];

				// Initialize the Sobol directions array values
				u_int *sobolDirections = (u_int *)(buffer + sizeof(slg::ocl::TilePathSamplerSharedData));
				SobolSequence::GenerateDirectionVectors(sobolDirections, renderEngine->pathTracer.eyeSampleSize);

				intersectionDevice->EnqueueWriteBuffer(samplerSharedDataBuff, CL_TRUE, size, &buffer[0]);
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
	intersectionDevice->AllocBufferRW(&samplesBuff, nullptr, sampleSize * taskCount, "Sample");
}

void PathOCLBaseOCLRenderThread::InitSampleResultsBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	const size_t sampleResultSize = sizeof(slg::ocl::SampleResult);

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a SampleResult: " << sampleResultSize << "bytes");
	intersectionDevice->AllocBufferRW(&sampleResultsBuff, nullptr, sampleResultSize * taskCount, "Sample");
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

	intersectionDevice->AllocBufferRW(&sampleDataBuff, nullptr, uDataSize * taskCount, "SampleData");
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

	intersectionDevice->AllocBufferRW(&raysBuff, nullptr, sizeof(Ray) * taskCount, "Ray");
	intersectionDevice->AllocBufferRW(&hitsBuff, nullptr, sizeof(RayHit) * taskCount, "RayHit");

	//--------------------------------------------------------------------------
	// Allocate GPU task buffers
	//--------------------------------------------------------------------------

	InitGPUTaskBuffer();

	//--------------------------------------------------------------------------
	// Allocate GPU task statistic buffers
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRW(&taskStatsBuff, nullptr, sizeof(slg::ocl::pathoclbase::GPUTaskStats) * taskCount, "GPUTask Stats");

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

	intersectionDevice->AllocBufferRW(&eyePathInfosBuff, nullptr, sizeof(slg::ocl::EyePathInfo) * taskCount, "PathInfo");

	//--------------------------------------------------------------------------
	// Allocate volume info buffers if required
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRW(&directLightVolInfosBuff, nullptr, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "DirectLightVolumeInfo");

	//--------------------------------------------------------------------------
	// Allocate GPU pixel filter distribution
	//--------------------------------------------------------------------------

	intersectionDevice->AllocBufferRO(&pixelFilterBuff, renderEngine->pixelFilterDistribution,
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

	// Clear all thread films
	BOOST_FOREACH(ThreadFilm *threadFilm, threadFilms) {
		intersectionDevice->PushThreadCurrentDevice();
		threadFilm->ClearFilm(intersectionDevice, filmClearKernel, filmClearWorkGroupSize);
		intersectionDevice->PopThreadCurrentDevice();
	}

	intersectionDevice->FinishQueue();

	// Reset statistics in order to be more accurate
	intersectionDevice->ResetPerformaceStats();
}

#endif

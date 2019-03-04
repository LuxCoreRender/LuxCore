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

#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/ocl.h"
#include "luxrays/core/oclintersectiondevice.h"
#include "luxrays/kernels/kernels.h"

#include "luxcore/cfg.h"

#include "slg/slg.h"
#include "slg/kernels/kernels.h"
#include "slg/renderconfig.h"
#include "slg/engines/pathoclbase/pathoclbase.h"
#include "slg/samplers/sobol.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// PathOCLBaseOCLRenderThread initialization methods
//------------------------------------------------------------------------------

size_t PathOCLBaseOCLRenderThread::GetOpenCLHitPointSize() const {
	// HitPoint memory size
	size_t hitPointSize = sizeof(Vector) + sizeof(Point) + sizeof(UV) +
			2 * sizeof(Normal) + sizeof(Matrix4x4);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTCOLOR) ||
			renderEngine->compiledScene->IsTextureCompiled(HITPOINTGREY) ||
			renderEngine->compiledScene->hasTriangleLightWithVertexColors)
		hitPointSize += sizeof(Spectrum);
	if (renderEngine->compiledScene->IsTextureCompiled(HITPOINTALPHA))
		hitPointSize += sizeof(float);
	if (renderEngine->compiledScene->RequiresPassThrough())
		hitPointSize += sizeof(float);
	// Fields dpdu, dpdv, dndu, dndv
	if (renderEngine->compiledScene->HasBumpMaps())
		hitPointSize += 2 * sizeof(Vector) + 2 * sizeof(Normal);
	// Volume fields
	if (renderEngine->compiledScene->HasVolumes())
		hitPointSize += 2 * sizeof(u_int) + 2 * sizeof(u_int);
	// intoObject
	hitPointSize += sizeof(int);
	// Object ID
	if (renderEngine->compiledScene->IsTextureCompiled(OBJECTID_TEX) ||
			renderEngine->compiledScene->IsTextureCompiled(OBJECTID_COLOR_TEX) ||
			renderEngine->compiledScene->IsTextureCompiled(OBJECTID_NORMALIZED_TEX))
		hitPointSize += sizeof(u_int);

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
	if (renderEngine->compiledScene->HasVolumes())
		bsdfSize += sizeof(int);

	return bsdfSize;
}

size_t PathOCLBaseOCLRenderThread::GetOpenCLSampleResultSize() const {
	//--------------------------------------------------------------------------
	// SampleResult size
	//--------------------------------------------------------------------------

	// All thread films are supposed to have the same parameters
	const Film *threadFilm = threadFilms[0]->film;

	// SampleResult.filmX and SampleResult.filmY
	size_t sampleResultSize = 2 * sizeof(float);
	// SampleResult.radiancePerPixelNormalized[PARAM_FILM_RADIANCE_GROUP_COUNT]
	sampleResultSize += sizeof(slg::ocl::Spectrum) * threadFilm->GetRadianceGroupCount();
	if (threadFilm->HasChannel(Film::ALPHA))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::DEPTH))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::POSITION))
		sampleResultSize += sizeof(Point);
	if (threadFilm->HasChannel(Film::GEOMETRY_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (threadFilm->HasChannel(Film::SHADING_NORMAL) || threadFilm->HasChannel(Film::AVG_SHADING_NORMAL))
		sampleResultSize += sizeof(Normal);
	if (threadFilm->HasChannel(Film::MATERIAL_ID) ||
			threadFilm->HasChannel(Film::MATERIAL_ID_MASK) ||
			threadFilm->HasChannel(Film::BY_MATERIAL_ID) ||
			threadFilm->HasChannel(Film::MATERIAL_ID_COLOR))
		sampleResultSize += sizeof(u_int);
	if (threadFilm->HasChannel(Film::OBJECT_ID))
		sampleResultSize += sizeof(u_int);
	if (threadFilm->HasChannel(Film::DIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::DIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::EMISSION))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_DIFFUSE))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_GLOSSY))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::INDIRECT_SPECULAR))
		sampleResultSize += sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::DIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::INDIRECT_SHADOW_MASK))
		sampleResultSize += sizeof(float);
	if (threadFilm->HasChannel(Film::UV))
		sampleResultSize += sizeof(UV);
	if (threadFilm->HasChannel(Film::RAYCOUNT))
		sampleResultSize += sizeof(Film::RAYCOUNT);
	if (threadFilm->HasChannel(Film::IRRADIANCE))
		sampleResultSize += 2 * sizeof(Spectrum);
	if (threadFilm->HasChannel(Film::ALBEDO))
		sampleResultSize += sizeof(Spectrum);

	sampleResultSize += sizeof(BSDFEvent) +
			3 * sizeof(int) +
			// pixelX and pixelY fields
			sizeof(u_int) * 2;

	return sampleResultSize;
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

	AllocOCLBufferRO(&vertsBuff, &cscene->verts[0],
		sizeof(Point) * cscene->verts.size(), "Vertices");

	AllocOCLBufferRO(&trianglesBuff, &cscene->tris[0],
		sizeof(Triangle) * cscene->tris.size(), "Triangles");

	AllocOCLBufferRO(&meshDescsBuff, &cscene->meshDescs[0],
			sizeof(slg::ocl::Mesh) * cscene->meshDescs.size(), "Mesh description");
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
		AllocOCLBufferRO(&dlscDistributionIndexToLightIndexBuff, &cscene->dlscDistributionIndexToLightIndex[0],
			cscene->dlscDistributionIndexToLightIndex.size() * sizeof(u_int), "DLSC indices table");
		AllocOCLBufferRO(&dlscDistributionsBuff, &cscene->dlscDistributions[0],
			cscene->dlscDistributions.size() * sizeof(float), "DLSC indices table");
		AllocOCLBufferRO(&dlscBVHNodesBuff, &cscene->dlscBVHArrayNode[0],
			cscene->dlscBVHArrayNode.size() * sizeof(slg::ocl::IndexBVHArrayNode), "DLSC BVH nodes");
	} else {
		FreeOCLBuffer(&dlscAllEntriesBuff);
		FreeOCLBuffer(&dlscDistributionIndexToLightIndexBuff);
		FreeOCLBuffer(&dlscDistributionsBuff);
		FreeOCLBuffer(&dlscBVHNodesBuff);
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
		AllocOCLBufferRW(&pgicCausticNearPhotonsBuff,
			renderEngine->taskCount * cscene->pgicCausticLookUpMaxCount * sizeof(slg::ocl::NearPhoton), "PhotonGI near photon buffers");
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
	const bool hasPassThrough = renderEngine->compiledScene->RequiresPassThrough();
	const size_t openCLBSDFSize = GetOpenCLBSDFSize();

	//--------------------------------------------------------------------------
	// Allocate tasksBuff
	//--------------------------------------------------------------------------
	
	// Add Seed memory size
	size_t gpuTaskSize = sizeof(slg::ocl::Seed);

	// Add temporary storage space
	
	// Add tmpBsdf memory size
	if (hasPassThrough || renderEngine->compiledScene->HasVolumes())
		gpuTaskSize += openCLBSDFSize;

	// Add tmpHitPoint memory size
	gpuTaskSize += GetOpenCLHitPointSize();

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a GPUTask: " << gpuTaskSize << "bytes");
	AllocOCLBufferRW(&tasksBuff, gpuTaskSize * taskCount, "GPUTask");

	//--------------------------------------------------------------------------
	// Allocate tasksDirectLightBuff
	//--------------------------------------------------------------------------

	size_t gpuDirectLightTaskSize = 
			sizeof(slg::ocl::pathoclbase::DirectLightIlluminateInfo) + 
			sizeof(BSDFEvent) + // lastBSDFEvent
			sizeof(float) + // lastPdfW
			sizeof(float) + // lastGlossiness
			sizeof(Normal) + // lastNormal
			(renderEngine->compiledScene->HasVolumes() ? sizeof(int) : 0) + // lastIsVolume
			sizeof(int); // isLightVisible

	// Add seedPassThroughEvent memory size
	if (hasPassThrough)
		gpuDirectLightTaskSize += sizeof(ocl::Seed);

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a GPUTask DirectLight: " << gpuDirectLightTaskSize << "bytes");
	AllocOCLBufferRW(&tasksDirectLightBuff, gpuDirectLightTaskSize * taskCount, "GPUTaskDirectLight");

	//--------------------------------------------------------------------------
	// Allocate tasksStateBuff
	//--------------------------------------------------------------------------

	size_t gpuTaksStateSize =
			sizeof(int) + // state
			sizeof(slg::ocl::PathDepthInfo) + // depthInfo
			sizeof(Spectrum) + // throughput
			sizeof(int) + // albedoToDo
			sizeof(int) + // photonGICausticCacheAlreadyUsed
			sizeof(int); // photonGICacheEnabledOnLastHit
	// Add seedPassThroughEvent memory size
	if (hasPassThrough)
		gpuTaksStateSize += sizeof(ocl::Seed);

	// Add BSDF memory size
	gpuTaksStateSize += openCLBSDFSize;

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a GPUTask State: " << gpuTaksStateSize << "bytes");
	AllocOCLBufferRW(&tasksStateBuff, gpuTaksStateSize * taskCount, "GPUTaskState");
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

		// Plus the a pass field for each buckets
		//
		// The "+ SOBOL_OCL_WORK_SIZE - 1" is there to obtain the ceiling of the division
		size += sizeof(u_int) * ((filmRegionPixelCount + SOBOL_OCL_WORK_SIZE - 1) / SOBOL_OCL_WORK_SIZE);
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		switch (renderEngine->GetType()) {
			case TILEPATHOCL:
				size += sizeof(slg::ocl::TilePathSamplerSharedData) * filmRegionPixelCount;
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
	
	AllocOCLBufferRW(&samplerSharedDataBuff, size, "SamplerSharedData");

	// Initialize the sampler shared data
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		slg::ocl::RandomSamplerSharedData rssd;
		rssd.pixelBucketIndex = 0; // Initialized by OpenCL kernel
		rssd.adaptiveStrength = renderEngine->oclSampler->random.adaptiveStrength;
		
		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		oclQueue.enqueueWriteBuffer(*samplerSharedDataBuff, CL_TRUE, 0, size, &rssd);
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		char *buffer = new char[size];

		// Initialize SobolSamplerSharedData fields
		slg::ocl::SobolSamplerSharedData *sssd = (slg::ocl::SobolSamplerSharedData *)buffer;

		sssd->seedBase = renderEngine->seedBase;
		sssd->pixelBucketIndex = 0; // Initialized by OpenCL kernel
		sssd->adaptiveStrength = renderEngine->oclSampler->sobol.adaptiveStrength;

		cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
		oclQueue.enqueueWriteBuffer(*samplerSharedDataBuff, CL_TRUE, 0, size, buffer);

		delete[] buffer;
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		switch (renderEngine->GetType()) {
			case TILEPATHOCL: {
				// rngPass, rng0 and rng1 fields
				slg::ocl::TilePathSamplerSharedData *buffer = new slg::ocl::TilePathSamplerSharedData[filmRegionPixelCount];

				RandomGenerator rndGen(renderEngine->seedBase);

				for (u_int i = 0; i < filmRegionPixelCount; ++i) {
					buffer[i].rngPass = rndGen.uintValue();
					buffer[i].rng0 = rndGen.floatValue();
					buffer[i].rng1 = rndGen.floatValue();
				}

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

// Used only by TILEPATHOCL
void PathOCLBaseOCLRenderThread::UpdateSamplerSharedDataBuffer(const TileWork &tileWork) {
	if (renderEngine->oclSampler->type != slg::ocl::TILEPATHSAMPLER)
		throw runtime_error("Wrong sampler in PathOCLBaseRenderThread::UpdateSamplerSharedDataBuffer(): " +
						boost::lexical_cast<string>(renderEngine->oclSampler->type));

	switch (renderEngine->GetType()) {
			case TILEPATHOCL: {
				const u_int *subRegion = renderEngine->film->GetSubRegion();
				const u_int filmRegionPixelCount = (subRegion[1] - subRegion[0] + 1) * (subRegion[3] - subRegion[2] + 1);

				// rngPass, rng0 and rng1 fields
				vector<slg::ocl::TilePathSamplerSharedData> buffer(filmRegionPixelCount);

				RandomGenerator rndGen(tileWork.GetTileSeed());

				for (u_int i = 0; i < filmRegionPixelCount; ++i) {
					buffer[i].rngPass = rndGen.uintValue();
					buffer[i].rng0 = rndGen.floatValue();
					buffer[i].rng1 = rndGen.floatValue();
				}

				cl::CommandQueue &oclQueue = intersectionDevice->GetOpenCLQueue();
				oclQueue.enqueueWriteBuffer(*samplerSharedDataBuff, CL_TRUE, 0,
						sizeof(slg::ocl::TilePathSamplerSharedData) * buffer.size(), &buffer[0]);
				break;
			}
			case RTPATHOCL:
				break;
			default:
				throw runtime_error("Unknown render engine in PathOCLBaseRenderThread::UpdateSamplerSharedDataBuffer(): " +
						boost::lexical_cast<string>(renderEngine->GetType()));
		}
}

void PathOCLBaseOCLRenderThread::InitSamplesBuffer() {
	const u_int taskCount = renderEngine->taskCount;

	//--------------------------------------------------------------------------
	// SampleResult size
	//--------------------------------------------------------------------------

	const size_t sampleResultSize = GetOpenCLSampleResultSize();

	//--------------------------------------------------------------------------
	// Sample size
	//--------------------------------------------------------------------------

	size_t sampleSize = sampleResultSize;

	// Add Sample memory size
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		// pixelIndexBase, pixelIndexOffset and pixelIndexRandomStart fields
		sampleSize += 3 * sizeof(unsigned int);
	} else if (renderEngine->oclSampler->type == slg::ocl::METROPOLIS) {
		sampleSize += 2 * sizeof(float) + 5 * sizeof(u_int) + sampleResultSize;		
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		// pixelIndexBase, pixelIndexOffset, pass and pixelIndexRandomStart fields
		sampleSize += 4 * sizeof(u_int);
		// Seed field
		sampleSize += sizeof(slg::ocl::Seed);
		// rngPass field
		sampleSize += sizeof(u_int);
		// rng0 and rng1 fields
		sampleSize += 2 * sizeof(float);
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		// rngPass field
		sampleSize += sizeof(u_int);
		// rng0 and rng1 fields
		sampleSize += 2 * sizeof(float);
		// pass field
		sampleSize += sizeof(u_int);
	} else
		throw runtime_error("Unknown sampler.type in PathOCLBaseRenderThread::InitSamplesBuffer(): " +
				boost::lexical_cast<string>(renderEngine->oclSampler->type));

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Size of a Sample: " << sampleSize << "bytes");
	AllocOCLBufferRW(&samplesBuff, sampleSize * taskCount, "Sample");
}

void PathOCLBaseOCLRenderThread::InitSampleDataBuffer() {
	const u_int taskCount = renderEngine->taskCount;
	const bool hasPassThrough = renderEngine->compiledScene->RequiresPassThrough();

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

	size_t uDataSize;
	if (renderEngine->oclSampler->type == slg::ocl::RANDOM) {
		sampleDimensions = 0;

		// To store IDX_SCREEN_X and IDX_SCREEN_Y
		uDataSize = 2 * sizeof(float);
	} else if (renderEngine->oclSampler->type == slg::ocl::SOBOL) {
		sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * Min<u_int>(renderEngine->pathTracer.maxPathDepth.depth, SOBOL_MAX_DEPTH);

		// To store IDX_SCREEN_X and IDX_SCREEN_Y
		uDataSize = 2 * sizeof(float);
	} else if (renderEngine->oclSampler->type == slg::ocl::METROPOLIS) {
		sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * renderEngine->pathTracer.maxPathDepth.depth;

		// Metropolis needs 2 sets of samples, the current and the proposed mutation
		uDataSize = 2 * sizeof(float) * sampleDimensions;
	} else if (renderEngine->oclSampler->type == slg::ocl::TILEPATHSAMPLER) {
		sampleDimensions = eyePathVertexDimension + PerPathVertexDimension * Min<u_int>(renderEngine->pathTracer.maxPathDepth.depth, SOBOL_MAX_DEPTH);

		// To store IDX_SCREEN_X and IDX_SCREEN_Y
		uDataSize = 2 * sizeof(float);
	} else
		throw runtime_error("Unknown sampler.type in PathOCLBaseRenderThread::InitSampleDataBuffer(): " + boost::lexical_cast<string>(renderEngine->oclSampler->type));

	SLG_LOG("[PathOCLBaseRenderThread::" << threadIndex << "] Sample dimensions: " << sampleDimensions);
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
	// Allocate volume info buffers if required
	//--------------------------------------------------------------------------

	if (renderEngine->compiledScene->HasVolumes()) {
		AllocOCLBufferRW(&pathVolInfosBuff, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "PathVolumeInfo");
		AllocOCLBufferRW(&directLightVolInfosBuff, sizeof(slg::ocl::PathVolumeInfo) * taskCount, "DirectLightVolumeInfo");
	}

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

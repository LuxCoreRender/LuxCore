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

#ifndef _SLG_COMPILEDSESSION_H
#define	_SLG_COMPILEDSESSION_H

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/unordered_set.hpp>

#include "slg/slg.h"
#include "slg/editaction.h"

#include "slg/core/indexbvh.h"
#include "slg/film/film.h"
#include "slg/scene/scene.h"
#include "slg/scene/sceneobject.h"
#include "slg/lights/strategies/dlscache.h"
#include "slg/lights/visibility/envlightvisibilitycache.h"
#include "slg/engines/pathtracer.h"

namespace slg {

class CompiledScene {
public:
	CompiledScene(Scene *scn, const PathTracer *pt);
	~CompiledScene();
	
	void SetMaxMemPageSize(const size_t maxSize);
	void EnableCode(const std::string &tags);

	void Compile();
	void Recompile(const EditActionList &editActions);
	void RecompilePhotonGI() { CompilePhotonGI(); }

	static void CompileFilm(const Film &film, slg::ocl::Film &oclFilm);

	static float *CompileDistribution1D(const luxrays::Distribution1D *dist, u_int *size);
	static float *CompileDistribution2D(const luxrays::Distribution2D *dist, u_int *size);

	static std::string ToOCLString(const slg::ocl::Spectrum &v);

	// Compiled Camera
	slg::ocl::Camera camera;
	float *cameraBokehDistribution;
	u_int cameraBokehDistributionSize;

	// Compiled Scene Meshes
	std::vector<luxrays::Point> verts;
	std::vector<luxrays::Normal> normals;
	std::vector<luxrays::Normal> triNormals;
	std::vector<luxrays::UV> uvs;
	std::vector<luxrays::Spectrum> cols;
	std::vector<float> alphas;
	std::vector<float> vertexAOVs;
	std::vector<float> triAOVs;
	std::vector<luxrays::Triangle> tris;
	std::vector<luxrays::ocl::InterpolatedTransform> interpolatedTransforms;
	std::vector<luxrays::ocl::ExtMesh> meshDescs;
	luxrays::BSphere worldBSphere;

	// Compiled Scene Objects
	std::vector<slg::ocl::SceneObject> sceneObjs;

	// Compiled Lights
	std::vector<slg::ocl::LightSource> lightDefs;
	// Additional light related information
	std::vector<u_int> envLightIndices;
	std::vector<u_int> lightIndexOffsetByMeshIndex, lightIndexByTriIndex;
	// Env. light Distribution2Ds
	std::vector<float> envLightDistributions;
	// Compiled light sampling strategy
	float *lightsDistribution;
	u_int lightsDistributionSize;
	float *infiniteLightSourcesDistribution;
	u_int infiniteLightSourcesDistributionSize;
	// DLSC related data
	std::vector<slg::ocl::DLSCacheEntry> dlscAllEntries;
	std::vector<float> dlscDistributions; 
	std::vector<luxrays::ocl::IndexBVHArrayNode> dlscBVHArrayNode;
	float dlscRadius2, dlscNormalCosAngle;
	// EnvLightVisibilityCache related data
	std::vector<slg::ocl::ELVCacheEntry> elvcAllEntries;
	std::vector<float> elvcDistributions;
	std::vector<u_int> elvcTileDistributionOffsets; 
	std::vector<luxrays::ocl::IndexBVHArrayNode> elvcBVHArrayNode;
	float elvcRadius2, elvcNormalCosAngle;
	u_int elvcTilesXCount, elvcTilesYCount;
	
	// Compiled Materials (and Volumes)
	std::vector<slg::ocl::Material> mats;
	std::vector<slg::ocl::MaterialEvalOp> matEvalOps;
	// Expressed in float
	u_int maxMaterialEvalStackSize;
	u_int defaultWorldVolumeIndex;

	// Compiled Textures
	std::vector<slg::ocl::Texture> texs;
	std::vector<slg::ocl::TextureEvalOp> texEvalOps;
	// Expressed in float
	u_int maxTextureEvalStackSize;

	// Compiled ImageMaps
	std::vector<slg::ocl::ImageMap> imageMapDescs;
	std::vector<std::vector<float> > imageMapMemBlocks;
	
	// Compiled PhotonGI cache

	// PhotonGI indirect cache
	std::vector<slg::ocl::RadiancePhoton> pgicRadiancePhotons;
	u_int pgicLightGroupCounts;
	std::vector<slg::ocl::Spectrum> pgicRadiancePhotonsValues;
	std::vector<luxrays::ocl::IndexBVHArrayNode> pgicRadiancePhotonsBVHArrayNode;
	// PhotonGI caustic cache
	std::vector<slg::ocl::Photon> pgicCausticPhotons;
	std::vector<luxrays::ocl::IndexBVHArrayNode> pgicCausticPhotonsBVHArrayNode;

	// All global settings
	slg::ocl::PathTracer compiledPathTracer;

	// Elements compiled during the last call to Compile()/Recompile()
	bool wasCameraCompiled, wasSceneObjectsCompiled, wasGeometryCompiled, 
		wasMaterialsCompiled, wasLightsCompiled, wasImageMapsCompiled,
		wasPhotonGICompiled;

private:
	void AddToImageMapMem(slg::ocl::ImageMap &im, void *data, const size_t memSize);
	u_int CompileImageMap(const ImageMap *im);

	void CompileCamera();
	void CompileSceneObjects();
	void CompileGeometry();
	u_int CompileMaterialConditionalOps(const u_int matIndex,
		const std::vector<slg::ocl::MaterialEvalOp> &evalOpsA, const u_int evalOpStackSizeA,
		const std::vector<slg::ocl::MaterialEvalOp> &evalOpsB, const u_int evalOpStackSizeB,
		std::vector<slg::ocl::MaterialEvalOp> &evalOps) const;
	u_int CompileMaterialConditionalOps(const u_int matIndex,
			const u_int matAIndex, const slg::ocl::MaterialEvalOpType opTypeA,
			const u_int matBIndex, const slg::ocl::MaterialEvalOpType opTypeB,
			std::vector<slg::ocl::MaterialEvalOp> &evalOps) const;
	u_int CompileMaterialOps(const u_int matIndex, const slg::ocl::MaterialEvalOpType opType,
			std::vector<slg::ocl::MaterialEvalOp> &evalOps) const;
	void CompileMaterialOps();
	void CompileMaterials();
	void CompileTextureMapping2D(slg::ocl::TextureMapping2D *mapping, const TextureMapping2D *m);
	void CompileTextureMapping3D(slg::ocl::TextureMapping3D *mapping, const TextureMapping3D *m);
	u_int CompileTextureOpsGenericBumpMap(const u_int texIndex);
	u_int CompileTextureOps(const u_int texIndex, const slg::ocl::TextureEvalOpType opType);
	void CompileTextureOps();
	void CompileTextures();
	void CompileImageMaps();
	void CompileLights();

	void CompileDLSC(const LightStrategyDLSCache *dlscLightStrategy);
	void CompileELVC(const EnvLightVisibilityCache *visibilityMapCache);
	void CompileLightStrategy();
	
	void CompilePhotonGI();
	void CompilePathTracer();

	Scene *scene;
	const PathTracer *pathTracer;

	size_t maxMemPageSize;
	boost::unordered_set<std::string> enabledCode;
}; 

}

#endif

#endif	/* _SLG_COMPILEDSESSION_H */

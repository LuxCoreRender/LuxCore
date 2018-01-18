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

#ifndef _LUXCOREIMPL_H
#define	_LUXCOREIMPL_H

#include <luxcore/luxcore.h>
#include <slg/renderconfig.h>
#include <slg/rendersession.h>
#include <slg/renderstate.h>
#include <slg/scene/scene.h>
#include <slg/film/film.h>

namespace luxcore {
namespace detail {

//------------------------------------------------------------------------------
// FilmImpl
//------------------------------------------------------------------------------

class RenderSessionImpl;

class FilmImpl : public Film {
public:
	FilmImpl(const std::string &fileName);
	FilmImpl(const luxrays::Properties &props,
		const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel);
	FilmImpl(const RenderSessionImpl &session);
	FilmImpl(slg::Film *film);
	~FilmImpl();

	unsigned int GetWidth() const;
	unsigned int GetHeight() const;
	luxrays::Properties GetStats() const;

	void Clear();
	void AddFilm(const Film &film);
	void AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY);

	void SaveOutputs() const;
	void SaveOutput(const std::string &fileName, const FilmOutputType type, const luxrays::Properties &props) const;
	void SaveFilm(const std::string &fileName) const;

	double GetTotalSampleCount() const;

	size_t GetOutputSize(const FilmOutputType type) const;
	bool HasOutput(const FilmOutputType type) const;
	
	unsigned int GetRadianceGroupCount() const;
	unsigned int GetChannelCount(const FilmChannelType type) const;

	void GetOutputFloat(const FilmOutputType type, float *buffer, const unsigned int index);
	void GetOutputUInt(const FilmOutputType type, unsigned int *buffer, const unsigned int index);
	
	const float *GetChannelFloat(const FilmChannelType type, const unsigned int index);
	const unsigned int *GetChannelUInt(const FilmChannelType type, const unsigned int index);

	void Parse(const luxrays::Properties &props);

	friend class RenderSessionImpl;

private:
	slg::Film *GetSLGFilm() const;

	const RenderSessionImpl *renderSession;
	slg::Film *standAloneFilm;
};

//------------------------------------------------------------------------------
// CameraImpl
//------------------------------------------------------------------------------

class SceneImpl;

class CameraImpl : public Camera {
public:
	CameraImpl(const SceneImpl &scene);
	~CameraImpl();

	const CameraType GetType() const;

	void Translate(const float x, const float y, const float z) const;
	void TranslateLeft(const float t) const;
	void TranslateRight(const float t) const;
	void TranslateForward(const float t) const;
	void TranslateBackward(const float t) const;

	void Rotate(const float angle, const float x, const float y, const float z) const;
	void RotateLeft(const float angle) const;
	void RotateRight(const float angle) const;
	void RotateUp(const float angle) const;
	void RotateDown(const float angle) const;

	friend class SceneImpl;

private:
	const SceneImpl &scene;
};

//------------------------------------------------------------------------------
// SceneImpl
//------------------------------------------------------------------------------

class SceneImpl : public Scene {
public:	
	SceneImpl(slg::Scene *scn);
	SceneImpl(const float imageScale = 1.f);
	SceneImpl(const luxrays::Properties &props, const float imageScale = 1.f);
	SceneImpl(const std::string &fileName, const float imageScale = 1.f);
	~SceneImpl();
	
	void GetBBox(float min[3], float max[3]) const;
	const Camera &GetCamera() const;

	bool IsImageMapDefined(const std::string &imgMapName) const;

	void SetDeleteMeshData(const bool v);

	void DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n, float *uv,
		float *cols, float *alphas);

	void SaveMesh(const std::string &meshName, const std::string &fileName);
	void DefineStrands(const std::string &shapeName, const luxrays::cyHairFile &strandsFile,
		const StrandsTessellationType tesselType,
		const unsigned int adaptiveMaxDepth, const float adaptiveError,
		const unsigned int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition);

	bool IsMeshDefined(const std::string &meshName) const;
	bool IsTextureDefined(const std::string &texName) const;
	bool IsMaterialDefined(const std::string &matName) const;

	const unsigned int GetLightCount() const;
	const unsigned int GetObjectCount() const;

	void Parse(const luxrays::Properties &props);

	void DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
			const float transMat[16]);
	void DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
			const unsigned int count, const float *transMat);
	void DuplicateObject(const std::string &srcObjName, const std::string &dstObjName,
			const u_int steps, const float *times, const float *transMats);
	void DuplicateObject(const std::string &srcObjName, const std::string &dstObjNamePrefix,
			const unsigned int count, const u_int steps, const float *times,
			const float *transMats);
	void UpdateObjectTransformation(const std::string &objName, const float transMat[16]);
	void UpdateObjectMaterial(const std::string &objName, const std::string &matName);

	void DeleteObject(const std::string &objName);
	void DeleteLight(const std::string &lightName);

	void RemoveUnusedImageMaps();
	void RemoveUnusedTextures();
	void RemoveUnusedMaterials();
	void RemoveUnusedMeshes();

	void DefineImageMapUChar(const std::string &imgMapName,
			unsigned char *pixels, const float gamma, const unsigned int channels,
			const unsigned int width, const unsigned int height,
			ChannelSelectionType selectionType, WrapType wrapType);
	void DefineImageMapHalf(const std::string &imgMapName,
			unsigned short *pixels, const float gamma, const unsigned int channels,
			const unsigned int width, const unsigned int height,
			ChannelSelectionType selectionType, WrapType wrapType);
	void DefineImageMapFloat(const std::string &imgMapName,
			float *pixels, const float gamma, const unsigned int channels,
			const unsigned int width, const unsigned int height,
			ChannelSelectionType selectionType, WrapType wrapType);

	const luxrays::Properties &ToProperties() const;
	void Save(const std::string &fileName) const;

	// Note: this method is not part of LuxCore API and it is used only internally
	void DefineMesh(luxrays::ExtTriangleMesh *mesh);

	static luxrays::Point *AllocVerticesBuffer(const unsigned int meshVertCount);
	static luxrays::Triangle *AllocTrianglesBuffer(const unsigned int meshTriCount);

	friend class CameraImpl;
	friend class RenderConfigImpl;

private:
	mutable luxrays::Properties scenePropertiesCache;

	slg::Scene *scene;
	CameraImpl *camera;
	bool allocatedScene;
};

//------------------------------------------------------------------------------
// RenderConfigImpl
//------------------------------------------------------------------------------

class RenderStateImpl;
class RenderSessionImpl;

class RenderConfigImpl : public RenderConfig {
public:
	RenderConfigImpl(const luxrays::Properties &props, SceneImpl *scene = NULL);
	RenderConfigImpl(const std::string &fileName);
	RenderConfigImpl(const std::string &fileName, RenderStateImpl **startState,
			FilmImpl **startFilm);
	~RenderConfigImpl();

	const luxrays::Properties &GetProperties() const;
	const luxrays::Property GetProperty(const std::string &name) const;
	const luxrays::Properties &ToProperties() const;

	Scene &GetScene() const;

	void Parse(const luxrays::Properties &props);

	void Delete(const std::string &prefix);

	bool GetFilmSize(unsigned int *filmFullWidth, unsigned int *filmFullHeight,
		unsigned int *filmSubRegion) const;

	void DeleteSceneOnExit();

	void Save(const std::string &fileName);
	void Export(const std::string &dirName);

	static const luxrays::Properties &GetDefaultProperties();

	friend class RenderSessionImpl;

private:
	slg::RenderConfig *renderConfig;

	SceneImpl *scene;
	bool allocatedScene;
};

//------------------------------------------------------------------------------
// RenderStateImpl
//------------------------------------------------------------------------------

class RenderStateImpl : public RenderState {
public:
	RenderStateImpl(const std::string &fileName);
	RenderStateImpl(slg::RenderState *state);
	~RenderStateImpl();

	void Save(const std::string &fileName) const;

	friend class RenderSessionImpl;

private:
	slg::RenderState *renderState;
};

//------------------------------------------------------------------------------
// RenderSessionImpl
//------------------------------------------------------------------------------

class RenderSessionImpl : public RenderSession {
public:
	RenderSessionImpl(const RenderConfigImpl *config, RenderStateImpl *startState = NULL, FilmImpl *startFilm = NULL);
	RenderSessionImpl(const RenderConfigImpl *config, const std::string &startStateFileName, const std::string &startFilmFileName);

	~RenderSessionImpl();

	const RenderConfig &GetRenderConfig() const;
	RenderState *GetRenderState();

	void Start();
	void Stop();
	bool IsStarted() const;

	void BeginSceneEdit();
	void EndSceneEdit();
	bool IsInSceneEdit() const;

	void Pause();
	void Resume();
	bool IsInPause() const;

	bool HasDone() const;
	void WaitForDone() const;
	void WaitNewFrame();

	bool NeedPeriodicFilmSave();
	Film &GetFilm();

	void UpdateStats();
	const luxrays::Properties &GetStats() const;

	void Parse(const luxrays::Properties &props);

	void SaveResumeFile(const std::string &fileName);

	friend class FilmImpl;

private:
	const RenderConfig *renderConfig;
	FilmImpl *film;

	slg::RenderSession *renderSession;
	luxrays::Properties stats;
};

}
}

#endif	/* _LUXCOREIMPL_H */

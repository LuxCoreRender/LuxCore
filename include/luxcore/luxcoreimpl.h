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

#ifndef _LUXCOREIMPL_H
#define	_LUXCOREIMPL_H

#include "luxrays/utils/serializationutils.h"
#include <format>

#include <luxcore/luxcore.h>
#include <memory>
#include <slg/usings.h>
#include <slg/renderconfig.h>
#include <slg/rendersession.h>
#include <slg/renderstate.h>
#include <slg/scene/scene.h>
#include <slg/film/film.h>

namespace luxcore {
namespace detail {

class RenderSessionImpl;
using RenderSessionImplPtr = std::shared_ptr<RenderSessionImpl>;
using RenderSessionImplConstPtr = std::shared_ptr<const RenderSessionImpl>;
using RenderSessionImplUPtr = std::unique_ptr<RenderSessionImpl>;
using RenderSessionImplRef = RenderSessionImpl &;
using RenderSessionImplConstRef = const RenderSessionImpl &;

class RenderConfigImpl;
using RenderConfigImplUPtr = std::unique_ptr<RenderConfigImpl>;
using RenderConfigImplRef = RenderConfigImpl &;
using RenderConfigImplConstRef = const RenderConfigImpl &;

class RenderStateImpl;
using RenderStateImplRPtr = std::shared_ptr<RenderStateImpl>;

class SceneImpl;
using SceneImplConstRef = const SceneImpl &;
using SceneImplRef = SceneImpl &;
using SceneImplUPtr = std::unique_ptr<SceneImpl>;

class CameraImpl;
using CameraImplPtr = std::shared_ptr<CameraImpl>;
using CameraImplUPtr = std::unique_ptr<CameraImpl>;

class FilmImpl;
using FilmImplUPtr = std::unique_ptr<FilmImpl>;
using FilmImplRPtr = const std::unique_ptr<FilmImpl> &;
using FilmImplRef = FilmImpl&;


class FilmImplStandalone;
using FilmImplStandaloneUPtr = std::unique_ptr<FilmImplStandalone>;

// Disambiguation: there are luxcore::Film and slg:Film...
using LuxFilm = luxcore::Film;
using LuxFilmRef = luxcore::Film &;
using LuxFilmUPtr = std::unique_ptr<luxcore::Film>;
using LuxFilmPtr = const std::unique_ptr<luxcore::Film> &;
using LuxFilmConstRef = const luxcore::Film &;

// Disambiguation: there are luxcore::Camera and slg:Camera...
using LuxCamera = luxcore::Camera;
using LuxCameraConstRef = const luxcore::Camera &;
using LuxCameraRef = luxcore::Camera &;


//------------------------------------------------------------------------------
// FilmImpl
//------------------------------------------------------------------------------

class FilmImpl : public luxcore::Film {
public:

	// Standalone film
	static FilmImplUPtr Create(slg::FilmUPtr&& film);
	static FilmImplUPtr Create(const std::string &fileName);
	static FilmImplUPtr Create(
		luxrays::PropertiesRPtr props,
		const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel
	);

	// Session film
	static FilmImplUPtr Create(RenderSessionImplRef session);

	virtual ~FilmImpl() = default;

	unsigned int GetWidth() const;
	unsigned int GetHeight() const;
	luxrays::PropertiesUPtr GetStats() const;
	float GetFilmY(const unsigned int imagePipelineIndex = 0) const;

	void Clear();
	void AddFilm(LuxFilmConstRef film) override;
	void AddFilm(
		LuxFilmConstRef film,
		const unsigned int srcOffsetX, const unsigned int srcOffsetY,
		const unsigned int srcWidth, const unsigned int srcHeight,
		const unsigned int dstOffsetX, const unsigned int dstOffsetY
	) override;

	virtual void SaveOutputs() const = 0;
	void SaveOutput(
		const std::string &fileName,
		const FilmOutputType type,
		luxrays::PropertiesRPtr props
	) const;
	virtual void SaveFilm(const std::string &fileName) const = 0;

	double GetTotalSampleCount() const;

	size_t GetOutputSize(const FilmOutputType type) const;
	bool HasOutput(const FilmOutputType type) const;
	unsigned int GetOutputCount(const FilmOutputType type) const;

	unsigned int GetRadianceGroupCount() const;
	bool HasChannel(const FilmChannelType type) const;
	unsigned int GetChannelCount(const FilmChannelType type) const;

	virtual void GetOutputFloat(const FilmOutputType type, float *buffer,
			const unsigned int index, const bool executeImagePipeline) = 0;
	virtual void GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
			const unsigned int index, const bool executeImagePipeline) = 0;
	void UpdateOutputFloat(const FilmOutputType type, const float *buffer,
			const unsigned int index, const bool executeImagePipeline) = 0;
	void UpdateOutputUInt(const FilmOutputType type, const unsigned int *buffer,
			const unsigned int index, const bool executeImagePipeline); // throw

	virtual const float *GetChannelFloat(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) = 0;
	virtual const unsigned int *GetChannelUInt(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) = 0;
	virtual float *UpdateChannelFloat(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) = 0;
	virtual unsigned int *UpdateChannelUInt(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline);

	virtual void Parse(luxrays::PropertiesRPtr props) = 0;

	virtual void DeleteAllImagePipelines() = 0;

	virtual void ExecuteImagePipeline(const u_int index) = 0;
	virtual void AsyncExecuteImagePipeline(const u_int index) = 0;
	virtual void WaitAsyncExecuteImagePipeline() = 0;
	virtual bool HasDoneAsyncExecuteImagePipeline() = 0;

	virtual void ApplyOIDN(const u_int index) = 0;

	friend class RenderSessionImpl;

protected:
	FilmImpl() {}

private:
	virtual slg::FilmRef GetSLGFilm() const = 0;
};


// FilmImplStandalone is created from another Film
class FilmImplStandalone : public FilmImpl {
public:
	FilmImplStandalone(const std::string &fileName);
	FilmImplStandalone(
		luxrays::PropertiesRPtr props,
		const bool hasPixelNormalizedChannel,
		const bool hasScreenNormalizedChannel
	);
	FilmImplStandalone() = default;
	virtual ~FilmImplStandalone() = default;


	virtual void SaveOutputs() const override;
	virtual void SaveFilm(const std::string &fileName) const override;
	virtual void GetOutputFloat(const FilmOutputType type, float *buffer,
			const unsigned int index, const bool executeImagePipeline) override;
	virtual void GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
			const unsigned int index, const bool executeImagePipeline) override;
	void UpdateOutputFloat(const FilmOutputType type, const float *buffer,
			const unsigned int index, const bool executeImagePipeline) override;

	virtual const float *GetChannelFloat(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) override;
	virtual float *UpdateChannelFloat(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) override;
	virtual const unsigned int * GetChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) override;


	virtual void Parse(luxrays::PropertiesRPtr props) override;

	virtual void DeleteAllImagePipelines() override;

	virtual void ExecuteImagePipeline(const u_int index) override;
	virtual void AsyncExecuteImagePipeline(const u_int index) override;
	virtual void WaitAsyncExecuteImagePipeline() override;
	virtual bool HasDoneAsyncExecuteImagePipeline() override;

	virtual void ApplyOIDN(const u_int index) override;
	virtual slg::FilmRef GetSLGFilm() const override;

	friend class FilmImpl;
	friend class RenderSessionImpl;

private:
	slg::FilmUPtr standAloneFilm;

};


// FilmImplSession is created by RenderSessionImpl
class FilmImplSession : public FilmImpl {
public:
	FilmImplSession(RenderSessionImplRef session);
	virtual ~FilmImplSession() = default;

	FilmImplSession() = delete;

	virtual void SaveOutputs() const override;
	virtual void SaveFilm(const std::string &fileName) const override;
	virtual void GetOutputFloat(const FilmOutputType type, float *buffer,
			const unsigned int index, const bool executeImagePipeline) override;
	virtual void GetOutputUInt(const FilmOutputType type, unsigned int *buffer,
			const unsigned int index, const bool executeImagePipeline) override;
	void UpdateOutputFloat(const FilmOutputType type, const float *buffer,
			const unsigned int index, const bool executeImagePipeline) override;

	virtual const float *GetChannelFloat(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) override;
	virtual float *UpdateChannelFloat(const FilmChannelType type,
			const unsigned int index, const bool executeImagePipeline) override;
	virtual const unsigned int * GetChannelUInt(const FilmChannelType type,
		const unsigned int index, const bool executeImagePipeline) override;


	virtual void Parse(luxrays::PropertiesRPtr props) override;

	virtual void DeleteAllImagePipelines() override;

	virtual void ExecuteImagePipeline(const u_int index) override;
	virtual void AsyncExecuteImagePipeline(const u_int index) override;
	virtual void WaitAsyncExecuteImagePipeline() override;
	virtual bool HasDoneAsyncExecuteImagePipeline() override;

	virtual void ApplyOIDN(const u_int index) override;

	RenderSessionImplRef renderSession;  // Back link, read/write

private:

	virtual slg::FilmRef GetSLGFilm() const override;
};


//------------------------------------------------------------------------------
// CameraImpl
//------------------------------------------------------------------------------


class CameraImpl : public luxcore::Camera {
public:
	CameraImpl(SceneImplRef scene);
	~CameraImpl();

	const CameraType GetType() const;

	void Translate(const float x, const float y, const float z);
	void TranslateLeft(const float t);
	void TranslateRight(const float t);
	void TranslateForward(const float t);
	void TranslateBackward(const float t);

	void Rotate(const float angle, const float x, const float y, const float z);
	void RotateLeft(const float angle);
	void RotateRight(const float angle);
	void RotateUp(const float angle);
	void RotateDown(const float angle);

	friend class SceneImpl;

private:
	// Back link
	SceneImplRef scene;
};

//------------------------------------------------------------------------------
// SceneImpl
//------------------------------------------------------------------------------

class SceneImpl : public luxcore::Scene {

	struct Private {};

public:
	// Factory
	template<typename... Args>
	static SceneImplUPtr Create(Args... args) {
		return std::make_unique<SceneImpl>(Private(), std::forward<Args>(args)...);
	}

	// Constructors are private - please use factory
	SceneImpl(Private, slg::SceneRef scn);  // Non owning constructor
	SceneImpl(Private, luxrays::PropertiesRPtr resizePolicyProps = nullptr);
	SceneImpl(
		Private,
		luxrays::PropertiesRPtr props,
		luxrays::PropertiesRPtr resizePolicyProps
	);
	SceneImpl(
		Private,
		const std::string fileName,
		luxrays::PropertiesRPtr resizePolicyProps = nullptr
	);

	void GetBBox(float min[3], float max[3]) const;
	LuxCameraConstRef GetCamera() const;
	LuxCameraRef GetCamera();

	bool IsImageMapDefined(const std::string &imgMapName) const;

	void SetDeleteMeshData(const bool v);
	void SetMeshAppliedTransformation(const std::string &meshName,
			const float *appliedTransMat);

	void DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n,
		float *uvs,	float *cols, float *alphas);
	void DefineMeshExt(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		float *p, unsigned int *vi, float *n,
		std::array<float *, LC_MESH_MAX_DATA_COUNT> *uv,
		std::array<float *, LC_MESH_MAX_DATA_COUNT> *cols,
		std::array<float *, LC_MESH_MAX_DATA_COUNT> *alphas);
	void SetMeshVertexAOV(const std::string &meshName,
		const unsigned int index, float *data, size_t size) override;
	void SetMeshTriangleAOV(const std::string &meshName,
		const unsigned int index, float *data, size_t size) override;

	void SaveMesh(const std::string &meshName, const std::string &fileName);
	void DefineStrands(
		const std::string &shapeName,
		const luxrays::cyHairFile &strandsFile,
		const StrandsTessellationType tesselType,
		const unsigned int adaptiveMaxDepth,
		const float adaptiveError,
		const unsigned int solidSideCount,
		const bool solidCapBottom,
		const bool solidCapTop,
		const bool useCameraPosition
	);

	bool IsMeshDefined(const std::string &meshName) const;
	bool IsTextureDefined(const std::string &texName) const;
	bool IsMaterialDefined(const std::string &matName) const;

	const unsigned int GetLightCount() const;
	const unsigned int GetObjectCount() const;

	void Parse(luxrays::PropertiesRPtr props);

	void DuplicateObject(
		const std::string &srcObjName, const std::string &dstObjName,
		const float transMat[16], const unsigned int objectID
	);
	void DuplicateObject(
		const std::string &srcObjName, const std::string &dstObjNamePrefix,
		const unsigned int count, const float *transMat, const unsigned int *objectIDs
	);
	void DuplicateObject(
		const std::string &srcObjName, const std::string &dstObjName,
		const unsigned int steps, const float *times, const float *transMats,
		const unsigned int objectID);
	void DuplicateObject(
		const std::string &srcObjName, const std::string &dstObjNamePrefix,
		const unsigned int count, const unsigned int steps, const float *times,
		const float *transMats, const unsigned int *objectIDs);
	void UpdateObjectTransformation(
		const std::string &objName, const float transMat[16]
	);
	void UpdateObjectMaterial(
		const std::string &objName, const std::string &matName
	);

	void DeleteObject(const std::string &objName);
	void DeleteObjects(std::vector<std::string> &objNames);
	void DeleteLight(const std::string &lightName);
	void DeleteLights(std::vector<std::string> &lightNames);

	void RemoveUnusedImageMaps();
	void RemoveUnusedTextures();
	void RemoveUnusedMaterials();
	void RemoveUnusedMeshes();

	void DefineImageMapUChar(
		const std::string &imgMapName,
		unsigned char *pixels,
		const float gamma,
		const unsigned int channels,
		const unsigned int width,
		const unsigned int height,
		ChannelSelectionType selectionType,
		WrapType wrapType
	);
	void DefineImageMapHalf(const std::string &imgMapName,
			unsigned short *pixels, const float gamma, const unsigned int channels,
			const unsigned int width, const unsigned int height,
			ChannelSelectionType selectionType, WrapType wrapType);
	void DefineImageMapFloat(const std::string &imgMapName,
			float *pixels, const float gamma, const unsigned int channels,
			const unsigned int width, const unsigned int height,
			ChannelSelectionType selectionType, WrapType wrapType);

	luxrays::PropertiesRPtr ToProperties() const;
	void Save(const std::string &fileName);

	// Note: this method is not part of LuxCore API and it is used only internally
	void DefineMesh(luxrays::ExtTriangleMeshUPtr&& mesh);

	static luxrays::Point *AllocVerticesBuffer(const unsigned int meshVertCount);
	static luxrays::Triangle *AllocTrianglesBuffer(const unsigned int meshTriCount);


	//friend class CameraImpl;
	friend class RenderConfigImpl;
	friend class RenderSessionImpl;

	slg::SceneConstRef GetSlgScene() const { return sceneRef; }
	slg::SceneRef GetSlgScene() { return sceneRef; }

private:

	mutable luxrays::PropertiesUPtr scenePropertiesCache;

	// WARNING: KEEP FOLLOWING DECLARATIONS IN PRESENT ORDER
	// Order matters for initialization

	// Internal objects (owned)
	CameraImplUPtr camera;
	slg::SceneUPtr internalScene;

	// Reference to the working scene. Depending on object construction, it can
	// be an external scene, or the internal scene above
	slg::SceneRef sceneRef;

};

//------------------------------------------------------------------------------
// RenderConfigImpl
//------------------------------------------------------------------------------


class RenderConfigImpl : public luxcore::RenderConfig {
	struct Private {};
public:
	// Factory
	template<typename... Args>
	static RenderConfigImplUPtr Create(Args... args) {
		auto p = Private();
		return std::make_unique<RenderConfigImpl>(
			p, std::forward<Args>(args)...
		);
	}

	// Constructors (private, please use factory instead)
	RenderConfigImpl(  // Non owning constructor (scene is external)
		Private,
		luxrays::PropertiesRPtr props,
		SceneImpl& scene
	);
	RenderConfigImpl(Private, luxrays::PropertiesRPtr props);
	RenderConfigImpl(Private, const std::string fileName);
	RenderConfigImpl(
		Private,
		const std::string fileName,
		std::shared_ptr<RenderStateImpl>& startState,  // Out
		std::unique_ptr<FilmImpl>& startFilm  // Out
	);

	// Copy not authorized
	RenderConfigImpl(const RenderConfigImpl&) = delete;
	RenderConfigImpl& operator=(RenderConfigImpl&) = delete;

	virtual ~RenderConfigImpl() = default;

	luxrays::PropertiesRPtr GetProperties() const;
	luxrays::Property GetProperty(const std::string &name) const;
	luxrays::PropertiesRPtr ToProperties() const;

	const Scene& GetScene() const override;
	Scene& GetScene() override;

	bool HasCachedKernels() const;

	void Parse(luxrays::PropertiesRPtr props);

	void Delete(const std::string &prefix);

	bool GetFilmSize(unsigned int *filmFullWidth, unsigned int *filmFullHeight,
		unsigned int *filmSubRegion) const;

	void DeleteSceneOnExit();

	void Save(const std::string &fileName) const;
	void Export(const std::string &dirName) const;
	void ExportGLTF(const std::string &fileName) const;

	static luxrays::PropertiesRPtr GetDefaultProperties();

	template<typename T> T ReadFromSIF() const;

	friend class RenderSessionImpl;


private:
	// Input file for serialization (optional and mutable)
	mutable std::optional<luxrays::SerializationInputFile> sif;

	// CAVEAT: member order matters, for correct initialization.

	// The underlying slg object
	//
	// Warning: keep this declaration **before** internalScene declaration.
	// This of first importance for correct initialization order. See here:
	// https://en.cppreference.com/w/cpp/language/initializer_list.html#Initialization_order
	std::unique_ptr<slg::RenderConfig> renderConfig;

	// The (optional) internal scene.
	//
	// Warning: keep this declaration **before** sceneRef declaration.
	// This of first importance for correct initialization order. See here:
	// https://en.cppreference.com/w/cpp/language/initializer_list.html#Initialization_order
	SceneImplUPtr internalScene;

	// The working scene. Can bind to internal or external scene depending on
	// construction
	SceneImplRef sceneRef;

};


//------------------------------------------------------------------------------
// RenderStateImpl
//------------------------------------------------------------------------------

class RenderStateImpl : public RenderState {
public:
	RenderStateImpl(const std::string &fileName);
	RenderStateImpl(std::shared_ptr<slg::RenderState> state);

	void Save(const std::string &fileName) const;

	friend class RenderSessionImpl;

private:
	// RenderStateImpl does not create underlying slg::RenderState,
	// hence the shared pointer
	std::shared_ptr<slg::RenderState> renderState;
};

//------------------------------------------------------------------------------
// RenderSessionImpl
//------------------------------------------------------------------------------

class RenderSessionImpl : public luxcore::RenderSession
{
	// https://en.cppreference.com/w/cpp/memory/enable_shared_from_this.html
	// Need that to use std::make_unique
	struct Private{ explicit Private() = default; };

public:
	template<typename... Args>
	static RenderSessionImplUPtr Create(Args... args) {
		auto result = std::make_unique<RenderSessionImpl>(Private(), args...);
		result->InitFilm();
		return result;

	}

	// Public... but private constructors
	// https://en.cppreference.com/w/cpp/memory/enable_shared_from_this.html
	// Please use factory function
	RenderSessionImpl(
		Private priv,
		RenderConfigImplRef config
	);
	RenderSessionImpl(
		Private priv,
		RenderConfigImplRef config,  // Back link, not owned
		std::shared_ptr<RenderStateImpl>& startState,
		FilmImplStandalone& startFilm
	);
	RenderSessionImpl(
		Private priv,
		RenderConfigImplRef config,  // Back link, not owned
		const std::string &startStateFileName,
		const std::string &startFilmFileName
	);

	RenderConfigImplConstRef GetRenderConfig() const override;
	std::shared_ptr<RenderState> GetRenderState() override;

	void Start() override;
	void Stop() override;
	bool IsStarted() const override;

	void BeginSceneEdit() override;
	void EndSceneEdit() override;
	bool IsInSceneEdit() const override;

	void Pause() override;
	void Resume() override;
	bool IsInPause() const override;

	bool HasDone() const override;
	void WaitForDone() const override;
	void WaitNewFrame() override;

	LuxFilmRef GetFilm() override;
	FilmImplRPtr GetFilmPtr();

	void UpdateStats() override;
	const luxrays::PropertiesUPtr &GetStats() const override;

	void Parse(luxrays::PropertiesRPtr props) override;

	void SaveResumeFile(const std::string &fileName) override;

	virtual slg::RenderSessionRef GetSLGRenderSession() const { return *renderSession; }

	virtual ~RenderSessionImpl() override {
	}

	friend class FilmImpl;

private:
	// Back link, not owned
	RenderConfigImplConstRef renderConfig;

	// RenderSessionImpl creates and owns a slg::RenderSession and a FilmImpl
	// RenderSession must not be shared
	std::unique_ptr<slg::RenderSession> renderSession;
	FilmImplUPtr film;
	luxrays::PropertiesUPtr stats;

	void InitFilm();

};

}
}

template <> struct std::formatter<luxcore::Camera::CameraType>: formatter<string_view> {

  auto format(luxcore::Camera::CameraType cam, std::format_context& ctx) const
    -> format_context::iterator;
};



#endif	/* _LUXCOREIMPL_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

/*!
 * \file
 *
 * \brief LuxCore is new the LuxRender C++/Python API.
 * \author Bucciarelli David et al.
 * \version 1.0
 * \date October 2013
 *
 */

/*!
 * \mainpage
 * \section intro Introduction
 * LuxCore is the new LuxRender C++/Python API. It can be used to create and
 * render scenes. It includes the support for advanced new features like editing
 * materials, lights, geometry, interactive rendering and more.
 */

#ifndef _LUXCORE_H
#define	_LUXCORE_H

#include <cstddef>
#include <string>

#include <luxrays/utils/properties.h>
#include <slg/renderconfig.h>
#include <slg/rendersession.h>
#include <slg/sdl/scene.h>
#include <slg/film/film.h>
#include <luxcore/cfg.h>

/*! \mainpage LuxCore
 *
 * \section intro_sec Introduction
 *
 * LuxCore is the new LuxRender C++ API.
 */

/*!
 * \namespace luxcore
 *
 * \brief The LuxCore classes are defined within this namespace.
 */
namespace luxcore {

extern void (*LuxCore_LogHandler)(const char *msg); // LuxCore Log Handler

#define LC_LOG(a) { if (luxcore::LuxCore_LogHandler) { std::stringstream _LUXCORE_LOG_LOCAL_SS; _LUXCORE_LOG_LOCAL_SS << a; luxcore::LuxCore_LogHandler(_LUXCORE_LOG_LOCAL_SS.str().c_str()); } }

/*!
 * \brief Initializes LuxCore API. This function has to be called before
 * anything else. This function is thread safe. It can be called multiple times
 * if you want to update the log handler.
 *
 * \param LogHandler is a pointer to a function receiving all LuxCore log messages.
 */
extern void Init(void (*LogHandler)(const char *) = NULL);

class RenderSession;

/*!
 * \brief Film stores all the outputs of a rendering. It can be obtained only
 * from a RenderSession.
 */
class Film {
public:
	typedef enum {
		RGB, RGBA, RGB_TONEMAPPED, RGBA_TONEMAPPED, ALPHA, DEPTH, POSITION,
		GEOMETRY_NORMAL, SHADING_NORMAL, MATERIAL_ID, DIRECT_DIFFUSE,
		DIRECT_GLOSSY, EMISSION, INDIRECT_DIFFUSE, INDIRECT_GLOSSY,
		INDIRECT_SPECULAR, MATERIAL_ID_MASK, DIRECT_SHADOW_MASK, INDIRECT_SHADOW_MASK,
		RADIANCE_GROUP, UV, RAYCOUNT
	} FilmOutputType;

	~Film();

	/*!
	 * \brief Returns the Film width.
	 * 
	 * \return the Film width.
	 */
	u_int GetWidth() const;
	/*!
	 * \brief Returns the Film height.
	 * 
	 * \return the Film width.
	 */
	u_int GetHeight() const;
	/*!
	 * \brief Saves all Film output channels.
	 */
	void Save() const;

	/*!
	 * \brief Returns the size (in float or u_int) of a Film output channel.
	 *
	 * \param type is the Film output channel to use.
	 */
	size_t GetOutputSize(const FilmOutputType type) const;
	/*!
	 * \brief Fills the buffer with a Film output channel.
	 *
	 * \param type is the Film output channel to use. It must be one
	 * of the enabled channels in RenderConfig. The supported template types are
	 * float and unsigned int.
	 * \param buffer is the place where the data will be copied.
	 * \param index of the buffer to use. Most of the times is 0 however, for instance,
	 * if more than one light group is used, select the group to return.
	 */
	template<class T> void GetOutput(const FilmOutputType type, T *buffer, const u_int index = 0) const {
		throw std::runtime_error("Called Film::GetOutput() with wrong type");
	}

	/*!
	 * \brief Returns a pointer to the RGB_TONEMAPPED buffer. This is a fast path
	 * to avoid a buffer copy.
	 *
	 * \return a pointer to a float array with RGB values.
	 */
	const float *GetRGBToneMappedOutput() const;

	friend class RenderSession;

private:
	Film(const RenderSession &session);

	const RenderSession &renderSession;
};

template<> void Film::GetOutput<float>(const FilmOutputType type, float *buffer, const u_int index) const;
template<> void Film::GetOutput<u_int>(const FilmOutputType type, u_int *buffer, const u_int index) const;

class Scene;

/*!
 * \brief Film stores all the outputs of a rendering. It can be obtained only
 * from a RenderSession.
 */
class Camera {
public:
	~Camera();

	/*!
	 * \brief Translates by vector t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 *
	 * \param t is the translation vector.
	 */
	void Translate(const luxrays::Vector &t) const;
	/*!
	 * \brief Translates at left by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param t is the left translation.
	 */
	void TranslateLeft(const float t) const;
	/*!
	 * \brief Translates at right by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 */
	void TranslateRight(const float t) const;
	/*!
	 * \brief Translates forward by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 */
	void TranslateForward(const float t) const;
	/*!
	 * \brief Translates backward by t. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 */
	void TranslateBackward(const float t) const;

	/*!
	 * \brief Rotates by angle around the axis. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is rotation angle.
	 * \param axis is rotation axis.
	 */
	void Rotate(const float angle, const luxrays::Vector &axis) const;
	/*!
	* \brief Rotates the left by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is rotation angle.
	 */
	void RotateLeft(const float angle) const;
	/*!
	 * \brief Rotates the right by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is rotation angle.
	 */
	void RotateRight(const float angle) const;
	/*!
	 * \brief Rotates the up by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is rotation angle.
	 */
	void RotateUp(const float angle) const;
	/*!
	 * \brief Rotates the down by angle. This method can be used only when
	 * the Scene is not in use by a RenderSession.
	 * 
	 * \param angle is rotation angle.
	 */
	void RotateDown(const float angle) const;

	friend class Scene;

private:
	Camera(const Scene &scene);
	
	const Scene &scene;
};

/*!
 * \brief Scene stores textures, materials and objects definitions.
 */
class Scene {
public:
	/*!
	 * \brief Constructs a new empty Scene.
	 *
	 * \param imageScale defines the scale used for storing any kind of image in memory.
	 */
	Scene(const float imageScale = 1.f);
	/*!
	 * \brief Constructs a new Scene as defined in fileName file.
	 *
	 * \param fileName is the name of the file with the scene description to read.
	 * \param imageScale defines the scale used for storing any kind of image in memory.
	 */
	Scene(const std::string &fileName, const float imageScale = 1.f);
	~Scene();
	
	/*!
	 * \brief Returns all the Properties required to define this Scene.
	 *
	 * \return a reference to the Properties of this Scene.
	 */
	const luxrays::Properties &GetProperties() const;
	/*!
	 * \brief Returns the DataSet of the scene. It is available only
	 * during the rendering (i.e. after a RenderSession::Start()).
	 *
	 * \return a reference to the Properties of this Scene.
	 */
	const luxrays::DataSet &GetDataSet() const;
	/*!
	 * \brief Returns the Camera of the scene.
	 *
	 * \return a reference to the Camera of this Scene. It is available only
	 * during the rendering (i.e. after a RenderSession::Start()).
	 */
	const Camera &GetCamera() const;

	/*!
	 * \brief Defines an image map (to be later used in textures, infinite lights, etc.).
	 * The memory allocated for cols array is always freed by the Scene class.
	 *
	 * \param imgMapName is the name of the defined image map.
	 * \param cols is a pointer to the an array of float.
	 * \param gamma is the gamma correction value of the image.
	 * \param channels is the number of float used for each pixel (1 or 3).
	 * \param width is the width of the image map.
	 * \param height is the width of the image map.
	 */
	void DefineImageMap(const std::string &imgMapName, float *cols, const float gamma,
		const u_int channels, const u_int width, const u_int height);
	/*!
	 * \brief Check if an image map with the given name has been defined or not.
	 *
	 * \param imgMapName is the name to check.
	 *
	 * \return true if the image map has been defined, false otherwise.
	 */
	bool IsImageMapDefined(const std::string &imgMapName) const;
	/*!
	 * \brief Sets if the Scene class destructor has to delete all the arrays
	 * pointed by the defined meshes or not.
	 *
	 * \param v defines if I have to delete the mesh data or not.
	 */
	void SetDeleteMeshData(const bool v);
	/*!
	 * \brief Defines a mesh (to be later used in one or more scene objects). The
	 * memory allocated for the ExtTriangleMesh is always freed by the Scene class
	 * however the memory for the all vertices, triangle indices, etc. will be
	 * freed or not according the settings.
	 *
	 * \param meshName is the name of the defined mesh.
	 * \param mesh is a pointer to the mesh to be used.
	 */
	void DefineMesh(const std::string &meshName, luxrays::ExtTriangleMesh *mesh);
	/*!
	 * \brief Defines a mesh (to be later used in one or more scene objects). The
	 * memory allocate for the ExtTriangleMesh is always freed by the Scene class
	 * however the memory for the all vertices, triangle indices, etc. will be
	 * freed or not according the settings.
	 *
	 * \param meshName is the name of the define mesh.
	 * \param plyNbVerts is the number of mesh vertices.
	 * \param plyNbTris is the number of mesh triangles.
	 * \param p is a pointer to an array of vertices.
	 * \param vi is a pointer to an array of triangles.
	 * \param n is a pointer to an array of normals. It can be NULL.
	 * \param uv is a pointer to an array of UV coordinates. It can be NULL.
	 * \param cols is a pointer to an array of vertices colors. It can be NULL.
	 * \param alphas is a pointer to an array of vertices alpha. It can be NULL.
	 */
	void DefineMesh(const std::string &meshName,
		const long plyNbVerts, const long plyNbTris,
		luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
		luxrays::Spectrum *cols, float *alphas);
	/*!
	 * \brief Check if a mesh with the given name has been defined or not.
	 *
	 * \param meshName is the name to check.
	 *
	 * \return true if the mesh has been defined, false otherwise.
	 */
	bool IsMeshDefined(const std::string &meshName) const;
	/*!
	 * \brief Check if a texture with the given name has been defined or not.
	 *
	 * \param texName is the name to check.
	 *
	 * \return true if the texture has been defined, false otherwise.
	 */
	bool IsTextureDefined(const std::string &texName) const;
	/*!
	 * \brief Check if a material with the given name has been defined or not.
	 *
	 * \param matName is the name to check.
	 *
	 * \return true if the material has been defined, false otherwise.
	 */
	bool IsMaterialDefined(const std::string &matName) const;
	/*!
	 * \brief Returns the number of light sources in the Scene.
	 *
	 * \return the number of light sources in the Scene.
	 */	
	const u_int GetLightCount() const;
	/*!
	 * \brief Returns the number of objects in the Scene.
	 *
	 * \return the number of objects in the Scene.
	 */	
	const u_int GetObjectCount() const;

	/*!
	 * \brief Edits or creates camera, textures, materials and/or objects
	 * based on the Properties defined.
	 *
	 * \param props are the Properties with the definition of camera, textures,
	 * materials and/or objects.
	 */
	void Parse(const luxrays::Properties &props);

	/*!
	 * \brief Deletes an object from the scene.
	 *
	 * \param objName is the name of the object to delete.
	 */
	void DeleteObject(const std::string &objName);

	/*!
	 * \brief Removes all unused textures.
	 */
	void RemoveUnusedImageMaps();
	/*!
	 * \brief Removes all unused textures.
	 */
	void RemoveUnusedTextures();
	/*!
	 * \brief Removes all unused materials.
	 */
	void RemoveUnusedMaterials();
	/*!
	 * \brief Removes all unused meshes.
	 */
	void RemoveUnusedMeshes();

	friend class RenderConfig;
	friend class Camera;

private:
	Scene(slg::Scene *scn);

	slg::Scene *scene;
	Camera camera;
	bool allocatedScene;
};

/*!
 * \brief RenderConfig stores all the configuration settings used to render a
 * scene.
 */
class RenderConfig {
public:
	/*!
	 * \brief Constructs a new RenderConfig using the provided Properties and
	 * (optional) Scene.
	 *
	 * \param props are the Properties used to build the new RenderConfig.
	 * \param scene is the scene used to build the new RenderConfig. The scene
	 * is not deleted by the destructor if the parameter is not NULL. If it is NULL
	 * the scene will be read from the file specified in "scene.file" Property
	 * and deleted by the destructor.
	 */
	RenderConfig(const luxrays::Properties &props, Scene *scene = NULL);
	~RenderConfig();

	/*!
	 * \brief Returns a reference to the Properties used to create the RenderConfig;
	 *
	 * \return the RenderConfig properties.
	 */
	const luxrays::Properties &GetProperties() const;
	/*!
	 * \brief Returns the Property with the given name or the default value if it
	 * has not been defined.
	 *
	 * \return the Property with the given name.
	 */
	const luxrays::Property GetProperty(const std::string &name) const;

	/*!
	 * \brief Returns a reference to the Scene used in the RenderConfig;
	 *
	 * \return the reference to the RenderConfig Scene.
	 */
	Scene &GetScene();

	/*!
	 * \brief Sets configuration Properties with new values. this method can be
	 * used only if the RenderConfig is not in use by a RenderSession.
	 * 
	 * \param props are the Properties to set. 
	 */
	void Parse(const luxrays::Properties &props);
	/*!
	 * \brief Deletes any configuration Property starting for the given prefix. This
	 * method should be used only when the RenderConfig is not used by any
	 * RenderSession.
	 * 
	 * \param prefix is the prefix of the Properties to delete.
	 */
	void Delete(const std::string prefix);

	/*!
	 * \brief Return the configured Film width, height and if sub-region
	 * rendering is enabled or less.
	 * 
	 * \param filmFullWidth is where the configured Film width is returned if the
	 * pointer is not NULL. 
	 * \param filmFullHeight is where the configured Film height is returned if the
	 * pointer is not NULL. 
	 * \param filmSubRegion is an array of 4 values with the horizontal
	 * (followed by the vertical) begin and end of the Film region to
	 * render (in pixels).
	 *
	 * \return True if there is a sub-region to render, false otherwise.
	 */
	bool GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const;

	/*!
	 * \brief Returns a Properties container with all default values
	 * 
	 * \return the default Properties.
	 */
	static const luxrays::Properties &GetDefaultProperties();

	friend class RenderSession;

private:
	slg::RenderConfig *renderConfig;

	Scene *scene;
	bool allocatedScene;
};

/*!
 * \brief RenderSession execute a rendering based on the RenderConfig provided.
 */
class RenderSession {
public:
	/*!
	 * \brief Constructs a new RenderSession using the provided RenderConfig.
	 *
	 * \param config is the RenderConfig used to create the rendering session. The
	 * RenderConfig is not deleted by the destructor.
	 */
	RenderSession(const RenderConfig *config);
	~RenderSession();

	/*!
	 * \brief Returns a reference to the RenderingConfig used to create this
	 * RenderSession.
	 *
	 * \return a reference to the RenderingConfig.
	 */
	const RenderConfig &GetRenderConfig() const;

	/*!
	 * \brief Starts the rendering.
	 */
	void Start();
	/*!
	 * \brief Stops the rendering.
	 */
	void Stop();

	/*!
	 * \brief Suspends the rendering and allows to edit the Scene.
	 */
	void BeginSceneEdit();
	/*!
	 * \brief Ends the Scene editing and un-suspends the rendering.
	 */
	void EndSceneEdit();

	/*!
	 * \brief Used to wait for the next frame with real-time render engines like
	 * RTPATHOCL or RTBIASPATHOCL. It does nothing with other render engines.
	 */
	void WaitNewFrame();

	/*!
	 * \brief Checks if it is time to save the film according the RenderConfig.
	 *
	 * \return true if it is time to save the Film, false otherwise.
	 */
	bool NeedPeriodicFilmSave();
	/*!
	 * \brief Returns a reference to a Film with the output of the rendering.
	 *
	 * \return the reference to the Film.
	 */
	Film &GetFilm();

	/*!
	 * \brief Updates the statistics.
	 */
	void UpdateStats();
	/*!
	 * \brief Returns a list of statistics related to the ongoing rendering. The
	 * returned Properties is granted to have a content only after the first call
	 * to UpdateStats method.
	 *
	 * \return a Properties container with the statistics.
	 */
	const luxrays::Properties &GetStats() const;

	friend class Film;

private:
	const RenderConfig *renderConfig;
	Film film;

	slg::RenderSession *renderSession;
	luxrays::Properties stats;
};

/*!
 * \brief Types of render engines available.
 */
typedef slg::RenderEngineType RenderEngineType;
/*!
 * \brief Translates the name of a render engine in RenderEngineType.
 *
 * \param type is the name of the render engine.
 */
extern RenderEngineType String2RenderEngineType(const std::string &type);
/*!
 * \brief Translates a RenderEngineType in its name.
 *
 * \param type is the RenderEngineType to translate.
 */
extern const std::string RenderEngineType2String(const RenderEngineType type);
/*!
 * \brief Types of render engines available.
 */
typedef slg::SamplerType SamplerType;
/*!
 * \brief Translates the name of a sampler in SamplerType.
 *
 * \param type is the name of the render engine.
 */
extern SamplerType String2SamplerType(const std::string &type);
/*!
 * \brief Translates a SamplerType in its name.
 *
 * \param type is the SamplerType to translate.
 */
extern const std::string SamplerType2String(const RenderEngineType type);
/*!
 * \brief Types of render engines available.
 */
typedef slg::ToneMapType ToneMapType;
/*!
 * \brief Translates the name of a tone mapping in ToneMapType.
 *
 * \param type is the name of the render engine.
 */
extern ToneMapType String2ToneMapType(const std::string &type);
/*!
 * \brief Translates a ToneMapType in its name.
 *
 * \param type is the ToneMapType to translate.
 */
extern const std::string ToneMapType2String(const RenderEngineType type);

}

#endif

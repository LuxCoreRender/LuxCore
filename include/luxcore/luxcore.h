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

/*!
 * \brief Initializes LuxCore API. This function has to be called before anything else.
 */
extern void Init();

class RenderSession;

/*!
 * \brief Film stores all the outputs of a rendering. It can be obtained only
 * from a RenderSession.
 */
class Film {
public:
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
	 * \brief Checks if it is time to save the film according the RenderConfig.
	 */
	bool NeedPeriodicSave();
	/*!
	 * \brief Saves all Film output channels.
	 */
	void Save();

	// Just a temporary hack
	const float *GetScreenBuffer();

	friend class RenderSession;

private:
	Film(const RenderSession &session);

	const RenderSession &renderSession;
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
	 * \brief Sets if the Scene class destructor has to delete all the arrays
	 * pointed by the defined meshes or not.
	 *
	 * \param v defines if I have to delete the mesh data or not.
	 */
	void SetDeleteMeshData(const bool v);
	/*!
	 * \brief Defines a mesh (to be later used in one or more scene objects). The
	 * memory allocate for the ExtTriangleMesh is always freed by the Scene class
	 * however the memory for the all vertices, triangle indices, etc. will be
	 * freed or not according the settings.
	 *
	 * \param meshName is the name of the define mesh.
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

private:
	Scene(slg::Scene *scn);

	slg::Scene *scene;
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
	 * \brief Returns a list of statistics related to the ongoing rendering.
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

}

#endif

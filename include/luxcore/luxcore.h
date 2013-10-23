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


/*!
 * \brief Scene stores textures, mateirials and objects definitions.
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
	 * \brief Edits or creates textures, materials and/or objects based on the Properties
	 * defined.
	 *
	 * \param props are the Properties with the definition of textures,
	 * materials and/or objects
	 */
	void Parse(const luxrays::Properties &props);

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
	 * \brief Checks if it is time to save the film according the RenderConfig.
	 */
	bool NeedPeriodicFilmSave();
	/*!
	 * \brief Saves all Film output channels
	 */
	void SaveFilm();

	// Just a temporary hack
	const float *GetScreenBuffer();

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

private:
	const RenderConfig *renderConfig;
	slg::RenderSession *renderSession;
	luxrays::Properties stats;
};

}

#endif

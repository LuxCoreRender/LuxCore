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

#include <sstream>
#include <iostream>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include "slg/engines/filesaver/filesaver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

using namespace boost::filesystem;

//------------------------------------------------------------------------------
// Scene FileSaver render engine
//------------------------------------------------------------------------------

FileSaverRenderEngine::FileSaverRenderEngine(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		RenderEngine(rcfg, flm, flmMutex) {
	film->Init();
}

void FileSaverRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	directoryName = cfg.Get(GetDefaultProps().Get("filesaver.directory")).Get<string>();
	renderEngineType = cfg.Get(GetDefaultProps().Get("filesaver.renderengine.type")).Get<string>();
	
	SaveScene();
}


template <class T> static string v2s(T &v) {
	std::ostringstream ss;
    ss << std::setprecision(24) << v.x << " " << v.y << " " << v.z;
	return ss.str();
}

void FileSaverRenderEngine::SaveScene() {
	SLG_LOG("[FileSaverRenderEngine] Export directory: " << directoryName);

	const path dirPath(directoryName);

	// Check if the directory exists
	if (!exists(dirPath))
		throw std::runtime_error("Directory doesn't exist: " + directoryName);
	if (!is_directory(dirPath))
		throw std::runtime_error("It is not a directory: " + directoryName);

	//--------------------------------------------------------------------------
	// Export the .cfg file
	//--------------------------------------------------------------------------
	{
		const std::string cfgFileName = (dirPath / "render.cfg").generic_string();
		SLG_LOG("[FileSaverRenderEngine] Config file name: " << cfgFileName);

		Properties cfg = renderConfig->cfg;

		// Overwrite the scene file name
		cfg.Set(Property("scene.file")("scene.scn"));
		// Set render engine type
		cfg.Set(Property("renderengine.type")(renderEngineType));
		// Remove FileSaver Option
		cfg.Delete("filesaver.directory");

		BOOST_OFSTREAM cfgFile(cfgFileName.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
		if(!cfgFile.is_open())
			throw std::runtime_error("Unable to open: " + cfgFileName);

		cfgFile << cfg.ToString();
		if(!cfgFile.good())
			throw std::runtime_error("Error while writing file: " + cfgFileName);

		cfgFile.close();
	}

	//--------------------------------------------------------------------------
	// Export the .scn
	//--------------------------------------------------------------------------
	const std::string sceneFileName = (dirPath / "scene.scn").generic_string();
	{
		SLG_LOG("[FileSaverRenderEngine] Scene file name: " << sceneFileName);

		Properties props = renderConfig->scene->ToProperties();

		// Write the scene file
		BOOST_OFSTREAM sceneFile(sceneFileName.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
		if(!sceneFile.is_open())
			throw std::runtime_error("Unable to open: " + sceneFileName);
		sceneFile << props.ToString();
		sceneFile.close();
	}

	//--------------------------------------------------------------------------
	// Export the .ply/.exr files
	//--------------------------------------------------------------------------
	{
		// Write the image map information
		SDL_LOG("Saving image maps information:");
		vector<const ImageMap *> ims;
		renderConfig->scene->imgMapCache.GetImageMaps(ims);
		for (u_int i = 0; i < ims.size(); ++i) {
			const string fileName = (dirPath / ims[i]->GetFileName(renderConfig->scene->imgMapCache)).generic_string();
			SDL_LOG("  " + fileName);
			ims[i]->WriteImage(fileName);
		}

		// Write the mesh information
		SDL_LOG("Saving meshes information:");
		const vector<ExtMesh *> &meshes =  renderConfig->scene->extMeshCache.GetMeshes();
		set<string> savedMeshes;
		double lastPrint = WallClockTime();
		for (u_int i = 0; i < meshes.size(); ++i) {
			if (WallClockTime() - lastPrint > 2.0) {
				SDL_LOG("  " << i << "/" << meshes.size());
				lastPrint = WallClockTime();
			}

			u_int meshIndex;
			if (meshes[i]->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
				const ExtInstanceTriangleMesh *m = (ExtInstanceTriangleMesh *)meshes[i];
				meshIndex = renderConfig->scene->extMeshCache.GetExtMeshIndex(m->GetExtTriangleMesh());
			} else
				meshIndex = renderConfig->scene->extMeshCache.GetExtMeshIndex(meshes[i]);
			const string fileName = directoryName + "/mesh-" + (boost::format("%05d") % meshIndex).str() + ".ply";

			// Check if I have already saved this mesh (mostly useful for instances)
			if (savedMeshes.find(fileName) == savedMeshes.end()) {
				//SDL_LOG("  " + fileName);
				meshes[i]->WritePly(fileName);
				savedMeshes.insert(fileName);
			}
		}
	}
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties FileSaverRenderEngine::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("filesaver.directory")) <<
			cfg.Get(GetDefaultProps().Get("filesaver.renderengine.type"));
}

RenderEngine *FileSaverRenderEngine::FromProperties(const RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) {
	return new FileSaverRenderEngine(rcfg, flm, flmMutex);
}

const Properties &FileSaverRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("filesaver.directory")("luxcore-exported-scene") <<
			Property("filesaver.renderengine.type")("PATHCPU");

	return props;
}

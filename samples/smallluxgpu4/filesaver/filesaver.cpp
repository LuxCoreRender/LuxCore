/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <sstream>
#include <iostream>
#include <fstream>

#include <boost/filesystem.hpp>

#include "filesaver/filesaver.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;
using namespace boost::filesystem;

namespace slg {

//------------------------------------------------------------------------------
// Scene FileSaver render engine
//------------------------------------------------------------------------------

FileSaverRenderEngine::FileSaverRenderEngine(RenderConfig *rcfg, Film *flm, boost::mutex *flmMutex) :
		RenderEngine(rcfg, flm, flmMutex) {

}

void FileSaverRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	directoryName = cfg.GetString("filesaver.directory", "slg-exported-scene");
	
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

	// Check if the directory exist
	if (!exists(dirPath))
		throw std::runtime_error("Directory doesn't exist: " + directoryName);
	if (!is_directory(dirPath))
		throw std::runtime_error("It is not a directory: " + directoryName);

	const std::string cfgFileName = (dirPath / "render.cfg").generic_string();
	const std::string sceneFileName = (dirPath / "scene.scn").generic_string();

	//--------------------------------------------------------------------------
	// Export the .cfg file
	//--------------------------------------------------------------------------
	{
		
		SLG_LOG("[FileSaverRenderEngine] Config file name: " << cfgFileName);

		Properties cfg = renderConfig->cfg;

		// Overwrite the scene file name
		cfg.SetString("scene.file", sceneFileName);
		// Remove FileSaver Option
		cfg.Delete("filesaver.directory");

		std::ofstream cfgFile(cfgFileName.c_str());
		if(!cfgFile.is_open())
			throw std::runtime_error("Unable to open: " + cfgFileName);

		cfgFile << cfg.ToString();
		if(!cfgFile.good())
			throw std::runtime_error("Error while writing file: " + cfgFileName);

		cfgFile.close();
	}

	//--------------------------------------------------------------------------
	// Export the .scn file
	//--------------------------------------------------------------------------
	{
		
		SLG_LOG("[FileSaverRenderEngine] Scene file name: " << sceneFileName);

		Scene *scene = renderConfig->scene;

		Properties props;

		// Write the camera information
		props.LoadFromObject(*(scene->camera));

		// Write the texture information
		SLG_LOG("[FileSaverRenderEngine] Exporting textures:");
		for (u_int i = 0; i < scene->texDefs.GetSize(); ++i) {
			Texture *tex = scene->texDefs.GetTexture(i);
			SLG_LOG("[FileSaverRenderEngine]   " + tex->GetName());
			props.LoadFromObject(*tex);
		}

		// Write the scene file
		std::ofstream sceneFile(sceneFileName.c_str());
		if(!sceneFile.is_open())
			throw std::runtime_error("Unable to open: " + sceneFileName);
		sceneFile << props.ToString();
		sceneFile.close();
	}
}

}

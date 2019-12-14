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

#include <sstream>
#include <iostream>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#include <nlohmann/json.hpp>

#include "slg/engines/filesaver/filesaver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

using namespace boost::filesystem;
using namespace nlohmann;

//------------------------------------------------------------------------------
// Scene FileSaver render engine
//------------------------------------------------------------------------------

FileSaverRenderEngine::FileSaverRenderEngine(const RenderConfig *rcfg) :
		RenderEngine(rcfg) {
}

void FileSaverRenderEngine::InitFilm() {
	film->Init();
}

void FileSaverRenderEngine::StartLockLess() {
	const Properties &cfg = renderConfig->cfg;

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------

	renderEngineType = cfg.Get(GetDefaultProps().Get("filesaver.renderengine.type")).Get<string>();
	exportFormat = cfg.Get(GetDefaultProps().Get("filesaver.format")).Get<string>();
	directoryName = cfg.Get(GetDefaultProps().Get("filesaver.directory")).Get<string>();
	fileName = cfg.Get(GetDefaultProps().Get("filesaver.filename")).Get<string>();
	
	SaveScene();
}

void FileSaverRenderEngine::SaveScene() {
	if (exportFormat == "TXT")
		FileSaverRenderEngine::ExportScene(renderConfig, directoryName, renderEngineType);
	else if (exportFormat == "BIN") {
		Properties additionalCfg;
		additionalCfg.Set(Property("renderengine.type")(renderEngineType));

		// Save the binary file
		RenderConfig::SaveSerialized(fileName, renderConfig, additionalCfg);
	} else
		throw runtime_error("Unknown format in FileSaverRenderEngine: " + exportFormat);
}

void FileSaverRenderEngine::ExportSceneGLTF(const RenderConfig *renderConfig,
		const string &fileName) {
	SLG_LOG("[FileSaverRenderEngine] Export glTF scene file: " << fileName);
	
	json j;

	//--------------------------------------------------------------------------
	// Scenes
	//--------------------------------------------------------------------------

	j["scenes"] = json::array({
		json::object({
			{ "nodes", { 0 } }
		})
	});

	//--------------------------------------------------------------------------
	// Nodes
	//--------------------------------------------------------------------------
	
	j["nodes"] = json::array({
		json::object({
			{ "mesh", 0 }
		})
	});

	//--------------------------------------------------------------------------
	// Meshes
	//--------------------------------------------------------------------------
	
	j["meshes"] = json::array({
		json::object({
			{ "primitives", json::array({
				json::object({
					{ "attributes", json::object({
						{ "POSITION", 1 }
					})},
					{"indices", 0 }
				})
			})}
		})
	});

	//--------------------------------------------------------------------------
	// Buffers
	//--------------------------------------------------------------------------

	j["buffers"] = json::array({
		json::object({
			{ "uri", "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=" },
			{ "byteLength", 44 }
		})
	});

	//--------------------------------------------------------------------------
	// BufferViews
	//--------------------------------------------------------------------------

	j["bufferViews"] = json::array({
		json::object({
			{ "buffer", 0 },
			{ "byteOffset", 0 },
			{ "byteLength", 6 },
			{ "target", 34963 }
		}),
		json::object({
			{ "buffer", 0 },
			{ "byteOffset", 8 },
			{ "byteLength", 36 },
			{ "target", 34962 }
		}),
	});

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	j["accessors"] = json::array({
		json::object({
			{ "bufferView", 0 },
			{ "byteOffset", 0 },
			{ "componentType", 5123 },
			{ "count", 3 },
			{ "type", "SCALAR" },
			{ "max", { 2 } },
			{ "min", { 0 } }
		}),
		json::object({
			{ "bufferView", 1 },
			{ "byteOffset", 0 },
			{ "componentType", 5126 },
			{ "count", 3 },
			{ "type", "VEC3" },
			{ "max", { 1.0, 1.0, 0.0 } },
			{ "min", { 0.0, 0.0, 0.0 } }
		}),
	});

	//--------------------------------------------------------------------------
	// Asset
	//--------------------------------------------------------------------------

	j["asset"]["version"] = "2.0";
	j["asset"]["generator"] = "LuxCore API";

	SLG_LOG("glTF: ");
	SLG_LOG(endl << std::setw(4) << j);
}

void FileSaverRenderEngine::ExportScene(const RenderConfig *renderConfig,
		const string &directoryName, const string &renderEngineType) {
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

		Properties props = renderConfig->scene->ToProperties(false);

		// Write the scene file
		BOOST_OFSTREAM sceneFile(sceneFileName.c_str(), std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
		if(!sceneFile.is_open())
			throw std::runtime_error("Unable to open: " + sceneFileName);
		sceneFile << props.ToString();
		sceneFile.close();
	}

	//--------------------------------------------------------------------------
	// Export the .ply/.exr/.png files
	//--------------------------------------------------------------------------
	{
		// Write the image map information
		SDL_LOG("Saving image maps information:");
		vector<const ImageMap *> ims;
		renderConfig->scene->imgMapCache.GetImageMaps(ims);
		for (u_int i = 0; i < ims.size(); ++i) {
			const string fileName = (dirPath / renderConfig->scene->imgMapCache.GetSequenceFileName(ims[i])).generic_string();
			SDL_LOG("  " + fileName);
			ims[i]->WriteImage(fileName);
		}

		// Write the mesh information
		SDL_LOG("Saving meshes information:");
		const u_int meshCount =  renderConfig->scene->extMeshCache.GetSize();
		double lastPrint = WallClockTime();
		for (u_int i = 0; i < meshCount; ++i) {
			if (WallClockTime() - lastPrint > 2.0) {
				SDL_LOG("  " << i << "/" << meshCount);
				lastPrint = WallClockTime();
			}

			ExtMesh *mesh = renderConfig->scene->extMeshCache.GetExtMesh(i);
			// The only meshes I need to save are the real one. The others (instances, etc.)
			// will reference only true one.
			if (mesh->GetType() != TYPE_EXT_TRIANGLE)
				continue;

			const string fileName = (dirPath / renderConfig->scene->extMeshCache.GetSequenceFileName(mesh)).generic_string();

			//SDL_LOG("  " + fileName);
			mesh->Save(fileName);
		}
	}
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

Properties FileSaverRenderEngine::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps().Get("filesaver.format")) <<
			cfg.Get(GetDefaultProps().Get("filesaver.directory")) <<
			cfg.Get(GetDefaultProps().Get("filesaver.renderengine.type"));
}

RenderEngine *FileSaverRenderEngine::FromProperties(const RenderConfig *rcfg) {
	return new FileSaverRenderEngine(rcfg);
}

const Properties &FileSaverRenderEngine::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("filesaver.format")("TXT") <<
			Property("filesaver.directory")("luxcore-exported-scene") <<
			Property("filesaver.filename")("luxcore-exported-scene.bcf") <<
			Property("filesaver.renderengine.type")("PATHCPU");

	return props;
}

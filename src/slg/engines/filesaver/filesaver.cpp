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

#include <sstream>
#include <iostream>
#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include <nlohmann/json.hpp>

#include "slg/engines/filesaver/filesaver.h"
#include "slg/textures/imagemaptex.h"

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

static string Base64Encode(const char *data, const size_t size) {
	stringstream ss;
	
	typedef boost::archive::iterators::base64_from_binary<
			boost::archive::iterators::transform_width<const char *, 6, 8>
	> base64_t;

	copy(base64_t(data), base64_t(data + size), boost::archive::iterators::ostream_iterator<char>(ss));

	return ss.str();
}

void FileSaverRenderEngine::ExportSceneGLTF(const RenderConfig *renderConfig,
		const string &fileName) {
	SLG_LOG("[FileSaverRenderEngine] Export glTF scene file: " << fileName);
	
	const path fileNamePath(fileName);
	const path dirPath = fileNamePath.parent_path();
	
	json j;

	j["extensionsUsed"] = json::array({
		"KHR_materials_unlit"
	});

	//--------------------------------------------------------------------------
	// Scenes
	//--------------------------------------------------------------------------

	j["scenes"] = json::array({
		json::object({
			{ "nodes", json::array() }
		})
	});

	//--------------------------------------------------------------------------
	// Nodes, Meshes and related Buffers, BufferViews and Accessors
	//--------------------------------------------------------------------------
	
	SDL_LOG("Saving Scene Objects information:");

	j["nodes"] = json::array();
	j["meshes"] = json::array();
	j["buffers"] = json::array();
	j["bufferViews"] = json::array();
	j["accessors"] = json::array();
	j["materials"] = json::array();
	j["textures"] = json::array();
	j["images"] = json::array();
	j["samplers"] = json::array();

	// Add default material for not baked objects
	j["materials"].push_back(json::object({
		{ "name", "defaultNotBakedMaterial" },
        { "pbrMetallicRoughness", json::object({
            { "baseColorFactor", { 1.0, 1.0, 1.0, 1.0 } },
            { "roughnessFactor", 1.0 },
            { "metallicFactor", 0.0 }
        }) },
        { "extensions", json::object({
            { "KHR_materials_unlit", json::object({}) }
		}) },
	}));

	// Add the default sampler
	j["samplers"].push_back(json::object({
		{ "magFilter", 0x2601 }, // GL_LINEAR
		{ "minFilter", 0x2703 }, // GL_LINEAR_MIPMAP_LINEAR
		{ "wrapS", 0x812F }, // GL_CLAMP_TO_EDGE
		{ "wrapT", 0x812F } // GL_CLAMP_TO_EDGE
	}));

	const u_int sceneObjectsCount =  renderConfig->scene->objDefs.GetSize();
	double lastPrint = WallClockTime();
	for (u_int i = 0; i < sceneObjectsCount; ++i) {
		if (WallClockTime() - lastPrint > 2.0) {
			SDL_LOG("  " << i << "/" << sceneObjectsCount);
			lastPrint = WallClockTime();
		}

		const SceneObject *scnObj = renderConfig->scene->objDefs.GetSceneObject(i);
		const ExtMesh *mesh = scnObj->GetExtMesh();
		// TODO: other mesh types
		if (mesh->GetType() != TYPE_EXT_TRIANGLE)
			continue;

		const ExtTriangleMesh *triMesh = (const ExtTriangleMesh *)mesh;

		//----------------------------------------------------------------------
		// Add vertices buffer

		const size_t encodedVerticesSize = sizeof(Point) * triMesh->GetTotalVertexCount();
		const string encodedVertices = Base64Encode((const char *)triMesh->GetVertices(),
				encodedVerticesSize);

		j["buffers"].push_back(json::object({
			{ "uri", "data:application/octet-stream;base64," + encodedVertices },
			{ "byteLength", encodedVerticesSize }
		}));

		// Add vertices buffer view

		j["bufferViews"].push_back(json::object({
			{ "buffer",  j["buffers"].size() - 1 },
			{ "byteOffset", 0 },
			{ "byteLength", encodedVerticesSize },
			{ "target", 0x8892 }, // GL_ARRAY_BUFFER
		}));

		// Add vertices accessor

		const BBox meshBBox = triMesh->GetBBox();

		const u_int verticesAccessorIndex = j["accessors"].size();
		j["accessors"].push_back(json::object({
			{ "bufferView", j["bufferViews"].size() - 1 },
			{ "byteOffset", 0 },
			{ "componentType", 0x1406 }, // GL_FLOAT
			{ "count", triMesh->GetTotalVertexCount() },
			{ "type", "VEC3" },
			{ "max", { meshBBox.pMax.x, meshBBox.pMax.y, meshBBox.pMax.z } },
			{ "min", { meshBBox.pMin.x, meshBBox.pMin.y, meshBBox.pMin.z } }
		}));

		//----------------------------------------------------------------------
		// Add vertex UVs

		u_int vertexUVsAccessorIndex = NULL_INDEX;
		if (scnObj->HasBakeMap(COMBINED) && triMesh->HasUVs(scnObj->GetBakeMapUVIndex())) {
			const size_t encodedVertexUVsSize = sizeof(UV) * triMesh->GetTotalVertexCount();
			const string encodedVertexUVs = Base64Encode(
					(const char *)(triMesh->GetAllUVs()[scnObj->GetBakeMapUVIndex()]),
					encodedVertexUVsSize);

			j["buffers"].push_back(json::object({
				{ "uri", "data:application/octet-stream;base64," + encodedVertexUVs },
				{ "byteLength", encodedVertexUVsSize }
			}));

			// Add vertex UVs buffer view

			j["bufferViews"].push_back(json::object({
				{ "buffer",  j["buffers"].size() - 1 },
				{ "byteOffset", 0 },
				{ "byteLength", encodedVertexUVsSize },
				{ "target", 0x8892 }, // GL_ARRAY_BUFFER
			}));

			// Add vertex UVs accessor

			UV minUV(numeric_limits<float>::infinity(), numeric_limits<float>::infinity());
			UV maxUV(-numeric_limits<float>::infinity(), -numeric_limits<float>::infinity());
			UV *uv = triMesh->GetAllUVs()[scnObj->GetBakeMapUVIndex()];
			for (u_int uvIndex = 0; uvIndex < triMesh->GetTotalVertexCount(); ++uvIndex) {
				minUV.u = Min(minUV.u, uv[uvIndex].u);
				minUV.v = Min(minUV.v, uv[uvIndex].v);

				maxUV.u = Max(maxUV.u, uv[uvIndex].u);
				maxUV.v = Max(maxUV.v, uv[uvIndex].v);
			}

			vertexUVsAccessorIndex = j["accessors"].size();
			j["accessors"].push_back(json::object({
				{ "bufferView", j["bufferViews"].size() - 1 },
				{ "byteOffset", 0 },
				{ "componentType", 0x1406 }, // GL_FLOAT
				{ "count", triMesh->GetTotalVertexCount() },
				{ "type", "VEC2" },
				{ "max", { maxUV.u, maxUV.v } },
				{ "min", { minUV.u, minUV.v } }
			}));
		}

		//----------------------------------------------------------------------
		// Add triangle indices buffer

		const size_t encodedTrianglesSize = sizeof(Triangle) * triMesh->GetTotalTriangleCount();
		const string encodedTriangles = Base64Encode((const char *)triMesh->GetTriangles(),
				encodedTrianglesSize);

		j["buffers"].push_back(json::object({
			{ "uri", "data:application/octet-stream;base64," + encodedTriangles },
			{ "byteLength", encodedTrianglesSize }
		}));

		// Add triangle indices buffer view

		j["bufferViews"].push_back(json::object({
			{ "buffer", j["buffers"].size() - 1 },
			{ "byteOffset", 0 },
			{ "byteLength", encodedTrianglesSize },
			{ "target", 0x8893 }, // GL_ELEMENT_ARRAY_BUFFER
		}));

		// Add triangle indices accessor

		const u_int indicesAccessorIndex = j["accessors"].size();
		j["accessors"].push_back(json::object({
			{ "bufferView", j["bufferViews"].size() - 1 },
			{ "byteOffset", 0 },
			{ "componentType", 0x1405 }, // GL_UNSIGNED_INT
			{ "count", triMesh->GetTotalTriangleCount() * 3 },
			{ "type", "SCALAR" },
			{ "max", { triMesh->GetTotalVertexCount() - 1 } },
			{ "min", { 0 } }
		}));

		//----------------------------------------------------------------------
		// Add the mesh

		const u_int meshIndex = j["meshes"].size();
		j["meshes"].push_back(json::object({
			{ "primitives", json::array({
				json::object({
					{ "mode", 0x4 }, // GL_TRIANGLES
					{ "indices", indicesAccessorIndex }
				})
			}) }
		}));

		if (vertexUVsAccessorIndex != NULL_INDEX) {
			// Add vertex position and UV
			j["meshes"][meshIndex]["primitives"][0]["attributes"] = json::object({
				{ "POSITION", verticesAccessorIndex },
				{ "TEXCOORD_0", vertexUVsAccessorIndex }
			});
		} else {
			// Add only vertex position
			j["meshes"][meshIndex]["primitives"][0]["attributes"] = json::object({
				{ "POSITION", verticesAccessorIndex }
			});			
		}

		//----------------------------------------------------------------------
		// Add the materials

		// 0 is the default material for not baked object
		u_int materialIndex = 0;
		if (scnObj->HasBakeMap(COMBINED) && triMesh->HasUVs(scnObj->GetBakeMapUVIndex())) {
			// Write the image to file

			const ImageMap *imgMap = scnObj->GetBakeMap();
			const string imgMapFileName = (dirPath / imgMap->GetName()).generic_string() + ".png";
			SDL_LOG("  Saving image map: " << imgMapFileName << " (channels: " << imgMap->GetChannelCount() << ")");
			imgMap->WriteImage(imgMapFileName);
			
			// Create the image
			const u_int imageIndex = j["images"].size();
			j["images"].push_back(json::object({
				{ "uri", imgMap->GetName() + ".png" }
			}));
	
			// Create the texture
			const u_int textureIndex = j["textures"].size();
			j["textures"].push_back(json::object({
				{ "source", imageIndex },
				{ "sampler", 0 }
			}));

			// Create the baked material
			materialIndex = j["materials"].size();
			j["materials"].push_back(json::object({
				{ "name", scnObj->GetName() + "Material" },
				{ "pbrMetallicRoughness", json::object({
					{ "baseColorFactor", { 1.0, 1.0, 1.0, 1.0 } },
					{ "baseColorTexture", json::object({
						{ "index", textureIndex }
					}) },
					{ "roughnessFactor", 1.0 },
					{ "metallicFactor", 0.0 }
				}) },
				{ "extensions", json::object({
					{ "KHR_materials_unlit", json::object({}) }
				}) },
			}));
		}

		j["meshes"][meshIndex]["primitives"][0]["material"] = materialIndex;

		//----------------------------------------------------------------------
		// Add the node

		const u_int nodeIndex = j["nodes"].size();
		j["nodes"].push_back(json::object({
			{ "mesh", meshIndex },
			// To use an OpenGL-like coordinate reference system
			{ "matrix", {
				1, 0, 0, 0,
				0, 0, -1, 0,
				0, 1, 0, 0,
				0, 0, 0, 1
				}
			}
		}));

		j["scenes"][0]["nodes"].push_back(nodeIndex);

		//----------------------------------------------------------------------
	}


	//--------------------------------------------------------------------------
	// Asset
	//--------------------------------------------------------------------------

	j["asset"]["version"] = "2.0";
	j["asset"]["generator"] = "LuxCore API";

	//--------------------------------------------------------------------------
	// Save the glTF file
	//--------------------------------------------------------------------------

	//SLG_LOG("glTF: ");
	//SLG_LOG(endl << std::setw(4) << j);
	
	// The use of boost::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	boost::filesystem::ofstream gltfFile(boost::filesystem::path(fileName),
			boost::filesystem::ofstream::out |
			boost::filesystem::ofstream::binary |
			boost::filesystem::ofstream::trunc);

	if(!gltfFile.is_open())
		throw std::runtime_error("Unable to open: " + fileName);

	gltfFile << std::setw(4) << j;
	if(!gltfFile.good())
		throw std::runtime_error("Error while writing file: " + fileName);

	gltfFile.close();
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

		cfg.Save(cfgFileName);
	}

	//--------------------------------------------------------------------------
	// Export the .scn
	//--------------------------------------------------------------------------
	const std::string sceneFileName = (dirPath / "scene.scn").generic_string();
	{
		SLG_LOG("[FileSaverRenderEngine] Scene file name: " << sceneFileName);

		Properties props = renderConfig->scene->ToProperties(false);

		// Write the scene file
		props.Save(sceneFileName);
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
			// Avoid to save ImageMapTexture::randomImageMap
			if (ims[i] != ImageMapTexture::randomImageMap.get()) {
				const string fileName = (dirPath / renderConfig->scene->imgMapCache.GetSequenceFileName(ims[i])).generic_string();
				SDL_LOG("  " + fileName);
				ims[i]->WriteImage(fileName);
			}
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

			const ExtMesh *mesh = renderConfig->scene->extMeshCache.GetExtMesh(i);
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

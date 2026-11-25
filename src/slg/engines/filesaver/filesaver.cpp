/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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
#include <span>

#include <filesystem>
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

using namespace std::filesystem;
using namespace nlohmann;

//------------------------------------------------------------------------------
// Scene FileSaver render engine
//------------------------------------------------------------------------------

FileSaverRenderEngine::FileSaverRenderEngine(RenderConfigRef rcfg) :
		RenderEngine(rcfg) {
}

void FileSaverRenderEngine::InitFilm() {
	film->Init();
}

void FileSaverRenderEngine::StartLockLess() {
	auto& cfg = renderConfig.GetConfig();

	//--------------------------------------------------------------------------
	// Rendering parameters
	//--------------------------------------------------------------------------
	renderEngineType = cfg.Get(GetDefaultProps()->Get("filesaver.renderengine.type")).Get<string>();
	exportFormat = cfg.Get(GetDefaultProps()->Get("filesaver.format")).Get<string>();
	directoryName = cfg.Get(GetDefaultProps()->Get("filesaver.directory")).Get<string>();
	fileName = cfg.Get(GetDefaultProps()->Get("filesaver.filename")).Get<string>();

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

static string Base64Encode(std::span<const std::byte> bytes) {
	size_t size = bytes.size();
	stringstream ss;

	typedef boost::archive::iterators::base64_from_binary<
			boost::archive::iterators::transform_width<const char *, 6, 8>
	> base64_t;

	copy(base64_t(bytes.data()), base64_t(bytes.data() + size), boost::archive::iterators::ostream_iterator<char>(ss));

	// "===" padding is there as work around to Python base64.b64decode()
	// behavior. See https://gist.github.com/perrygeo/ee7c65bb1541ff6ac770

	return ss.str() + "===";
}

void FileSaverRenderEngine::ExportSceneGLTF(
	RenderConfigRef renderConfig,
	const string &fileName
) {
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

	const u_int sceneObjectsCount =  renderConfig.GetScene().GetObjects().GetSize();
	double lastPrint = WallClockTime();
	for (u_int i = 0; i < sceneObjectsCount; ++i) {
		if (WallClockTime() - lastPrint > 2.0) {
			SDL_LOG("  " << i << "/" << sceneObjectsCount);
			lastPrint = WallClockTime();
		}

		auto& scnObj = renderConfig.GetScene().GetObjects().GetSceneObject(i);
		auto& mesh = scnObj.GetExtMesh();
		// TODO: other mesh types
		if (mesh.GetType() != TYPE_EXT_TRIANGLE)
			continue;

		auto& triMesh = static_cast<const ExtTriangleMesh&>(mesh);

		//----------------------------------------------------------------------
		// Add vertices buffer

		const size_t encodedVerticesSize = sizeof(Point) * triMesh.GetTotalVertexCount();
		//const string encodedVertices = Base64Encode((const char *)triMesh.GetVertices(),
				//encodedVerticesSize);
		const string encodedVertices = Base64Encode(std::as_bytes(triMesh.GetVertices()));

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

		const BBox& meshBBox = triMesh.GetBBox();

		const u_int verticesAccessorIndex = j["accessors"].size();
		j["accessors"].push_back(json::object({
			{ "bufferView", j["bufferViews"].size() - 1 },
			{ "byteOffset", 0 },
			{ "componentType", 0x1406 }, // GL_FLOAT
			{ "count", triMesh.GetTotalVertexCount() },
			{ "type", "VEC3" },
			{ "max", { meshBBox.pMax.x, meshBBox.pMax.y, meshBBox.pMax.z } },
			{ "min", { meshBBox.pMin.x, meshBBox.pMin.y, meshBBox.pMin.z } }
		}));

		//----------------------------------------------------------------------
		// Add vertex UVs

		u_int vertexUVsAccessorIndex = NULL_INDEX;
		if (scnObj.HasBakeMap(COMBINED) && triMesh.HasUVs(scnObj.GetBakeMapUVIndex())) {

			const auto uvs = std::span(
				triMesh.GetAllUVs()[scnObj.GetBakeMapUVIndex()].get(),
				triMesh.GetTotalVertexCount()
			);
			const auto bytes = std::as_bytes(uvs);
			const string encodedVertexUVs = Base64Encode(bytes);

			j["buffers"].push_back(json::object({
				{ "uri", "data:application/octet-stream;base64," + encodedVertexUVs },
				{ "byteLength", bytes.size() }
			}));

			// Add vertex UVs buffer view

			j["bufferViews"].push_back(json::object({
				{ "buffer",  j["buffers"].size() - 1 },
				{ "byteOffset", 0 },
				{ "byteLength", bytes.size() },
				{ "target", 0x8892 }, // GL_ARRAY_BUFFER
			}));

			// Add vertex UVs accessor

			UV minUV(numeric_limits<float>::infinity(), numeric_limits<float>::infinity());
			UV maxUV(-numeric_limits<float>::infinity(), -numeric_limits<float>::infinity());
			UV *uv = triMesh.GetAllUVs()[scnObj.GetBakeMapUVIndex()].get();
			for (u_int uvIndex = 0; uvIndex < triMesh.GetTotalVertexCount(); ++uvIndex) {
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
				{ "count", triMesh.GetTotalVertexCount() },
				{ "type", "VEC2" },
				{ "max", { maxUV.u, maxUV.v } },
				{ "min", { minUV.u, minUV.v } }
			}));
		}

		//----------------------------------------------------------------------
		// Add triangle indices buffer

		// TODO
		//const auto tris = std::span(
			//triMesh.GetTriangles(),
			//triMesh.GetTotalTriangleCount()
		//);
		const auto tris = triMesh.GetTriangles();
		const auto bytes = std::as_bytes(tris);
		const string encodedTriangles = Base64Encode(bytes);

		j["buffers"].push_back(json::object({
			{ "uri", "data:application/octet-stream;base64," + encodedTriangles },
			{ "byteLength", bytes.size() }
		}));

		// Add triangle indices buffer view

		j["bufferViews"].push_back(json::object({
			{ "buffer", j["buffers"].size() - 1 },
			{ "byteOffset", 0 },
			{ "byteLength", bytes.size() },
			{ "target", 0x8893 }, // GL_ELEMENT_ARRAY_BUFFER
		}));

		// Add triangle indices accessor

		const u_int indicesAccessorIndex = j["accessors"].size();
		j["accessors"].push_back(json::object({
			{ "bufferView", j["bufferViews"].size() - 1 },
			{ "byteOffset", 0 },
			{ "componentType", 0x1405 }, // GL_UNSIGNED_INT
			{ "count", triMesh.GetTotalTriangleCount() * 3 },
			{ "type", "SCALAR" },
			{ "max", { triMesh.GetTotalVertexCount() - 1 } },
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
		if (scnObj.HasBakeMap(COMBINED) && triMesh.HasUVs(scnObj.GetBakeMapUVIndex())) {
			// Write the image to file

			auto imgMap = scnObj.GetBakeMap();
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
				{ "name", scnObj.GetName() + "Material" },
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
	
	// The use of std::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	std::ofstream gltfFile(std::filesystem::path(fileName),
			std::ofstream::out |
			std::ofstream::binary |
			std::ofstream::trunc);

	if(!gltfFile.is_open())
		throw std::runtime_error("Unable to open: " + fileName);

	gltfFile << std::setw(4) << j;
	if(!gltfFile.good())
		throw std::runtime_error("Error while writing file: " + fileName);

	gltfFile.close();
}

void FileSaverRenderEngine::ExportScene(RenderConfigRef renderConfig,
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

		auto& cfg = renderConfig.GetConfig();

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

		PropertiesUPtr props = renderConfig.GetScene().ToProperties(false);

		// Write the scene file
		props->Save(sceneFileName);
	}

	//--------------------------------------------------------------------------
	// Export the .ply/.exr/.png files
	//--------------------------------------------------------------------------
	{
		// Write the image map information
		SDL_LOG("Saving image maps information:");
		std::vector<std::reference_wrapper<const ImageMap>> ims;
		auto& scene = renderConfig.GetScene();
		scene.GetImageMaps().GetImageMaps(ims);
		for (u_int i = 0; i < ims.size(); ++i) {
			// Avoid to save ImageMapTexture::randomImageMap
			if (&ims[i].get() != scene.GetRandomImageMap().get()) {
				const std::string fileName =
					(dirPath / scene.GetImageMaps().GetSequenceFileName(ims[i])).generic_string();
				SDL_LOG("  " + fileName);
				ims[i].get().WriteImage(fileName);
			}
		}

		// Write the mesh information
		SDL_LOG("Saving meshes information:");
		const u_int meshCount =  renderConfig.GetScene().GetExtMeshes().GetSize();
		double lastPrint = WallClockTime();
		for (u_int i = 0; i < meshCount; ++i) {
			if (WallClockTime() - lastPrint > 2.0) {
				SDL_LOG("  " << i << "/" << meshCount);
				lastPrint = WallClockTime();
			}

			auto& mesh = renderConfig.GetScene().GetExtMeshes().GetExtMesh(i);
			// The only meshes I need to save are the real one. The others (instances, etc.)
			// will reference only true one.
			if (mesh.GetType() != TYPE_EXT_TRIANGLE)
				continue;

			const string fileName = (dirPath / renderConfig.GetScene().GetExtMeshes().GetSequenceFileName(mesh)).generic_string();

			//SDL_LOG("  " + fileName);
			mesh.Save(fileName);
		}
	}
}

//------------------------------------------------------------------------------
// Static methods used by RenderEngineRegistry
//------------------------------------------------------------------------------

PropertiesUPtr FileSaverRenderEngine::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	*props <<
				cfg.Get(GetDefaultProps()->Get("renderengine.type")) <<
			cfg.Get(GetDefaultProps()->Get("filesaver.format")) <<
			cfg.Get(GetDefaultProps()->Get("filesaver.directory")) <<
			cfg.Get(GetDefaultProps()->Get("filesaver.renderengine.type"));
	return props;
}

RenderEngine *FileSaverRenderEngine::FromProperties(RenderConfigRef rcfg) {
	return new FileSaverRenderEngine(rcfg);
}

PropertiesUPtr FileSaverRenderEngine::InitDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			Property("renderengine.type")(GetObjectTag()) <<
			Property("filesaver.format")("TXT") <<
			Property("filesaver.directory")("luxcore-exported-scene") <<
			Property("filesaver.filename")("luxcore-exported-scene.bcf") <<
			Property("filesaver.renderengine.type")("PATHCPU");

	return props;
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

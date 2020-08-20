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

#include "luxrays/utils/fileext.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/filmoutputs.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmOutputs
//------------------------------------------------------------------------------

FilmOutputs::FilmOutputs() {
	safeSave = true;
}

FilmOutputs::~FilmOutputs() {
}

void FilmOutputs::Reset() {
	types.clear();
	fileNames.clear();
	outputProps.clear();

	safeSave = true;
}

void FilmOutputs::Add(const FilmOutputType type, const string &fileName,
		const Properties *p) {
	types.push_back(type);
	fileNames.push_back(fileName);
	if (p)
		outputProps.push_back(*p);
	else
		outputProps.push_back(Properties());
}

Properties FilmOutputs::ToProperties(const Properties &cfg) {
	Properties props;

	props << cfg.Get(Property("film.outputs.safesave")(true));

	set<string> outputNames;
	vector<string> outputKeys = cfg.GetAllNames("film.outputs.");
	for (vector<string>::const_iterator outputKey = outputKeys.begin(); outputKey != outputKeys.end(); ++outputKey) {
		const string &key = *outputKey;
		const size_t dot1 = key.find(".", string("film.outputs.").length());
		if (dot1 == string::npos)
			continue;

		// Extract the output type name
		const string outputName = Property::ExtractField(key, 2);
		if (outputName == "")
			throw runtime_error("Syntax error in film output definition: " + outputName);

		if (outputNames.count(outputName) > 0)
			continue;

		outputNames.insert(outputName);
		const Property type = cfg.Get(Property("film.outputs." + outputName + ".type")("RGB_IMAGEPIPELINE"));
		const Property fileName = cfg.Get(Property("film.outputs." + outputName + ".filename")("image.png"));

//		// Check if it is a supported file format
//		FREE_IMAGE_FORMAT fif = FREEIMAGE_GETFIFFROMFILENAME(FREEIMAGE_CONVFILENAME(fileName).c_str());
//		if (fif == FIF_UNKNOWN)
//			throw runtime_error("Unknown image format in film output: " + outputName);

		// HDR image or not
		bool hdrImage = false;
        const string fileExtension = GetFileNameExt(fileName.Get<string>());
		if (fileExtension == ".exr" || fileExtension == ".hdr")
			hdrImage = true;

		switch (String2FilmOutputType(type.Get<string>())) {
			case RGB: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Not tonemapped image can be saved only in HDR formats: " + outputName);
				break;
			}
			case RGBA: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Not tonemapped image can be saved only in HDR formats: " + outputName);
				break;
			}
			case RGB_IMAGEPIPELINE: {
				const Property imagePipelineIndex = cfg.Get(Property("film.outputs." + outputName + ".index")(0));
				props << type << fileName << imagePipelineIndex;
				break;
			}
			case RGBA_IMAGEPIPELINE: {
				const Property imagePipelineIndex = cfg.Get(Property("film.outputs." + outputName + ".index")(0));
				props << type << fileName << imagePipelineIndex;
				break;
			}
			case ALPHA: {
				props << type << fileName;
				break;
			}
			case DEPTH: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Depth image can be saved only in HDR formats: " + outputName);
				break;
			}
			case POSITION: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Position image can be saved only in HDR formats: " + outputName);
				break;
			}
			case GEOMETRY_NORMAL: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Geometry normal image can be saved only in HDR formats: " + outputName);
				break;
			}
			case SHADING_NORMAL: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Shading normal image can be saved only in HDR formats: " + outputName);
				break;
			}
			case MATERIAL_ID: {
				if (!hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Material ID image can be saved only in non HDR formats: " + outputName);
				break;
			}
			case DIRECT_DIFFUSE: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Direct diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case DIRECT_DIFFUSE_REFLECT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Direct diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case DIRECT_DIFFUSE_TRANSMIT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Direct diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case DIRECT_GLOSSY: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Direct glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case DIRECT_GLOSSY_REFLECT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Direct glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case DIRECT_GLOSSY_TRANSMIT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Direct glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case EMISSION: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Emission image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_DIFFUSE: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_DIFFUSE_REFLECT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_DIFFUSE_TRANSMIT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect diffuse image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_GLOSSY: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_GLOSSY_REFLECT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_GLOSSY_TRANSMIT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect glossy image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_SPECULAR: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect specular image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_SPECULAR_REFLECT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect specular image can be saved only in HDR formats: " + outputName);
				break;
			}
			case INDIRECT_SPECULAR_TRANSMIT: {
				if (hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Indirect specular image can be saved only in HDR formats: " + outputName);
				break;
			}
			case MATERIAL_ID_MASK: {
				const Property materialID = cfg.Get(Property("film.outputs." + outputName + ".id")(255));
				props << type << fileName << materialID;
				break;
			}
			case DIRECT_SHADOW_MASK: {
				props << type << fileName;
				break;
			}
			case INDIRECT_SHADOW_MASK: {
				props << type << fileName;
				break;
			}
			case RADIANCE_GROUP: {
				const Property lightID = cfg.Get(Property("film.outputs." + outputName + ".id")(0));
				props << type << fileName << lightID;
				break;
			}
			case UV: {
				props << type << fileName;
				break;
			}
			case RAYCOUNT: {
				props << type << fileName;
				break;
			}
			case BY_MATERIAL_ID: {
				const Property materialID = cfg.Get(Property("film.outputs." + outputName + ".id")(255));
				props << type << fileName << materialID;
				break;
			}
			case IRRADIANCE: {
				props << type << fileName;
				break;
			}
			case OBJECT_ID: {
				if (!hdrImage)
					props << type << fileName;
				else
					throw runtime_error("Object ID image can be saved only in non HDR formats: " + outputName);
				break;
			}
			case OBJECT_ID_MASK: {
				const Property materialID = cfg.Get(Property("film.outputs." + outputName + ".id")(255));
				props << type << fileName << materialID;
				break;
			}
			case BY_OBJECT_ID: {
				const Property materialID = cfg.Get(Property("film.outputs." + outputName + ".id")(255));
				props << type << fileName << materialID;
				break;
			}
			case SAMPLECOUNT: {
				props << type << fileName;
				break;
			}
			case CONVERGENCE: {
				props << type << fileName;
				break;
			}
			case SERIALIZED_FILM: {
				props << type << fileName;
				break;
			}
			case MATERIAL_ID_COLOR: {
				props << type << fileName;
				break;
			}
			case ALBEDO: {
				props << type << fileName;
				break;
			}
			case AVG_SHADING_NORMAL: {
				props << type << fileName;
				break;
			}
			case NOISE: {
				props << type << fileName;
				break;
			}
			case USER_IMPORTANCE: {
				props << type << fileName;
				break;
			}
			default:
				throw runtime_error("Unknown film output type: " + type.Get<string>());
		}
	}

	return props;
}

FilmOutputs::FilmOutputType FilmOutputs::String2FilmOutputType(const string &type) {
	if (type == "RGB")
		return RGB;
	else if (type == "RGBA")
		return RGBA;
	else if ((type == "RGB_IMAGEPIPELINE") || (type == "RGB_TONEMAPPED"))
		return RGB_IMAGEPIPELINE;
	else if ((type == "RGBA_IMAGEPIPELINE") || (type == "RGBA_TONEMAPPED"))
		return RGBA_IMAGEPIPELINE;
	else if (type == "ALPHA")
		return ALPHA;
	else if (type == "DEPTH")
		return DEPTH;
	else if (type == "POSITION")
		return POSITION;
	else if (type == "GEOMETRY_NORMAL")
		return GEOMETRY_NORMAL;
	else if (type == "SHADING_NORMAL")
		return SHADING_NORMAL;
	else if (type == "MATERIAL_ID")
		return MATERIAL_ID;
	else if (type == "DIRECT_DIFFUSE")
		return DIRECT_DIFFUSE;
	else if (type == "DIRECT_DIFFUSE_REFLECT")
		return DIRECT_DIFFUSE_REFLECT;
	else if (type == "DIRECT_DIFFUSE_TRANSMIT")
		return DIRECT_DIFFUSE_TRANSMIT;
	else if (type == "DIRECT_GLOSSY")
		return DIRECT_GLOSSY;
	else if (type == "DIRECT_GLOSSY_REFLECT")
		return DIRECT_GLOSSY_REFLECT;
	else if (type == "DIRECT_GLOSSY_TRANSMIT")
		return DIRECT_GLOSSY_TRANSMIT;
	else if (type == "EMISSION")
		return EMISSION;
	else if (type == "INDIRECT_DIFFUSE")
		return INDIRECT_DIFFUSE;
	else if (type == "INDIRECT_DIFFUSE_REFLECT")
		return INDIRECT_DIFFUSE_REFLECT;
	else if (type == "INDIRECT_DIFFUSE_TRANSMIT")
		return INDIRECT_DIFFUSE_TRANSMIT;
	else if (type == "INDIRECT_GLOSSY")
		return INDIRECT_GLOSSY;
	else if (type == "INDIRECT_GLOSSY_REFLECT")
		return INDIRECT_GLOSSY_REFLECT;
	else if (type == "INDIRECT_GLOSSY_TRANSMIT")
		return INDIRECT_GLOSSY_TRANSMIT;
	else if (type == "INDIRECT_SPECULAR")
		return INDIRECT_SPECULAR;
	else if (type == "INDIRECT_SPECULAR_REFLECT")
		return INDIRECT_SPECULAR_REFLECT;
	else if (type == "INDIRECT_SPECULAR_TRANSMIT")
		return INDIRECT_SPECULAR_TRANSMIT;
	else if (type == "MATERIAL_ID_MASK")
		return MATERIAL_ID_MASK;
	else if (type == "DIRECT_SHADOW_MASK")
		return DIRECT_SHADOW_MASK;
	else if (type == "INDIRECT_SHADOW_MASK")
		return INDIRECT_SHADOW_MASK;
	else if (type == "RADIANCE_GROUP")
		return RADIANCE_GROUP;
	else if (type == "UV")
		return UV;
	else if (type == "RAYCOUNT")
		return RAYCOUNT;
	else if (type == "BY_MATERIAL_ID")
		return BY_MATERIAL_ID;
	else if (type == "IRRADIANCE")
		return IRRADIANCE;
	else if (type == "OBJECT_ID")
		return OBJECT_ID;
	else if (type == "OBJECT_ID_MASK")
		return OBJECT_ID_MASK;
	else if (type == "BY_OBJECT_ID")
		return BY_OBJECT_ID;
	else if (type == "SAMPLECOUNT")
		return SAMPLECOUNT;
	else if (type == "CONVERGENCE")
		return CONVERGENCE;
	else if (type == "SERIALIZED_FILM")
		return SERIALIZED_FILM;
	else if (type == "MATERIAL_ID_COLOR")
		return MATERIAL_ID_COLOR;
	else if (type == "ALBEDO")
		return ALBEDO;
	else if (type == "AVG_SHADING_NORMAL")
		return AVG_SHADING_NORMAL;
	else if (type == "NOISE")
		return NOISE;
	else if (type == "USER_IMPORTANCE")
		return USER_IMPORTANCE;
	else
		throw runtime_error("Unknown film output type: " + type);
}

const string FilmOutputs::FilmOutputType2String(const FilmOutputs::FilmOutputType type) {
	switch (type) {
		case RGB:
			return "RGB";
		case RGBA:
			return "RGBA";
		case RGB_IMAGEPIPELINE:
			return "RGB_IMAGEPIPELINE";
		case RGBA_IMAGEPIPELINE:
			return "RGBA_IMAGEPIPELINE";
		case ALPHA:
			return "ALPHA";
		case DEPTH:
			return "DEPTH";
		case POSITION:
			return "POSITION";
		case GEOMETRY_NORMAL:
			return "GEOMETRY_NORMAL";
		case SHADING_NORMAL:
			return "SHADING_NORMAL";
		case MATERIAL_ID:
			return "MATERIAL_ID";
		case DIRECT_DIFFUSE:
			return "DIRECT_DIFFUSE";
		case DIRECT_DIFFUSE_REFLECT:
			return "DIRECT_DIFFUSE_REFLECT";
		case DIRECT_DIFFUSE_TRANSMIT:
			return "DIRECT_DIFFUSE_TRANSMIT";
		case DIRECT_GLOSSY:
			return "DIRECT_GLOSSY";
		case DIRECT_GLOSSY_REFLECT:
			return "DIRECT_GLOSSY_REFLECT";
		case DIRECT_GLOSSY_TRANSMIT:
			return "DIRECT_GLOSSY_TRANSMIT";
		case EMISSION:
			return "EMISSION";
		case INDIRECT_DIFFUSE:
			return "INDIRECT_DIFFUSE";
		case INDIRECT_DIFFUSE_REFLECT:
			return "INDIRECT_DIFFUSE_REFLECT";
		case INDIRECT_DIFFUSE_TRANSMIT:
			return "INDIRECT_DIFFUSE_TRANSMIT";
		case INDIRECT_GLOSSY:
			return "INDIRECT_GLOSSY";
		case INDIRECT_GLOSSY_REFLECT:
			return "INDIRECT_GLOSSY_REFLECT";
		case INDIRECT_GLOSSY_TRANSMIT:
			return "INDIRECT_GLOSSY_TRANSMIT";
		case INDIRECT_SPECULAR:
			return "INDIRECT_SPECULAR";
		case INDIRECT_SPECULAR_REFLECT:
			return "INDIRECT_SPECULAR_REFLECT";
		case INDIRECT_SPECULAR_TRANSMIT:
			return "INDIRECT_SPECULAR_TRANSMIT";
		case MATERIAL_ID_MASK:
			return "MATERIAL_ID_MASK";
		case DIRECT_SHADOW_MASK:
			return "DIRECT_SHADOW_MASK";
		case INDIRECT_SHADOW_MASK:
			return "INDIRECT_SHADOW_MASK";
		case RADIANCE_GROUP:
			return "RADIANCE_GROUP";
		case UV:
			return "UV";
		case RAYCOUNT:
			return "RAYCOUNT";
		case BY_MATERIAL_ID:
			return "BY_MATERIAL_ID";
		case IRRADIANCE:
			return "IRRADIANCE";
		case OBJECT_ID:
			return "OBJECT_ID";
		case OBJECT_ID_MASK:
			return "OBJECT_ID_MASK";
		case BY_OBJECT_ID:
			return "BY_OBJECT_ID";
		case SAMPLECOUNT:
			return "SAMPLECOUNT";
		case CONVERGENCE:
			return "CONVERGENCE";
		case SERIALIZED_FILM:
			return "SERIALIZED_FILM";
		case MATERIAL_ID_COLOR:
			return "MATERIAL_ID_COLOR";
		case ALBEDO:
			return "ALBEDO";
		case AVG_SHADING_NORMAL:
			return "AVG_SHADING_NORMAL";
		case NOISE:
			return "NOISE";
		case USER_IMPORTANCE:
			return "USER_IMPORTANCE";
		default:
			throw runtime_error("Unknown film output type: " + ToString(type));
	}
}

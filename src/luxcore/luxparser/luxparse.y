/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

%name-prefix "luxcore_parserlxs_yy"

%{

#include <stdarg.h>
#include <sstream>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/replace.hpp>
	
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

namespace luxcore { namespace parselxs {

Properties *renderConfigProps = NULL;
Properties *sceneProps = NULL;

Properties overwriteProps;
Transform worldToCamera;

static string GetLuxCoreValidName(const string &name) {
	string validName = name;

	// Replace any "." in the name with 2x"__"
	boost::replace_all(validName, ".", "__");

	return validName;
}

// This is a copy of slg::FresnelPreset() but with different behavior when the
// preset is unknown.
void FresnelPreset(const string &presetName, Spectrum *eta, Spectrum *k) {
	if (presetName == "amorphous carbon") {
		*eta = Spectrum(2.94553471f, 2.22816062f, 1.98665321f);
		*k = Spectrum(.876640677f, .799504995f, .821194172f);
	} else if (presetName == "silver") {
		*eta = Spectrum(.155706137f, .115924977f, .138897374f);
		*k = Spectrum(4.88647795f, 3.12787175f, 2.17797375f);
	} else if (presetName == "gold") {
		*eta = Spectrum(.117958963f, .354153246f, 1.4389739f);
		*k = Spectrum(4.03164577f, 2.39416027f, 1.61966884f);
	} else if (presetName == "copper") {
		*eta = Spectrum(.134794354f, .928983212f, 1.10887861f);
		*k = Spectrum(3.98125982f, 2.440979f, 2.16473627f);
	} else if (presetName == "aluminium") {
		*eta = Spectrum(.69700277f, .879832864f, .5301736f);
		*k = Spectrum(9.30200672f, 6.27604008f, 4.89433956f);
	} else {
		LC_LOG("Unknown metal type '" << presetName << "'. Using default (aluminium).");

		*eta = Spectrum(.69700277f, .879832864f, .5301736f);
		*k = Spectrum(9.30200672f, 6.27604008f, 4.89433956f);
	}
}

//------------------------------------------------------------------------------
// RenderOptions
//------------------------------------------------------------------------------

class GraphicsState {
public:
	GraphicsState() {
		areaLightName = "";
		materialName = "";
		interiorVolumeName = "";
		exteriorVolumeName = "";
		currentLightGroup = 0;
	}
	~GraphicsState() {
	}

	string areaLightName, materialName, interiorVolumeName, exteriorVolumeName;
	Properties areaLightProps, materialProps, interiorVolumeProps, exteriorVolumeProps;

	u_int currentLightGroup;
};

// The GraphicsState stack
static vector<GraphicsState> graphicsStatesStack;
static GraphicsState currentGraphicsState;
// The transformations stack
static vector<Transform> transformsStack;
static Transform currentTransform;
// The named coordinate systems
static boost::unordered_map<string, Transform> namedCoordinateSystems;
// The light groups
static u_int freeLightGroupIndex;
static boost::unordered_map<string, u_int> namedLightGroups;
// The named Materials
static boost::unordered_map<string, Properties> namedMaterials;
// The named Volumes
static boost::unordered_map<string, Properties> namedVolumes;
// The named Textures
static boost::unordered_set<string> namedTextures;
// The named Object
static boost::unordered_map<string, vector<string> > namedObjectShapes;
static boost::unordered_map<string, vector<string> > namedObjectMaterials;
static boost::unordered_map<string, vector<Transform> > namedObjectTransforms;
static string currentObjectName;
static u_int freeObjectID, freeLightID;

void ResetParser() {
	overwriteProps.Clear();

	*renderConfigProps <<
			Property("opencl.platform.index")(-1) <<
			Property("opencl.cpu.use")(false) <<
			Property("opencl.gpu.use")(true) <<
			Property("renderengine.type")("PATHOCL") <<
			Property("sampler.type")("RANDOM") <<
			Property("film.filter.type")("BLACKMANHARRIS") <<
			Property("accelerator.instances.enable")(false);

	graphicsStatesStack.clear();
	currentGraphicsState = GraphicsState();
	transformsStack.clear();
	currentTransform = Transform();

	namedCoordinateSystems.clear();
	freeLightGroupIndex = 1;
	namedLightGroups.clear();
	namedMaterials.clear();
	namedVolumes.clear();
	namedTextures.clear();
	namedObjectShapes.clear();
	namedObjectMaterials.clear();
	namedObjectTransforms.clear();
	currentObjectName = "";
	freeObjectID = 0;
	freeLightID = 0;
}

static Properties GetTextureMapping2D(const string &prefix, const Properties &props) {
	const string type = props.Get(Property("mapping")("uv")).Get<string>();
	
	if (type == "uv") {
		return Property(prefix + ".mapping.type")("uvmapping2d") <<
				Property(prefix + ".mapping.uvscale")(props.Get(Property("uscale")(1.f)).Get<float>(),
					props.Get(Property("vscale")(1.f)).Get<float>()) <<
				Property(prefix + ".mapping.uvdelta")(props.Get(Property("udelta")(0.f)).Get<float>(),
					props.Get(Property("udelta")(0.f)).Get<float>());
	} else {
		LC_LOG("LuxCore supports only texture coordinate mapping 2D with 'uv' (i.e. not " << type << "). Ignoring the mapping.");
		return Properties();
	}
}

static Properties GetTextureMapping3D(const string &prefix, const Transform &tex2World, const Properties &props) {
	const string type = props.Get(Property("coordinates")("uv")).Get<string>();

	if (type == "uv") {
		return Property(prefix + ".mapping.type")("uvmapping3d") <<
				Property(prefix + ".mapping.transformation")(tex2World.mInv);
	} else if (type == "global") {
		return Property(prefix + ".mapping.type")("globalmapping3d") <<
				Property(prefix + ".mapping.transformation")(tex2World.mInv);
	} else {
		LC_LOG("LuxCore supports only texture coordinate mapping 3D with 'uv' and 'global' (i.e. not " << type << "). Ignoring the mapping.");
		return Properties();
	}
}

static Property GetTexture(const string &luxCoreName, const Property defaultProp, const Properties &props) {
	Property prop = props.Get(defaultProp);
	if (prop.GetValueType(0) == PropertyValue::STRING_VAL) {
		// It is a texture name
		string texName = GetLuxCoreValidName(prop.Get<string>());

		return Property(luxCoreName)(texName);
	} else
		return prop.Renamed(luxCoreName);
}

static void DefineMaterial(const string &name, const Properties &matProps,
		const Properties &lightProps,
		const string &interiorVolumeName, const string &exteriorVolumeName) {
	const string prefix = "scene.materials." + name;

	//--------------------------------------------------------------------------
	// Material definition
	//--------------------------------------------------------------------------

	const string type = matProps.Get(Property("type")("matte")).Get<string>();
	if (type == "matte") {
		if (matProps.IsDefined("sigma")) {
			*sceneProps <<
				Property(prefix + ".type")("roughmatte") <<
				GetTexture(prefix + ".kd", Property("Kd")(Spectrum(.9f)), matProps) <<
				GetTexture(prefix + ".sigma", Property("sigma")(0.f), matProps);
		} else {
			*sceneProps <<
				Property(prefix + ".type")("matte") <<
				GetTexture(prefix + ".kd", Property("Kd")(Spectrum(.9f)), matProps);
		}
	} else if (type == "mirror") {
		*sceneProps <<
				Property(prefix + ".type")("mirror") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), matProps);
	} else if (type == "glass") {
		const bool isArchitectural = matProps.Get(Property("architectural")(false)).Get<bool>();

		*sceneProps <<
				Property(prefix + ".type")(isArchitectural ? "archglass" : "glass") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), matProps) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), matProps) <<
				Property(prefix +".ioroutside")(1.f) <<
				GetTexture(prefix + ".iorinside", Property("index")(1.5f), matProps);
	} else if (type == "metal") {
		string presetName = matProps.Get("name").Get<string>();
		Spectrum n, k;
		FresnelPreset(presetName, &n, &k);

		*sceneProps <<
				Property(prefix + ".type")("metal2") <<
				Property(prefix + ".n")(n) <<
				Property(prefix + ".k")(k) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps);
	} else if (type == "mattetranslucent") {
		if (matProps.Get(Property("energyconserving")(false)).Get<bool>() == false)
			LC_LOG("Mattetranslucent with energyconserving=false is not supported, using energyconservin.");
		if (matProps.IsDefined("sigma")) {
			*sceneProps <<
				Property(prefix + ".type")("roughmattetranslucent") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(.9f)), matProps) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), matProps) <<
				GetTexture(prefix + ".sigma", Property("sigma")(0.f), matProps);
		} else {
			*sceneProps <<
				Property(prefix + ".type")("mattetranslucent") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), matProps) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), matProps);
		}
	} else if (type == "null") {
		*sceneProps <<
				Property(prefix + ".type")("null");
	} else if (type == "mix") {
		*sceneProps <<
				Property(prefix + ".type")("mix") <<
				GetTexture(prefix + ".amount", Property("amount")(.5f), matProps) <<
				Property(prefix + ".material1")(GetLuxCoreValidName(matProps.Get(Property("namedmaterial1")("")).Get<string>())) <<
				Property(prefix + ".material2")(GetLuxCoreValidName(matProps.Get(Property("namedmaterial2")("")).Get<string>()));
	} else if (type == "glossy") {
		if (matProps.Get(Property("separable")(true)).Get<bool>()) {
			if (matProps.IsDefined("sigma")) {
				*sceneProps <<
					Property(prefix + "~~base.type")("roughmatte") <<
					GetTexture(prefix + "~~base.kd", Property("Kd")(Spectrum(.9f)), matProps) <<
					GetTexture(prefix + "~~base.sigma", Property("sigma")(0.f), matProps);
			} else {
				*sceneProps <<
					Property(prefix + "~~base.type")("matte") <<
					GetTexture(prefix + "~~base.kd", Property("Kd")(Spectrum(.9f)), matProps);
			}
			*sceneProps <<
					Property(prefix + ".type")("glossycoating") <<
					Property(prefix + ".base")(name + "~~base") <<
					GetTexture(prefix + ".ks", Property("Ks")(.5f), matProps) <<
					GetTexture(prefix + ".ka", Property("Ka")(0.f), matProps) <<
					GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
					GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps) <<
					GetTexture(prefix + ".d", Property("d")(0.f), matProps) <<
					GetTexture(prefix + ".index", Property("index")(0.f), matProps) <<
					Property(prefix +".multibounce")(matProps.Get(Property("multibounce")(false)).Get<bool>());
		} else {
			*sceneProps <<
					Property(prefix + ".type")("glossy2") <<
					GetTexture(prefix + ".kd", Property("Kd")(.5f), matProps) <<
					GetTexture(prefix + ".ks", Property("Ks")(.5f), matProps) <<
					GetTexture(prefix + ".ka", Property("Ka")(0.f), matProps) <<
					GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
					GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps) <<
					GetTexture(prefix + ".d", Property("d")(0.f), matProps) <<
					GetTexture(prefix + ".index", Property("index")(0.f), matProps) <<
					Property(prefix +".multibounce")(matProps.Get(Property("multibounce")(false)).Get<bool>());
		}
	} else if (type == "metal2") {
		const string fresnelTexName = GetLuxCoreValidName(matProps.Get(Property("fresnel")("")).Get<string>());
		if (fresnelTexName == "") {
			const string texPrefix = "scene.textures." + name;
			*sceneProps <<
					Property(texPrefix + "_LUXCORE_PARSERLXS_fresnelapproxn.type")("fresnelapproxn") <<
					GetTexture(texPrefix + "_LUXCORE_PARSERLXS_fresnelapproxn.texture", Property("Kr")(Spectrum(.5f)), matProps) <<
					Property(texPrefix + "_LUXCORE_PARSERLXS_fresnelapproxk.type")("fresnelapproxk") <<
					GetTexture(texPrefix + "_LUXCORE_PARSERLXS_fresnelapproxk.texture", Property("Kr")(Spectrum(.5f)), matProps);
					Property(prefix + ".type")("metal2") <<
					Property(prefix + ".n")(fresnelTexName + "_LUXCORE_PARSERLXS_fresnelapproxn") <<
					Property(prefix + ".k")(fresnelTexName + "_LUXCORE_PARSERLXS_fresnelapproxk");
		} else {
			*sceneProps <<
					Property(prefix + ".type")("metal2") <<
					Property(prefix + ".fresnel")(fresnelTexName);
		}

		*sceneProps <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps);
	} else if (type == "roughglass") {
		*sceneProps <<
				Property(prefix + ".type")("roughglass") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), matProps) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), matProps) <<
				Property(prefix +".ioroutside")(1.f) <<
				GetTexture(prefix + ".iorinside", Property("index")(1.5f), matProps) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps);
	} else if (type == "velvet") {
		*sceneProps <<
				Property(prefix + ".type")("velvet") <<
				GetTexture(prefix + ".kd", Property("Kd")(Spectrum(1.f)), matProps) <<
				GetTexture(prefix + ".p1", Property("p1")(2.0f), matProps) <<
				GetTexture(prefix + ".p2", Property("p2")(20.0f), matProps) <<
				GetTexture(prefix + ".p3", Property("p3")(-2.0f), matProps);
				GetTexture(prefix + ".thickness", Property("thickness")(.1f), matProps);
	} else if (type == "cloth") {
		*sceneProps <<
				Property(prefix + ".type")("cloth") <<
				Property(prefix + ".preset")(matProps.Get("presetname").Get<string>()) <<
				GetTexture(prefix + ".weft_kd", Property("weft_kd")(Spectrum(.5f)), matProps) <<
				GetTexture(prefix + ".weft_ks", Property("weft_ks")(Spectrum(.5f)), matProps) <<
				GetTexture(prefix + ".warp_kd", Property("weft_kd")(Spectrum(.5f)), matProps) <<
				GetTexture(prefix + ".warp_ks", Property("weft_ks")(Spectrum(.5f)), matProps) <<
				Property(prefix + ".repeat_u")(matProps.Get(Property("repeat_u")(100.f)).Get<float>()) <<
				Property(prefix + ".repeat_v")(matProps.Get(Property("repeat_v")(100.f)).Get<float>());
	} else if (type == "carpaint") {
		*sceneProps <<
				Property(prefix + ".type")("carpaint") <<
				GetTexture(prefix + ".ka", Property("Ka")(Spectrum(0.f)), matProps) <<
				GetTexture(prefix + ".d", Property("d")(0.f), matProps);
		if (matProps.IsDefined("name"))
			*sceneProps << Property(prefix + ".preset")(matProps.Get("name").Get<string>());
		if (matProps.IsDefined("Kd"))
			*sceneProps << GetTexture(prefix + ".kd", Property("Kd")(Spectrum(0.f)), matProps);
		if (matProps.IsDefined("Ks1"))
			*sceneProps << GetTexture(prefix + ".ks1", Property("Ks1")(Spectrum(0.f)), matProps);
		if (matProps.IsDefined("Ks2"))
			*sceneProps << GetTexture(prefix + ".ks2", Property("Ks2")(Spectrum(0.f)), matProps);
		if (matProps.IsDefined("Ks3"))
			*sceneProps << GetTexture(prefix + ".ks3", Property("Ks3")(Spectrum(0.f)), matProps);
		if (matProps.IsDefined("R1"))
			*sceneProps << GetTexture(prefix + ".r1", Property("R1")(0.f), matProps);
		if (matProps.IsDefined("R2"))
			*sceneProps << GetTexture(prefix + ".r2", Property("R2")(0.f), matProps);
		if (matProps.IsDefined("R3"))
			*sceneProps << GetTexture(prefix + ".r3", Property("R3")(0.f), matProps);
		if (matProps.IsDefined("M1"))
			*sceneProps << GetTexture(prefix + ".m1", Property("M1")(0.f), matProps);
		if (matProps.IsDefined("M2"))
			*sceneProps << GetTexture(prefix + ".m2", Property("M2")(0.f), matProps);
		if (matProps.IsDefined("M3"))
			*sceneProps << GetTexture(prefix + ".m3", Property("M3")(0.f), matProps);
	} else if (type == "glossytranslucent") {
		*sceneProps <<
				Property(prefix + ".type")("glossytranslucent") <<
				GetTexture(prefix + ".kd", Property("Kd")(.5f), matProps) <<
				GetTexture(prefix + ".kt", Property("Kt")(.5f), matProps) <<
				GetTexture(prefix + ".ks", Property("Ks")(.5f), matProps) <<
				GetTexture(prefix + ".ka", Property("Ka")(0.f), matProps) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps) <<
				GetTexture(prefix + ".d", Property("d")(0.f), matProps) <<
				GetTexture(prefix + ".index", Property("index")(0.f), matProps) <<
				Property(prefix +".multibounce")(matProps.Get(Property("multibounce")(false)).Get<bool>());
		if (matProps.Get(Property("onesided")(true)).Get<bool>()) {
			*sceneProps <<
				GetTexture(prefix + ".ks_bf", Property("Ks")(.5f), matProps) <<
				GetTexture(prefix + ".ka_bf", Property("Ka")(0.f), matProps) <<
				GetTexture(prefix + ".uroughness_bf", Property("uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness_bf", Property("vroughness")(.1f), matProps) <<
				GetTexture(prefix + ".d_bf", Property("d")(0.f), matProps) <<
				GetTexture(prefix + ".index_bf", Property("index")(0.f), matProps) <<
				Property(prefix +".multibounce_bf")(matProps.Get(Property("multibounce")(false)).Get<bool>());
		} else {
			*sceneProps <<
				GetTexture(prefix + ".ks_bf", Property("backface_Ks")(.5f), matProps) <<
				GetTexture(prefix + ".ka_bf", Property("backface_Ka")(0.f), matProps) <<
				GetTexture(prefix + ".uroughness_bf", Property("backface_uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness_bf", Property("backface_vroughness")(.1f), matProps) <<
				GetTexture(prefix + ".d_bf", Property("backface_d")(0.f), matProps) <<
				GetTexture(prefix + ".index_bf", Property("backface_index")(0.f), matProps) <<
				Property(prefix +".multibounce_bf")(matProps.Get(Property("backface_multibounce")(false)).Get<bool>());
		}
	} else if (type == "glossycoating") {
		*sceneProps <<
				Property(prefix + ".type")("glossycoating") <<
				Property(prefix + ".base")(GetLuxCoreValidName(matProps.Get(Property("basematerial")("")).Get<string>())) <<
				GetTexture(prefix + ".ks", Property("Ks")(.5f), matProps) <<
				GetTexture(prefix + ".ka", Property("Ka")(0.f), matProps) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), matProps) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), matProps) <<
				GetTexture(prefix + ".d", Property("d")(0.f), matProps) <<
				GetTexture(prefix + ".index", Property("index")(0.f), matProps) <<
				Property(prefix +".multibounce")(matProps.Get(Property("multibounce")(false)).Get<bool>());
	} else {
		LC_LOG("LuxCore::ParserLXS supports only Matte, Mirror, Glass, Metal, MatteTranslucent, Null, "
				"Mix, Glossy2, Metal2, RoughGlass, Velvet, Cloth, Carpaint, GlossyTranslucent "
				"and GlossyCoating materials (i.e. not " <<
				type << "). Replacing an unsupported material with matte.");

		*sceneProps <<
				Property(prefix + ".type")("matte") <<
				Property(prefix + ".kd")(Spectrum(.9f));
	}

	//--------------------------------------------------------------------------
	// Material bump mapping and normal mapping definition
	//--------------------------------------------------------------------------

	if (matProps.IsDefined("bumpmap")) {
		// TODO: check the kind of texture to understand if it is a bump map or a normal map
		*sceneProps << Property(prefix + ".bumptex")(GetLuxCoreValidName(matProps.Get("bumpmap").Get<string>()));
		//*sceneProps << Property(prefix + ".normaltex")(props.Get("bumpmap").Get<string>());
	}

	//--------------------------------------------------------------------------
	// Material emission definition
	//--------------------------------------------------------------------------

	if (lightProps.GetSize() > 0) {
		*sceneProps <<
				GetTexture(prefix + ".emission", Property("L")(Spectrum(1.f)), lightProps) << 
				Property(prefix + ".emission.gain")(Spectrum(lightProps.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".emission.power")(lightProps.Get(Property("power")(100.f)).Get<float>()) <<
				Property(prefix + ".emission.efficency")(lightProps.Get(Property("efficency")(17.f)).Get<float>()) <<
				Property(prefix + ".emission.id")(currentGraphicsState.currentLightGroup);
				
				// Only define the .emission.iesfile and .emission.mapfile props if we already have values for them.
				if (lightProps.IsDefined("iesname")) {
					*sceneProps << 
						Property(prefix + ".emission.iesfile")(lightProps.Get("iesname").Get<string>()) <<
						Property(prefix + ".emission.flipz")(lightProps.Get(Property("flipz")(false)).Get<bool>());
				}
				if (lightProps.IsDefined("mapname")) {
					*sceneProps << Property(prefix + ".emission.mapfile")(lightProps.Get("mapname").Get<string>());
				}
	}

	//--------------------------------------------------------------------------
	// Material volumes definition
	//--------------------------------------------------------------------------

	if (interiorVolumeName != "") {
		*sceneProps <<
				Property(prefix + ".volume.interior")(interiorVolumeName);
	}
	if (exteriorVolumeName != "") {
		*sceneProps <<
				Property(prefix + ".volume.exterior")(exteriorVolumeName);
	}
}

static void DefineVolume(const string &name, const Properties &volProps) {
	const string prefix = "scene.volumes." + name;

	//--------------------------------------------------------------------------
	// Volume definition
	//--------------------------------------------------------------------------

	const string type = volProps.Get(Property("type")("homogenous")).Get<string>();
	if (type == "clear") {
		*sceneProps <<
				Property(prefix + ".type")("clear") <<
				GetTexture(prefix + ".absorption", Property("absorption")(0.f, 0.f, 0.f), volProps);
	} else if (type == "homogenous") {
		*sceneProps <<
				Property(prefix + ".type")("homogenous") <<
				GetTexture(prefix + ".absorption", Property("sigma_a")(0.f, 0.f, 0.f), volProps) <<
				GetTexture(prefix + ".scattering", Property("sigma_s")(0.f, 0.f, 0.f), volProps) <<
				GetTexture(prefix + ".asymmetry", Property("g")(0.f, 0.f, 0.f), volProps);
	} else if (type == "heterogeneous") {
		*sceneProps <<
				Property(prefix + ".type")("heterogeneous") <<
				GetTexture(prefix + ".absorption", Property("sigma_a")(0.f, 0.f, 0.f), volProps) <<
				GetTexture(prefix + ".scattering", Property("sigma_s")(0.f, 0.f, 0.f), volProps) <<
				GetTexture(prefix + ".asymmetry", Property("g")(0.f, 0.f, 0.f), volProps) <<
				GetTexture(prefix + ".steps.size", Property("stepsize")(1.f), volProps);
	} else {
		LC_LOG("LuxCore::ParserLXS supports clear, homogenous "
				"and heterogeneous volumes (i.e. not " <<
				type << "). Replacing an unsupported material with a clear volume.");

		*sceneProps <<
				Property(prefix + ".type")("clear");
	}
}

u_int lineNum = 0;
string currentFile;

class ParamListElem {
public:
	ParamListElem() : token(NULL), arg(NULL), size(0), textureHelper(false)
	{ }
	const char *token;
	void *arg;
	u_int size;
	bool textureHelper;
};
u_int curParamlistAllocated = 0;
u_int curParamlistSize = 0;
ParamListElem *curParamlists = NULL;

#define CPAL curParamlistAllocated
#define CPS curParamlistSize
#define CP curParamlists
#define CPT(__n) curParamlists[(__n)].token
#define CPA(__n) curParamlists[(__n)].arg
#define CPSZ(__n) curParamlists[(__n)].size
#define CPTH(__n) curParamlists[(__n)].textureHelper

class ParamArray {
public:
	ParamArray() : elementSize(0), allocated(0), nelems(0), array(NULL) { }
	u_int elementSize;
	u_int allocated;
	u_int nelems;
	void *array;
};

ParamArray *curArray = NULL;
bool arrayIsSingleString = false;

#define NA(r) ((float *) r->array)
#define SA(r) ((const char **) r->array)

void AddArrayElement(void *elem)
{
	if (curArray->nelems >= curArray->allocated) {
		curArray->allocated = 2 * curArray->allocated + 1;
		curArray->array = realloc(curArray->array,
			curArray->allocated * curArray->elementSize);
	}
	char *next = static_cast<char *>(curArray->array) +
		curArray->nelems * curArray->elementSize;
	memcpy(next, elem, curArray->elementSize);
	curArray->nelems++;
}

ParamArray *ArrayDup(ParamArray *ra)
{
	ParamArray *ret = new ParamArray;
	ret->elementSize = ra->elementSize;
	ret->allocated = ra->allocated;
	ret->nelems = ra->nelems;
	ret->array = malloc(ra->nelems * ra->elementSize);
	memcpy(ret->array, ra->array, ra->nelems * ra->elementSize);
	return ret;
}

void ArrayFree(ParamArray *ra)
{
	free(ra->array);
	delete ra;
}

void FreeArgs()
{
	for (u_int i = 0; i < CPS; ++i) {
		// NOTE - Ratow - freeing up strings inside string type args
		if (memcmp("string", CPT(i), 6) == 0 ||
			memcmp("texture", CPT(i), 7) == 0) {
			for (u_int j = 0; j < CPSZ(i); ++j)
				free(static_cast<char **>(CPA(i))[j]);
		}
		delete[] static_cast<char *>(CPA(i));
	}
}

static bool VerifyArrayLength(ParamArray *arr, u_int required,
	const char *command)
{
	if (arr->nelems != required) {
		LC_LOG(command << " requires a(n) "	<< required << " element array !");
		return false;
	}
	return true;
}

typedef enum {
	PARAM_TYPE_INT, PARAM_TYPE_BOOL, PARAM_TYPE_FLOAT,
	PARAM_TYPE_POINT, PARAM_TYPE_VECTOR, PARAM_TYPE_NORMAL,
	PARAM_TYPE_COLOR, PARAM_TYPE_STRING, PARAM_TYPE_TEXTURE
} ParamType;

static bool LookupType(const char *token, ParamType *type, string &name) {
	BOOST_ASSERT(token != NULL);
	*type = ParamType(0);
	const char *strp = token;
	while (*strp && isspace(*strp))
		++strp;
	if (!*strp) {
		LC_LOG("Parameter '" << token << "' doesn't have a type declaration ?!");
		name = string(token);
		return false;
	}
#define TRY_DECODING_TYPE(name, mask) \
	if (strncmp(name, strp, strlen(name)) == 0) { \
		*type = mask; strp += strlen(name); \
	}
	TRY_DECODING_TYPE("float", PARAM_TYPE_FLOAT)
	else TRY_DECODING_TYPE("integer", PARAM_TYPE_INT)
	else TRY_DECODING_TYPE("bool", PARAM_TYPE_BOOL)
	else TRY_DECODING_TYPE("point", PARAM_TYPE_POINT)
	else TRY_DECODING_TYPE("vector", PARAM_TYPE_VECTOR)
	else TRY_DECODING_TYPE("normal", PARAM_TYPE_NORMAL)
	else TRY_DECODING_TYPE("string", PARAM_TYPE_STRING)
	else TRY_DECODING_TYPE("texture", PARAM_TYPE_TEXTURE)
	else TRY_DECODING_TYPE("color", PARAM_TYPE_COLOR)
	else {
		LC_LOG("Unable to decode type for token '" << token << "'");
		name = string(token);
		return false;
	}
	while (*strp && isspace(*strp))
		++strp;
	name = string(strp);
	return true;
}

static void InitProperties(Properties &props, const u_int count, const ParamListElem *list) {
	for (u_int i = 0; i < count; ++i) {
		ParamType type;
		string name;
		if (!LookupType(list[i].token, &type, name)) {
			LC_LOG("Type of parameter '" << list[i].token << "' is unknown");
			continue;
		}
		if (list[i].textureHelper && type != PARAM_TYPE_TEXTURE &&
			type != PARAM_TYPE_STRING) {
			LC_LOG("Bad type for " << name << ". Changing it to a texture");
			type = PARAM_TYPE_TEXTURE;
		}

		Property prop(name);

		void *data = list[i].arg;
		u_int nItems = list[i].size;
		if (type == PARAM_TYPE_INT) {
			// parser doesn't handle ints, so convert from floats
			int *idata = new int[nItems];
			float *fdata = static_cast<float *>(data);
			for (u_int j = 0; j < nItems; ++j)
				idata[j] = static_cast<int>(fdata[j]);
			for (u_int j = 0; j < nItems; ++j)
				prop.Add(idata[j]);
			delete[] idata;
		} else if (type == PARAM_TYPE_BOOL) {
			// strings -> bools
			bool *bdata = new bool[nItems];
			for (u_int j = 0; j < nItems; ++j) {
				string s(*static_cast<const char **>(data));
				if (s == "true")
					bdata[j] = true;
				else if (s == "false")
					bdata[j] = false;
				else {
					LC_LOG("Value '" << s << "' unknown for boolean parameter '" << list[i].token << "'. Using 'false'");
					bdata[j] = false;
				}
			}
			for (u_int j = 0; j < nItems; ++j)
				prop.Add(bdata[j]);
			delete[] bdata;
		} else if (type == PARAM_TYPE_FLOAT) {
			const float *d = static_cast<float *>(data);
			for (u_int j = 0; j < nItems; ++j)
				prop.Add(d[j]);
		} else if (type == PARAM_TYPE_POINT) {
			const Point *d = static_cast<Point *>(data);
			for (u_int j = 0; j < nItems / 3; ++j)
				prop.Add(d[j]);
		} else if (type == PARAM_TYPE_VECTOR) {
			const Vector *d = static_cast<Vector *>(data);
			for (u_int j = 0; j < nItems / 3; ++j)
				prop.Add(d[j]);
		} else if (type == PARAM_TYPE_NORMAL) {
			const Normal *d = static_cast<Normal *>(data);
			for (u_int j = 0; j < nItems / 3; ++j)
				prop.Add(d[j]);
		} else if (type == PARAM_TYPE_COLOR) {
			const Spectrum *d = static_cast<Spectrum *>(data);
			for (u_int j = 0; j < nItems / 3; ++j)
				prop.Add(d[j]);
		} else if (type == PARAM_TYPE_STRING) {
			string *strings = new string[nItems];
			for (u_int j = 0; j < nItems; ++j)
				strings[j] = string(static_cast<const char **>(data)[j]);
			for (u_int j = 0; j < nItems; ++j)
				prop.Add(strings[j]);
			delete[] strings;
		} else if (type == PARAM_TYPE_TEXTURE) {
			if (nItems == 1) {
				string val(*static_cast<const char **>(data));
				prop.Add(val);
			} else {
				LC_LOG("Only one string allowed for 'texture' parameter " << name);
			}
		}

		props.Set(prop);
	}
}

} }

using namespace luxcore::parselxs;

//------------------------------------------------------------------------------

extern int yylex(void);

#define YYMAXDEPTH 100000000

void yyerror(const char *str)
{
	stringstream ss;
	ss << "Parsing error";
	if (currentFile != "")
		ss << " in file '" << currentFile << "'";
	if (lineNum > 0)
		ss << " at line " << lineNum;
	ss << ": " << str;
	LC_LOG(ss.str().c_str());
}

#define YYPRINT(file, type, value)  \
{ \
	if ((type) == ID || (type) == STRING) \
		fprintf((file), " %s", (value).string); \
	else if ((type) == NUM) \
		fprintf((file), " %f", (value).num); \
}

%}

%union {
char string[1024];
float num;
ParamArray *ribarray;
}

%token <string> STRING ID
%token <num> NUM
%token LBRACK RBRACK

%token ACCELERATOR AREALIGHTSOURCE ATTRIBUTEBEGIN ATTRIBUTEEND
%token CAMERA CONCATTRANSFORM COORDINATESYSTEM COORDSYSTRANSFORM EXTERIOR
%token FILM IDENTITY INTERIOR LIGHTSOURCE LOOKAT MATERIAL MAKENAMEDMATERIAL
%token MAKENAMEDVOLUME MOTIONBEGIN MOTIONEND NAMEDMATERIAL 
%token OBJECTBEGIN OBJECTEND OBJECTINSTANCE PORTALINSTANCE
%token MOTIONINSTANCE LIGHTGROUP PIXELFILTER RENDERER REVERSEORIENTATION ROTATE 
%token SAMPLER SCALE SEARCHPATH PORTALSHAPE SHAPE SURFACEINTEGRATOR TEXTURE
%token TRANSFORMBEGIN TRANSFORMEND TRANSFORM TRANSLATE VOLUME VOLUMEINTEGRATOR
%token WORLDBEGIN WORLDEND

%token HIGH_PRECEDENCE

%type<ribarray> array num_array string_array
%type<ribarray> real_num_array real_string_array
%%
start: ri_stmt_list
{
};

array_init: %prec HIGH_PRECEDENCE
{
	if (curArray)
		ArrayFree(curArray);
	curArray = new ParamArray;
	arrayIsSingleString = false;
};

string_array_init: %prec HIGH_PRECEDENCE
{
	curArray->elementSize = sizeof(const char *);
};

num_array_init: %prec HIGH_PRECEDENCE
{
	curArray->elementSize = sizeof(float);
};

array: string_array
{
	$$ = $1;
}
| num_array
{
	$$ = $1;
};

string_array: real_string_array
{
	$$ = $1;
}
| single_element_string_array
{
	$$ = ArrayDup(curArray);
	arrayIsSingleString = true;
};

real_string_array: array_init LBRACK string_list RBRACK
{
	$$ = ArrayDup(curArray);
};

single_element_string_array: array_init string_list_entry
{
};

string_list: string_list string_list_entry
{
}
| string_list_entry
{
};

string_list_entry: string_array_init STRING
{
	char *toAdd = strdup($2);
	AddArrayElement(&toAdd);
};

num_array: real_num_array
{
	$$ = $1;
}
| single_element_num_array
{
	$$ = ArrayDup(curArray);
};

real_num_array: array_init LBRACK num_list RBRACK
{
	$$ = ArrayDup(curArray);
};

single_element_num_array: array_init num_list_entry
{
};

num_list: num_list num_list_entry
{
}
| num_list_entry
{
};

num_list_entry: num_array_init NUM
{
	float toAdd = $2;
	AddArrayElement(&toAdd);
};

paramlist: paramlist_init paramlist_contents
{
};

paramlist_init: %prec HIGH_PRECEDENCE
{
	CPS = 0;
};

paramlist_contents: paramlist_entry paramlist_contents
{
}
|
{
};

paramlist_entry: STRING array
{
	void *arg = new char[$2->nelems * $2->elementSize];
	memcpy(arg, $2->array, $2->nelems * $2->elementSize);
	if (CPS >= CPAL) {
		CPAL = 2 * CPAL + 1;
		CP = static_cast<ParamListElem *>(realloc(CP,
			CPAL * sizeof(ParamListElem)));
	}
	CPT(CPS) = $1;
	CPA(CPS) = arg;
	CPSZ(CPS) = $2->nelems;
	CPTH(CPS) = arrayIsSingleString;
	++CPS;
	ArrayFree($2);
};

ri_stmt_list: ri_stmt_list ri_stmt
{
}
| ri_stmt
{
};

ri_stmt: ACCELERATOR STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	// Map kdtree and bvh to luxrays' bvh accel otherwise just use the default settings
	const string name($2);
	if ((name =="kdtree") || (name =="bvh"))
		*renderConfigProps << Property("accelerator.type")("BVH");
		
	FreeArgs();
}
| AREALIGHTSOURCE STRING paramlist
{
	currentGraphicsState.areaLightName = $2;
	InitProperties(currentGraphicsState.areaLightProps, CPS, CP);

	FreeArgs();
}
| ATTRIBUTEBEGIN
{
	graphicsStatesStack.push_back(currentGraphicsState);
	transformsStack.push_back(currentTransform);
}
| ATTRIBUTEEND
{
	if (!graphicsStatesStack.size()) {
		LC_LOG("Unmatched AttributeEnd encountered. Ignoring it.");
	} else {
		currentGraphicsState = graphicsStatesStack.back();
		graphicsStatesStack.pop_back();
		currentTransform = transformsStack.back();
		transformsStack.pop_back();
	}
}
| CAMERA STRING paramlist
{
	const string name($2);
	if (name != "perspective")
		throw runtime_error("LuxCore supports only perspective camera");

	Properties props;
	InitProperties(props, CPS, CP);
	
	if (props.IsDefined("screenwindow")) {
		Property prop = props.Get("screenwindow");
		*sceneProps << Property("scene.camera.screenwindow")(
			prop.Get<float>(0), prop.Get<float>(1), prop.Get<float>(2), prop.Get<float>(3));
	}

	if (props.IsDefined("clippingplane")) {
		Property prop = props.Get("clippingplane");
		*sceneProps <<
				Property("scene.camera.clippingplane.enable")(1) <<
				Property("scene.camera.clippingplane.center")(prop.Get<float>(0), prop.Get<float>(1), prop.Get<float>(2)) <<
				Property("scene.camera.clippingplane.normal")(prop.Get<float>(3), prop.Get<float>(4), prop.Get<float>(5));
	}

	*sceneProps <<
			Property("scene.camera.fieldofview")(props.Get(Property("fov")(90.f)).Get<float>()) <<
			Property("scene.camera.lensradius")(props.Get(Property("lensradius")(0.f)).Get<float>()) <<
			Property("scene.camera.focaldistance")(props.Get(Property("focaldistance")(1e30f)).Get<float>()) <<
			Property("scene.camera.hither")(props.Get(Property("cliphither")(1e-3f)).Get<float>()) <<
			Property("scene.camera.yon")(props.Get(Property("clipyon")(1e30f)).Get<float>());

	worldToCamera = currentTransform;
	namedCoordinateSystems["camera"] = currentTransform;

	FreeArgs();
}
| CONCATTRANSFORM num_array
{
	if (VerifyArrayLength($2, 16, "ConcatTransform")) {
		const float *tr = static_cast<float *>($2->array);
		Transform t(Matrix4x4(tr[0], tr[4], tr[8], tr[12],
				tr[1], tr[5], tr[9], tr[13],
				tr[2], tr[6], tr[10], tr[14],
				tr[3], tr[7], tr[11], tr[15]));

		currentTransform = currentTransform * t;
	}
	ArrayFree($2);
}
| COORDINATESYSTEM STRING
{
	namedCoordinateSystems[$2] = currentTransform;
}
| COORDSYSTRANSFORM STRING
{
	const string name($2);
	if (namedCoordinateSystems.count(name))
		currentTransform = namedCoordinateSystems[name];
	else {
		throw runtime_error("Coordinate system '" + name + "' unknown");
	}
}
| EXTERIOR STRING
{
	const string name = GetLuxCoreValidName($2);
	if (!namedVolumes.count(name))
		throw runtime_error("Named volume '" + name + "' unknown");

	currentGraphicsState.exteriorVolumeName = name;
	currentGraphicsState.exteriorVolumeProps = namedVolumes[name];

	// If I'm not defining an object, set the world volume
	if (graphicsStatesStack.size() == 0) {
		*sceneProps <<
				Property("scene.camera.autovolume.enable")(false) <<
				Property("scene.camera.volume")(name);
				
	}
}
| FILM STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	// Image size
	*renderConfigProps <<
			Property("film.width")(props.Get(Property("xresolution")(800)).Get<u_int>()) <<
			Property("film.height")(props.Get(Property("yresolution")(600)).Get<u_int>());

	// Define the image pipeline (classic LuxRender has a fixed image pipeline)
	u_int pluginNumber = 0;

	// Tone mapping
	string pluginPrefix = "film.imagepipeline." + ToString(pluginNumber++);
	const string toneMapType = props.Get(Property("tonemapkernel")("autolinear")).Get<string>();
	if (toneMapType == "autolinear") {
		*renderConfigProps <<
			Property(pluginPrefix + ".type")("TONEMAP_AUTOLINEAR");
	} else if (toneMapType == "linear") {
		*renderConfigProps <<
			Property(pluginPrefix + ".type")("TONEMAP_LUXLINEAR") <<
			Property(pluginPrefix + ".sensitivity")(props.Get(Property("linear_sensitivity")(100.f)).Get<float>()) <<
			Property(pluginPrefix + ".exposure")(props.Get(Property("linear_exposure")(1.f / 1000.f)).Get<float>()) <<
			Property(pluginPrefix + ".fstop")(props.Get(Property("linear_fstop")(2.8f)).Get<float>());
	} else {
		LC_LOG("LuxCore supports only linear tone mapping, ignoring tone mapping settings");
	}

	// Camera response
	const string cameraResponse = props.Get(Property("cameraresponse")("")).Get<string>();
	if (cameraResponse != "") {
		pluginPrefix = "film.imagepipeline." + ToString(pluginNumber++);
		*renderConfigProps <<
			Property(pluginPrefix + ".type")("CAMERA_RESPONSE_FUNC") <<
			Property(pluginPrefix + ".name")(cameraResponse);
	}

	// Gamma correction
	pluginPrefix = "film.imagepipeline." + ToString(pluginNumber++);
	*renderConfigProps <<
		Property(pluginPrefix + ".type")("GAMMA_CORRECTION") <<
		Property(pluginPrefix + ".value")(props.Get(Property("gamma")(2.2f)).Get<float>());

	// Halt conditions
	*renderConfigProps <<
			Property("batch.halttime")(props.Get(Property("halttime")(0)).Get<u_int>()) <<
			Property("batch.haltspp")(props.Get(Property("haltspp")(0)).Get<u_int>());
	
	FreeArgs();
}
| IDENTITY
{
	currentTransform = Transform();
}
| INTERIOR STRING
{
	const string name = GetLuxCoreValidName($2);
	if (!namedVolumes.count(name))
		throw runtime_error("Named volume '" + name + "' unknown");

	currentGraphicsState.interiorVolumeName = name;
	currentGraphicsState.interiorVolumeProps = namedVolumes[name];
}
| LIGHTGROUP STRING paramlist
{
	const string name($2);
	if (namedLightGroups.count(name))
		currentGraphicsState.currentLightGroup = namedLightGroups[name];
	else {
		// It is a new light group
		currentGraphicsState.currentLightGroup = freeLightGroupIndex;
		namedLightGroups[name] = freeLightGroupIndex++;
	}

	FreeArgs();
}
| LIGHTSOURCE STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	// Define light name
	string lightName;
	if (props.IsDefined("name"))
		lightName = props.Get("name").Get<string>();
	else
		lightName = "LUXCORE_LIGHT";
	lightName += "_" + ToString(freeLightID++);
	const string prefix = "scene.lights." + lightName;

	const string name($2);
	if (name == "sun") {
		*sceneProps <<
				Property(prefix + ".type")("sun") <<
				Property(prefix + ".dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + ".turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + ".relsize")(props.Get(Property("relsize")(1.f)).Get<float>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	} else if (name == "sky") {
		*sceneProps <<
				Property(prefix + ".type")("sky") <<
				Property(prefix + ".dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + ".turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	} else if (name == "sky2") {
		*sceneProps <<
				Property(prefix + ".type")("sky2") <<
				Property(prefix + ".dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + ".turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	} else if (name == "sunsky") {
		*sceneProps <<
				Property(prefix + "_SUN.type")("sun") <<
				Property(prefix + "_SUN.dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + "_SUN.turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + "_SUN.relsize")(props.Get(Property("relsize")(1.f)).Get<float>()) <<
				Property(prefix + "_SUN.gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + "_SUN.transformation")(currentTransform.m) <<
				Property(prefix + "_SUN.id")(currentGraphicsState.currentLightGroup);

		*sceneProps <<
				Property(prefix + "_SKY.type")("sky") <<
				Property(prefix + "_SKY.dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + "_SKY.turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + "_SKY.gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + "_SKY.transformation")(currentTransform.m) <<
				Property(prefix + "_SKY.id")(currentGraphicsState.currentLightGroup);
	} else if (name == "sunsky2") {
		*sceneProps <<
				Property(prefix + "_SUN2.type")("sun") <<
				Property(prefix + "_SUN2.dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + "_SUN2.turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + "_SUN2.relsize")(props.Get(Property("relsize")(1.f)).Get<float>()) <<
				Property(prefix + "_SUN2.gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + "_SUN2.transformation")(currentTransform.m) <<
				Property(prefix + "_SUN2.id")(currentGraphicsState.currentLightGroup);

		*sceneProps <<
				Property(prefix + "_SKY2.type")("sky2") <<
				Property(prefix + "_SKY2.dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , 1.f))).Get<Vector>()) <<
				Property(prefix + "_SKY2.turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property(prefix + "_SKY2.gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + "_SKY2.transformation")(currentTransform.m) <<
				Property(prefix + "_SKY2.id")(currentGraphicsState.currentLightGroup);
	} else if ((name == "infinite") || (name == "infinitesample")) {
		// Check if i have to use infiniete or constantinfinite
		if (props.Get(Property("mapname")("")).Get<string>() == "") {
			*sceneProps <<
					Property(prefix + ".type")("constantinfinite") <<
					Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
					Property(prefix + ".color")(props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
					Property(prefix + ".id")(currentGraphicsState.currentLightGroup);			
		} else {
			const float gain = props.Get(Property("gain")(1.f)).Get<float>();
			const Spectrum L = props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>();
			const Spectrum combinedGain = gain * L;
			*sceneProps <<
					Property(prefix + ".type")("infinite") <<
					Property(prefix + ".file")(props.Get(Property("mapname")("")).Get<string>()) <<
					Property(prefix + ".gamma")(props.Get(Property("gamma")(2.2f)).Get<float>()) <<
					Property(prefix + ".gain")(combinedGain) <<
					Property(prefix + ".transformation")(currentTransform.m) <<
					Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
		}
	} else if (name == "point") {
		// Check if it is a point or mappoint light source
		if (props.IsDefined("mapname") || props.IsDefined("iesname")) {
			*sceneProps << Property(prefix + ".type")("mappoint");
			if (props.IsDefined("mapname"))
				*sceneProps << Property(prefix + ".mapfile")(props.Get(Property("mapname")("")).Get<string>());
			if (props.IsDefined("iesname")) {
				*sceneProps <<
						Property(prefix + ".iesfile")(props.Get(Property("iesname")("")).Get<string>()) <<
						Property(prefix + ".flipz")(props.Get(Property("flipz")(false)).Get<bool>());
			}
		} else
			*sceneProps << Property(prefix + ".type")("point");
		
		const Point from = props.Get(Property("from")(Point(0.f, 0.f, 0.f))).Get<Point>();

		*sceneProps <<
				Property(prefix + ".position")(Point(0.f, 0.f, 0.f)) <<
				Property(prefix + ".color")(props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".power")(props.Get(Property("power")(0.f)).Get<float>()) <<
				Property(prefix + ".efficency")(props.Get(Property("efficency")(0.f)).Get<float>()) <<
				Property(prefix + ".transformation")((Translate(Vector(from.x, from.y, from.z)) * currentTransform).m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	} else if (name == "spot") {
		*sceneProps <<
				Property(prefix + ".type")("spot") <<
				Property(prefix + ".coneangle")(props.Get(Property("coneangle")(30.f)).Get<float>()) <<
				Property(prefix + ".conedeltaangle")(props.Get(Property("conedeltaangle")(5.f)).Get<float>()) <<
				Property(prefix + ".position")(props.Get(Property("from")(Point(0.f, 0.f, 0.f))).Get<Point>()) <<
				Property(prefix + ".target")(props.Get(Property("to")(Point(0.f, 0.f, 1.f))).Get<Point>()) <<
				Property(prefix + ".color")(props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".power")(props.Get(Property("power")(0.f)).Get<float>()) <<
				Property(prefix + ".efficency")(props.Get(Property("efficency")(0.f)).Get<float>()) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	} else if (name == "projection") {
		*sceneProps <<
				Property(prefix + ".type")("projection") <<
				Property(prefix + ".mapfile")(props.Get(Property("mapname")("")).Get<string>()) <<
				Property(prefix + ".fov")(props.Get(Property("fov")(45.f)).Get<float>()) <<
				Property(prefix + ".position")(props.Get(Property("from")(Point(0.f, 0.f, 0.f))).Get<Point>()) <<
				Property(prefix + ".target")(props.Get(Property("to")(Point(0.f, 0.f, 1.f))).Get<Point>()) <<
				Property(prefix + ".color")(props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	} else if (name == "distant") {
		const float theta = props.Get(Property("theta")(0.f)).Get<float>();
		if (theta == 0.f) {
			*sceneProps <<
					Property(prefix + ".type")("sharpdistant");
		} else {
			*sceneProps <<
					Property(prefix + ".type")("distant") <<
					Property(prefix + ".theta")(theta) ;
		}

		const Point from = props.Get(Property("from")(Point(0.f, 0.f, 0.f))).Get<Point>();
		const Point to = props.Get(Property("to")(Point(0.f, 0.f, 1.f))).Get<Point>();
		
		*sceneProps <<
				Property(prefix + ".direction")(Normalize(to - from)) <<
				Property(prefix + ".color")(props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
				Property(prefix + ".id")(currentGraphicsState.currentLightGroup);
	}

	FreeArgs();
}
| LOOKAT NUM NUM NUM NUM NUM NUM NUM NUM NUM
{
	*sceneProps <<
			Property("scene.camera.lookat.orig")($2, $3, $4) <<
			Property("scene.camera.lookat.target")($5, $6, $7) <<
			Property("scene.camera.up")($8, $9, $10);
}
| MATERIAL STRING paramlist
{
	currentGraphicsState.materialName = "";
	InitProperties(currentGraphicsState.materialProps, CPS, CP);

	FreeArgs();
}
| MAKENAMEDMATERIAL STRING paramlist
{
	const string name = GetLuxCoreValidName($2);
	if (namedMaterials.count(name))
		throw runtime_error("Named material '" + name + "' already defined");

	Properties props;
	InitProperties(props, CPS, CP);
	namedMaterials[name] = props;
	DefineMaterial(name, props, Properties(), "", "");

	FreeArgs();
}
| MAKENAMEDVOLUME STRING STRING paramlist
{
	const string name = GetLuxCoreValidName($2);
	if (namedVolumes.count(name))
		throw runtime_error("Named volume '" + name + "' already defined");

	Properties props;
	InitProperties(props, CPS, CP);
	// The volume type
	const string type = $3;
	props << Property("type")(type);

	namedVolumes[name] = props;
	DefineVolume(name, props);

	FreeArgs();
}
| MOTIONBEGIN num_array
{
	ArrayFree($2);
}
| MOTIONEND
{
}
| NAMEDMATERIAL STRING
{
	const string name = GetLuxCoreValidName($2);
	if (!namedMaterials.count(name))
		throw runtime_error("Named material '" + name + "' unknown");

	currentGraphicsState.materialName = name;
	currentGraphicsState.materialProps = namedMaterials[name];
}
| OBJECTBEGIN STRING
{
	const string objName($2);

	currentObjectName = GetLuxCoreValidName(objName);
}
| OBJECTEND
{
	currentObjectName = "";
}
| OBJECTINSTANCE STRING
{
	const string namedObjNameStr($2);
	const string namedObjName = GetLuxCoreValidName(namedObjNameStr);

	const vector<string> &shapes = namedObjectShapes[namedObjName];
	const vector<string> &materials = namedObjectMaterials[namedObjName];
	const vector<Transform> &transforms = namedObjectTransforms[namedObjName];

	if (currentObjectName == "") {
		// I'm not inside a ObjectBegin/ObjectEnd

		const string objName = namedObjName + "_" + ToString(freeObjectID++);

		for (u_int i = 0; i < shapes.size(); ++i) {
			const string prefix = "scene.objects." + objName + "_" + ToString(i);

			*sceneProps <<
					Property(prefix + ".shape")(shapes[i]) <<
					Property(prefix + ".material")(materials[i]) <<
					Property(prefix + ".transformation")((currentTransform * transforms[i]).m);
		}
	} else {
		// I'm inside a ObjectBegin/ObjectEnd
		
		// I have to add all the shapes/materials/transforms of the referenced
		// named object to the current named object
		for (u_int i = 0; i < shapes.size(); ++i) {
			namedObjectShapes[currentObjectName].push_back(shapes[i]);
			namedObjectMaterials[currentObjectName].push_back(materials[i]);
			namedObjectTransforms[currentObjectName].push_back(currentTransform * transforms[i]);
		}
	}
}
| PORTALINSTANCE STRING
{
}
| MOTIONINSTANCE STRING NUM NUM STRING
{
}
| PIXELFILTER STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	const string name($2);
	if (name == "box") {
		*renderConfigProps <<
				Property("film.filter.type")("BOX") <<
				Property("film.filter.xwidth")(props.Get(Property("xwidth", .5f)).Get<float>() * .5f) <<
				Property("film.filter.ywidth")(props.Get(Property("ywidth", .5f)).Get<float>() * .5f);
	} else if (name == "gaussian") {
		*renderConfigProps <<
				Property("film.filter.type")("GAUSSIAN") <<
				Property("film.filter.xwidth")(props.Get(Property("xwidth", 2.f)).Get<float>() * .5f) <<
				Property("film.filter.ywidth")(props.Get(Property("ywidth", 2.f)).Get<float>() * .5f) <<
				Property("film.filter.gaussian.alpha")(props.Get(Property("alpha", 2.f)).Get<float>());
	} else if (name == "mitchell") {
		const bool supersample = props.Get(Property("supersample")(false)).Get<bool>();
		const string prefix = supersample ? "mitchell": "mitchellss";

		*renderConfigProps <<
				Property("film.filter.type")(supersample ? "MITCHELL_SS" : "MITCHELL") <<
				Property("film.filter.xwidth")(props.Get(Property("xwidth", 2.f)).Get<float>() * .5f) <<
				Property("film.filter.ywidth")(props.Get(Property("ywidth", 2.f)).Get<float>() * .5f) <<
				Property("film.filter." + prefix + ".B")(props.Get(Property("B", 1.f / 3.f)).Get<float>()) <<
				Property("film.filter." + prefix + ".C")(props.Get(Property("C", 1.f / 3.f)).Get<float>());
	} else if (name == "blackmanharris") {
		*renderConfigProps <<
				Property("film.filter.type")("BLACKMANHARRIS") <<
				Property("film.filter.xwidth")(props.Get(Property("xwidth", 4.f)).Get<float>() * .5f) <<
				Property("film.filter.ywidth")(props.Get(Property("ywidth", 4.f)).Get<float>() * .5f);
	} else {
		LC_LOG("LuxCore doesn't support the filter type " + name + ", using BLACKMANHARRIS filter instead");
		*renderConfigProps <<
				Property("film.filter.type")("BLACKMANHARRIS");
	}

	FreeArgs();
}
| RENDERER STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	// Check the name of the renderer
	if ((strcmp($2, "slg") == 0) || (strcmp($2, "slg") == 0)) {
		// It is LuxCoreRenderer

		// Look for the config parameter
		if (props.IsDefined("config")) {
			const Property &prop = props.Get("config");
			for (u_int i = 0; i < prop.GetSize(); ++i)
				luxcore::parselxs::overwriteProps.SetFromString(prop.Get<string>(i));
		}

		// Look for the localconfigfile parameter
		if (props.IsDefined("localconfigfile")) {
			luxcore::parselxs::overwriteProps.SetFromFile(props.Get("localconfigfile").Get<string>());
		}
	}

	FreeArgs();
}
| REVERSEORIENTATION
{
}
| ROTATE NUM NUM NUM NUM
{
	Transform t(Rotate($2, Vector($3, $4, $5)));
	currentTransform = currentTransform * t;
}
| SAMPLER STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	const string name($2);
	if (name == "metropolis") {
		*renderConfigProps <<
				Property("sampler.type")("METROPOLIS") <<
				Property("sampler.metropolis.maxconsecutivereject")(Property("maxconsecrejects")(512).Get<u_int>()) <<
				Property("sampler.metropolis.largesteprate")(Property("largemutationprob")(.4f).Get<float>()) <<
				Property("sampler.metropolis.imagemutationrate")(Property("mutationrange")(.1f).Get<float>());
	} else if ((name == "sobol") || (name == "lowdiscrepancy")) {
		*renderConfigProps <<
				Property("sampler.type")("SOBOL");
	} else if (name == "random") {
		*renderConfigProps <<
				Property("sampler.type")("RANDOM");
	} else {
		LC_LOG("LuxCore doesn't support the sampler type " + name + ", falling back to random sampler");
		*renderConfigProps <<
				Property("sampler.type")("RANDOM");
	}

	FreeArgs();
}
| SCALE NUM NUM NUM
{
	Transform t(Scale($2, $3, $4));
	currentTransform = currentTransform * t;
}
| SEARCHPATH STRING
{
}
| SHAPE STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	// Define object name
	string objName, prefix;
	if (currentObjectName == "") {
		if (props.IsDefined("name")) {
			// LuxRender object names are not unique
			objName = props.Get("name").Get<string>();
		} else
			objName = "LUXCORE_OBJECT";
		objName += "_" + ToString(freeObjectID++);
		objName = GetLuxCoreValidName(objName);
		prefix = "scene.objects." + objName;
	} else {
		objName = currentObjectName + "_" + ToString(freeObjectID++);
		prefix = "scene.shapes." + objName;
		
		namedObjectShapes[currentObjectName].push_back(objName);
		namedObjectTransforms[currentObjectName].push_back(Transform(Matrix4x4::MAT_IDENTITY));
	}

	// Define object material
	if ((currentGraphicsState.materialName == "") ||
		(currentGraphicsState.interiorVolumeName != "") ||
		(currentGraphicsState.exteriorVolumeName != "")) {
		const string materialName = "LUXCORE_MATERIAL_" + objName;

		// Define the used material on-the-fly
		DefineMaterial(materialName, currentGraphicsState.materialProps,
				currentGraphicsState.areaLightProps,
				currentGraphicsState.interiorVolumeName,
				currentGraphicsState.exteriorVolumeName);
		
		// Material assignment is not required for shapes		
		if (currentObjectName == "") {
			*sceneProps <<
				Property(prefix + ".material")(materialName);
		} else
			namedObjectMaterials[currentObjectName].push_back(materialName);
	} else {
		// It is a named material
		if ((currentGraphicsState.areaLightName == "") &&
				(currentGraphicsState.interiorVolumeName == "") &&
				(currentGraphicsState.exteriorVolumeName == "")) {
			// I can use the already defined material without emission and
			// without volumes

			// Material assignment is not required for shapes		
			if (currentObjectName == "") {
				*sceneProps <<
					Property(prefix + ".material")(currentGraphicsState.materialName);
			} else
				namedObjectMaterials[currentObjectName].push_back(currentGraphicsState.materialName);
		} else {
			const string materialName = "LUXCORE_MATERIAL_" + objName;

			// I have to define a new material with emission
			DefineMaterial(materialName, currentGraphicsState.materialProps,
				currentGraphicsState.areaLightProps,
				currentGraphicsState.interiorVolumeName,
				currentGraphicsState.exteriorVolumeName);

			// Material assignment is not required for shapes		
			if (currentObjectName == "") {
				*sceneProps <<
					Property(prefix + ".material")(materialName);
			} else
				namedObjectMaterials[currentObjectName].push_back(materialName);
		}
	}

	const string name($2);
	if (name == "plymesh") {
		*sceneProps <<
			Property(prefix + ".ply")(props.Get(Property("filename")("none")).Get<string>()) <<
			Property(prefix + ".transformation")(currentTransform.m);
	} else 	if ((name == "trianglemesh") || (name == "mesh")) {
		if (!props.IsDefined("P"))
			throw runtime_error("Missing P parameter in trianglemesh/mesh: " + objName);
		Property pointsProp = props.Get("P");
		if ((pointsProp.GetSize() == 0) || (pointsProp.GetSize() % 3 != 0))
			throw runtime_error("Wrong trianglemesh/mesh point list length: " + objName);
		// Copy all vertices
		Property points = pointsProp.Renamed(prefix + ".vertices");

		const string indicesName = (name == "trianglemesh") ? "indices" : "triindices";
		if (!props.IsDefined(indicesName))
			throw runtime_error("Missing indices parameter in trianglemesh/mesh: " + objName);
		Property indicesProp = props.Get(indicesName);
		if ((indicesProp.GetSize() == 0) || (indicesProp.GetSize() % 3 != 0))
			throw runtime_error("Wrong trianglemesh/mesh indices list length: " + objName);
		// Copy all indices
		Property faces = indicesProp.Renamed(prefix + ".faces");

		*sceneProps <<
			points <<
			faces <<
			Property(prefix + ".transformation")(currentTransform.m);

		if (props.IsDefined("N")) {
			Property normalsProp = props.Get("N");
			if ((normalsProp.GetSize() == 0) ||	(normalsProp.GetSize() != pointsProp.GetSize()))
				throw runtime_error("Wrong trianglemesh/mesh normal list length: " + objName);
			// Copy all normals
			*sceneProps <<
					normalsProp.Renamed(prefix + ".normals");
		}

		if (props.IsDefined("uv")) {
			Property uvsProps = props.Get("uv");
			if ((uvsProps.GetSize() == 0) || (uvsProps.GetSize() % 2 != 0) ||
					(uvsProps.GetSize() / 2 != pointsProp.GetSize() / 3))
				throw runtime_error("Wrong trianglemesh/mesh uv list length: " + objName);
			// Copy all uvs
			*sceneProps <<
					uvsProps.Renamed(prefix + ".uvs");
		}
	} else {
		LC_LOG("LuxCore doesn't support the shape type " + name + ", ignoring the shape definition");
	}

	FreeArgs();
}
| PORTALSHAPE STRING paramlist
{
	FreeArgs();
}
| SURFACEINTEGRATOR STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	const string name($2);
	if (name == "path") {
		// Path tracing
		*renderConfigProps <<
			Property("renderengine.type")("PATHOCL") <<
			Property("path.maxdepth")(props.Get(Property("maxdepth")(16u)).Get<u_int>());
	} else if (name == "bidirectional") {
		// Bidirectional path tracing
		*renderConfigProps <<
			Property("renderengine.type")("BIDIRCPU") <<
			Property("path.maxdepth")(props.Get(Property("eyedepth")(8u)).Get<u_int>()) <<
			Property("light.maxdepth")(props.Get(Property("lightdepth")(8u)).Get<u_int>());
	} else {
		// Unmapped surface integrator, just use path tracing
		LC_LOG("LuxCore doesn't support the SurfaceIntegrator, falling back to path tracing");
		*renderConfigProps <<
			Property("renderengine.type")("PATHOCL");
	}

	FreeArgs();
}
| TEXTURE STRING STRING STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	string name = GetLuxCoreValidName($2);
	if (namedTextures.count(name))
		LC_LOG("Texture '" << name << "' being redefined.");
	namedTextures.insert(name);

	const string texType($4);
	const string prefix = "scene.textures." + name;

	if (texType == "constant") {
		const string channels($3);

		if (channels == "float") {
			*sceneProps <<
				Property(prefix + ".type")("constfloat1") <<
				Property(prefix + ".value")(props.Get(Property("value")(1.f)).Get<float>());
		} else if ((channels == "color") || (channels == "spectrum")) {
			*sceneProps <<
				Property(prefix + ".type")("constfloat3") <<
				Property(prefix + ".value")(props.Get(Property("value")(Spectrum(1.f))).Get<Spectrum>());
		} else if (channels == "fresnel") {
			*sceneProps <<
				Property(prefix + ".type")("fresnelconst") <<
				Property(prefix + ".n")(props.Get(Property("value")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property(prefix + ".k")(Spectrum(0.f));
		} else  {
			LC_LOG("LuxCore doesn't support the constant texture type: " << channels);

			*sceneProps <<
					Property(prefix + ".type")("constfloat1") <<
					Property(prefix + ".value")(.1f);
		}
	} else if (texType == "imagemap") {
		const float gamma = props.Get(Property("gamma")(2.2f)).Get<float>();
		const float gain = props.Get(Property("gain")(1.f)).Get<float>();
		*sceneProps <<
				Property(prefix + ".type")("imagemap") <<
				Property(prefix + ".file")(props.Get(Property("filename")("")).Get<string>()) <<
				Property(prefix + ".gamma")(gamma) <<
				// LuxRender applies gain before gamma correction
				Property(prefix + ".gain")(powf(gain, gamma)) <<
				Property(prefix + ".channel")(props.Get(Property("channel")("default")).Get<string>()) <<
				GetTextureMapping2D(prefix, props);
	} else if (texType == "add") {
		*sceneProps <<
				Property(prefix + ".type")("add") <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(1.f)), props);
	} else if (texType == "subtract") {
		*sceneProps <<
				Property(prefix + ".type")("subtract") <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(1.f)), props);
	} else if (texType == "scale") {
		*sceneProps <<
				Property(prefix + ".type")("scale") <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(1.f)), props);
	} else if (texType == "mix") {
		*sceneProps <<
				Property(prefix + ".type")("mix") <<
				GetTexture(prefix + ".amount", Property("amount")(.5f), props) <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(0.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(1.f)), props);
	} else if (texType == "colordepth") {
		*sceneProps <<
				Property(prefix + ".type")("colordepth") <<
				Property(prefix + ".depth")(props.Get(Property("depth")(1.f)).Get<float>()) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(0.f)), props);
	} else if (texType == "blackbody") {
		*sceneProps <<
				Property(prefix + ".type")("blackbody") <<
				Property(prefix + ".temperature")(props.Get(Property("temperature")(6500.f)).Get<float>());
	} else if (texType == "irregulardata") {
		const Property &wl = props.Get(Property("wavelengths"));
		if (wl.GetSize() < 2)
			throw runtime_error("Insufficient wavelenghts in an irregulardata texture");

		const Property &dt = props.Get(Property("data"));
		if (dt.GetSize() < 2)
			throw runtime_error("Insufficient data in an irregulardata texture");

		if (wl.GetSize() != dt.GetSize())
			throw runtime_error("Number of wavelengths doesn't match number of data values in irregulardata texture");

		Property(prefix + ".wavelengths");

		*sceneProps <<
				Property(prefix + ".type")("irregulardata") <<
				wl.Renamed(prefix + ".wavelengths") <<
				dt.Renamed(prefix + ".data");
	} else if (texType == "lampspectrum") {
		*sceneProps <<
				Property(prefix + ".type")("lampspectrum") <<
				Property(prefix + ".name")(props.Get(Property("name")("Incandescent2")).Get<string>());
	} else if (texType == "triplanar") {
		*sceneProps <<
				Property(prefix + ".type")("triplanar") <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(1.f,0.f,0.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(0.f,1.f,0.f)), props) <<
				GetTexture(prefix + ".texture3", Property("tex3")(Spectrum(0.f,0.f,1.f)), props) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else
	//--------------------------------------------------------------------------
	// Procedural textures
	//--------------------------------------------------------------------------
	if (texType == "checkerboard") {
		const u_int dimension = props.Get(Property("dimension")(2)).Get<u_int>();

		*sceneProps <<
				Property(prefix + ".type")((dimension == 2) ? "checkerboard2d" : "checkerboard3d") <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(0.f)), props) <<
				((dimension == 2) ? GetTextureMapping2D(prefix, props) : GetTextureMapping3D(prefix, currentTransform, props));
	} else if (texType == "fbm") {
		*sceneProps <<
				Property(prefix + ".type")("fbm") <<
				GetTexture(prefix + ".octaves", Property("octaves")(8), props) <<
				GetTexture(prefix + ".roughness", Property("roughness")(.5f), props) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "marble") {
		*sceneProps <<
				Property(prefix + ".type")("marble") <<
				GetTexture(prefix + ".octaves", Property("octaves")(8), props) <<
				GetTexture(prefix + ".roughness", Property("roughness")(.5f), props) <<
				GetTexture(prefix + ".scale", Property("scale")(1.f), props) <<
				GetTexture(prefix + ".variation", Property("variation")(.2f), props) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "dots") {
		*sceneProps <<
				Property(prefix + ".type")("dots") <<
				GetTexture(prefix + ".inside", Property("inside")(1.f), props) <<
				GetTexture(prefix + ".outside", Property("outside")(0.f), props) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "brick") {
		*sceneProps <<
				Property(prefix + ".type")("brick") <<
				GetTexture(prefix + ".bricktex", Property("bricktex")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".mortartex", Property("mortartex")(Spectrum(.2f)), props) <<
				GetTexture(prefix + ".brickmodtex", Property("brickmodtex")(Spectrum(1.f)), props) <<
				Property(prefix + ".brickwidth")(props.Get(Property("brickwidth")(.3f)).Get<float>()) <<
				Property(prefix + ".brickheight")(props.Get(Property("brickheight")(.1f)).Get<float>()) <<
				Property(prefix + ".brickdepth")(props.Get(Property("brickdepth")(.15f)).Get<float>()) <<
				Property(prefix + ".mortarsize")(props.Get(Property("mortarsize")(.01f)).Get<float>()) <<
				Property(prefix + ".brickbond")(props.Get(Property("brickbond")("running")).Get<string>()) <<
				Property(prefix + ".brickrun")(props.Get(Property("brickrun")(.75f)).Get<float>()) <<
				Property(prefix + ".brickbevel")(props.Get(Property("brickbevel")(0.f)).Get<float>()) <<
				GetTexture(prefix + ".brickwidth", Property("brickwidth")(.3f), props) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "windy") {
		*sceneProps <<
				Property(prefix + ".type")("windy") <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "wrinkled") {
		*sceneProps <<
				Property(prefix + ".type")("wrinkled") <<
				GetTexture(prefix + ".octaves", Property("octaves")(8), props) <<
				GetTexture(prefix + ".roughness", Property("roughness")(.5f), props) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "uv") {
		*sceneProps <<
				Property(prefix + ".type")("uv") <<
				GetTextureMapping2D(prefix, props);
	} else if (texType == "band") {
		*sceneProps <<
				Property(prefix + ".type")("band") <<
				GetTexture(prefix + ".amount", Property("amount")(0.f), props);

		const Property offsetsProp = props.Get(Property("offsets"));
		const u_int offsetsSize = offsetsProp.GetSize();
		for (u_int i = 0; i < offsetsSize; ++i) {
			*sceneProps <<
					Property(prefix + ".offset" + ToString(i))(offsetsProp.Get<float>(i));

			const Property valueProp = props.Get(Property("value" + ToString(i)));
			if (valueProp.GetSize() == 1) {
				*sceneProps <<
					Property(prefix + ".value" + ToString(i))(valueProp.Get<float>());
			} else if (valueProp.GetSize() == 3) {
				*sceneProps <<
					Property(prefix + ".value" + ToString(i))(Spectrum(valueProp.Get<float>(0),
						valueProp.Get<float>(1), valueProp.Get<float>(2)));
			} else {
				LC_LOG("LuxCore supports only Band texture with constant values");
				
				*sceneProps <<
					Property(prefix + ".value" + ToString(i))(.7f);
			}
		}
	} else if (texType == "bilerp") {
		*sceneProps <<
				Property(prefix + ".type")("bilerp") <<
				GetTexture(prefix + ".texture00", Property("v00")(0.f), props) <<
				GetTexture(prefix + ".texture01", Property("v01")(1.f), props) <<
				GetTexture(prefix + ".texture10", Property("v10")(0.f), props) <<
				GetTexture(prefix + ".texture11", Property("v11")(1.f), props);
	}
	//--------------------------------------------------------------------------
	// Blender procedural textures
	//--------------------------------------------------------------------------
	else if (texType == "blender_blend") {
		*sceneProps <<
				Property(prefix + ".type")("blender_blend") <<
				Property(prefix + ".progressiontype")(props.Get(Property("progressiontype")("linear")).Get<string>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_clouds") {
		*sceneProps <<
				Property(prefix + ".type")("blender_clouds") <<
				Property(prefix + ".noisetype")(props.Get(Property("noisetype")("soft_noise")).Get<string>()) <<
				Property(prefix + ".noisebasis")(props.Get(Property("noisebasis")("blender_original")).Get<string>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".noisedepth")(props.Get(Property("noisedepth")(2)).Get<int>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_distortednoise") {
		*sceneProps <<
				Property(prefix + ".type")("blender_distortednoise") <<
				Property(prefix + ".noise_distortion")(props.Get(Property("noise_distortion")("blender_original")).Get<string>()) <<
				Property(prefix + ".noisebasis")(props.Get(Property("noisebasis")("blender_original")).Get<string>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".distortion")(props.Get(Property("distortion")(1.f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_magic") {
		*sceneProps <<
				Property(prefix + ".type")("blender_magic") <<
				Property(prefix + ".noisedepth")(props.Get(Property("noisedepth")(2)).Get<int>()) <<
				Property(prefix + ".turbulence")(props.Get(Property("turbulence")(5.f)).Get<float>()) <<
				Property(prefix + ".distortion")(props.Get(Property("distortion")(1.f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_marble") {
		*sceneProps <<
				Property(prefix + ".type")("blender_marble") <<
				Property(prefix + ".marbletype")(props.Get(Property("marbletype")("soft")).Get<string>()) <<
				Property(prefix + ".noisebasis")(props.Get(Property("noisebasis")("blender_original")).Get<string>()) <<
				Property(prefix + ".noisebasis2")(props.Get(Property("noisebasis2")("sin")).Get<string>()) <<
				Property(prefix + ".noisedepth")(props.Get(Property("noisedepth")(2)).Get<int>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".noisetype")(props.Get(Property("noisetype")("soft_noise")).Get<string>()) <<
				Property(prefix + ".turbulence")(props.Get(Property("turbulence")(5.f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_musgrave") {
		*sceneProps <<
				Property(prefix + ".type")("blender_musgrave") <<
				Property(prefix + ".musgravetype")(props.Get(Property("musgravetype")("multifractal")).Get<string>()) <<
				Property(prefix + ".noisebasis")(props.Get(Property("noisebasis")("blender_original")).Get<string>()) <<
				Property(prefix + ".dimension")(props.Get(Property("dimension")(1.f)).Get<float>()) <<
				Property(prefix + ".intensity")(props.Get(Property("intensity")(1.f)).Get<float>()) <<
				Property(prefix + ".lacunarity")(props.Get(Property("lacunarity")(1.f)).Get<float>()) <<
				Property(prefix + ".offset")(props.Get(Property("offset")(1.f)).Get<float>()) <<
				Property(prefix + ".gain")(props.Get(Property("gain")(1.f)).Get<float>()) <<
				Property(prefix + ".octaves")(props.Get(Property("octaves")(2.f)).Get<float>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_noise") {
		*sceneProps <<
				Property(prefix + ".type")("blender_noise") <<
				Property(prefix + ".noisedepth")(props.Get(Property("noisedepth")(2)).Get<int>());
	} else if (texType == "blender_stucci") {
		*sceneProps <<
				Property(prefix + ".type")("blender_stucci") <<
				Property(prefix + ".stuccitype")(props.Get(Property("stuccitype")("plastic")).Get<string>()) <<
				Property(prefix + ".noisebasis")(props.Get(Property("noisebasis")("blender_original")).Get<string>()) <<
				Property(prefix + ".noisetype")(props.Get(Property("noisetype")("soft_noise")).Get<string>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".turbulence")(props.Get(Property("turbulence")(5.f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_wood") {
		*sceneProps <<
				Property(prefix + ".type")("blender_wood") <<
				Property(prefix + ".woodtype")(props.Get(Property("woodtype")("bands")).Get<string>()) <<
				Property(prefix + ".noisebasis")(props.Get(Property("noisebasis")("blender_original")).Get<string>()) <<
				Property(prefix + ".noisebasis2")(props.Get(Property("noisebasis2")("sin")).Get<string>()) <<
				Property(prefix + ".noisetype")(props.Get(Property("noisetype")("soft_noise")).Get<string>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".turbulence")(props.Get(Property("turbulence")(5.f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	} else if (texType == "blender_voronoi") {
		*sceneProps <<
				Property(prefix + ".type")("blender_voronoi") <<
				Property(prefix + ".intensity")(props.Get(Property("intensity")(1.f)).Get<float>()) <<
				Property(prefix + ".exponent")(props.Get(Property("exponent")(2.f)).Get<float>()) <<
				Property(prefix + ".distmetric")(props.Get(Property("distmetric")("actual_distance")).Get<string>()) <<
				Property(prefix + ".w1")(props.Get(Property("w1")(1.f)).Get<float>()) <<
				Property(prefix + ".w2")(props.Get(Property("w2")(0.f)).Get<float>()) <<
				Property(prefix + ".w3")(props.Get(Property("w3")(0.f)).Get<float>()) <<
				Property(prefix + ".w4")(props.Get(Property("w4")(0.f)).Get<float>()) <<
				Property(prefix + ".noisesize")(props.Get(Property("noisesize")(.25f)).Get<float>()) <<
				Property(prefix + ".bright")(props.Get(Property("bright")(1.f)).Get<float>()) <<
				Property(prefix + ".contrast")(props.Get(Property("contrast")(1.f)).Get<float>()) <<
				GetTextureMapping3D(prefix, currentTransform, props);
	}
	//--------------------------------------------------------------------------
	else if (texType == "hitpointcolor") {
		*sceneProps <<
				Property(prefix + ".type")("hitpointcolor");
	} else if (texType == "hitpointalpha") {
		*sceneProps <<
				Property(prefix + ".type")("hitpointalpha");
	} else if (texType == "hitpointgrey") {
		const int channel = props.Get("channel").Get<int>();
		*sceneProps <<
				Property(prefix + ".type")("hitpointgrey") <<
				Property(prefix + ".channel")(((channel != 0) && (channel != 1) && (channel != 2)) ?
						-1 : channel);
	} else if (texType == "fresnelcolor") {
		Property krProp = props.Get(Property("Kr")(Spectrum(.5f)));
		if (krProp.GetSize() == 1) {
			// Kr is a texture
			//
			// Note: only constant textures are supported
			*sceneProps <<
					Property(prefix + ".type")("fresnelcolor") <<
					Property(prefix + ".kr")(GetLuxCoreValidName(props.Get(Property("Kr")("")).Get<string>()));
		} else if (krProp.GetSize() == 3) {
			// Kr is a Spectrum
			*sceneProps <<
					Property(prefix + ".type")("fresnelcolor") <<
					Property(prefix + ".kr")(props.Get(Property("Kr")(Spectrum(.5f))).Get<Spectrum>());
		} else
			throw runtime_error("Unknown Kr type of fresnelcolor: " + krProp.ToString());
	} else if (texType == "preset") {
		*sceneProps <<
				Property(prefix + ".type")("fresnelpreset") <<
				Property(prefix + ".name")(props.Get(Property("name")("aluminium")).Get<string>());
	} else if (texType == "sopra") {
		*sceneProps <<
				Property(prefix + ".type")("fresnelsopra") <<
				Property(prefix + ".file")(props.Get(Property("filename")("")).Get<string>());
	} else if (texType == "luxpop") {
		*sceneProps <<
				Property(prefix + ".type")("fresnelluxpop") <<
				Property(prefix + ".file")(props.Get(Property("filename")("")).Get<string>());
	} else if (texType == "cauchy") {
		const float index = props.Get(Property("index")(-1.f)).Get<float>();
		const float b = props.Get(Property("cauchyb")(0.f)).Get<float>();
		*sceneProps <<
				Property(prefix + ".type")("fresnelcauchy") <<
				Property(prefix + ".a")(props.Get(Property("cauchya")(index > 0.f ? (index - b * 1e6f / (720.f * 380.f)) : 1.5f)).Get<float>()) <<
				Property(prefix + ".b")(b);
	} else if (texType == "abbe") {
		const string mode = props.Get(Property("type")("d")).Get<string>();
		*sceneProps <<
				Property(prefix + ".type")("fresnelabbe") <<
				Property(prefix + ".mode")(props.Get(Property("type")("d")).Get<string>()) <<
				Property(prefix + ".v")(props.Get(Property("V")(64.17f)).Get<float>()) <<
				Property(prefix + ".n")(props.Get(Property("n")(1.5168f)).Get<float>());
		if (mode == "custom") {
			*sceneProps <<
				Property(prefix + ".d")(props.Get(Property("lambda_D")(587.5618f)).Get<float>()) <<
				Property(prefix + ".f")(props.Get(Property("lambda_F")(486.13f)).Get<float>()) <<
				Property(prefix + ".c")(props.Get(Property("lambda_C")(656.28f)).Get<float>());
		}
	} else {
		LC_LOG("LuxCore doesn't support the texture type: " << texType);

		*sceneProps <<
				Property(prefix + ".type")("constfloat1") <<
				Property(prefix + ".value")(.7f);
	}

	FreeArgs();
}
| TRANSFORMBEGIN
{
	transformsStack.push_back(currentTransform);
}
| TRANSFORMEND
{
	if (!(transformsStack.size() > graphicsStatesStack.size())) {
		LC_LOG("Unmatched TransformEnd encountered. Ignoring it.");
	} else {
		currentTransform = transformsStack.back();
		transformsStack.pop_back();
	}
}
| TRANSFORM real_num_array
{
	if (VerifyArrayLength($2, 16, "Transform")) {
		const float *tr = static_cast<float *>($2->array);
		Transform t(Matrix4x4(tr[0], tr[4], tr[8], tr[12],
				tr[1], tr[5], tr[9], tr[13],
				tr[2], tr[6], tr[10], tr[14],
				tr[3], tr[7], tr[11], tr[15]));

		currentTransform = t;
	}

	ArrayFree($2);
}
| TRANSLATE NUM NUM NUM
{
	Transform t(Translate(Vector($2, $3, $4)));
	currentTransform = currentTransform * t;
}
| VOLUMEINTEGRATOR STRING paramlist
{
	FreeArgs();
}
| VOLUME STRING paramlist
{
	FreeArgs();
}
| WORLDBEGIN
{
}
| WORLDEND
{
};
%%

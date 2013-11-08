
/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

%{

#include <stdarg.h>
#include <sstream>
#include <vector>

#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

namespace luxcore { namespace parselxs {
Properties *renderConfigProps = NULL;
Properties *sceneProps = NULL;

Properties overwriteProps;
Transform worldToCamera;

//------------------------------------------------------------------------------
// RenderOptions
//------------------------------------------------------------------------------

class GraphicsState {
public:
	GraphicsState() {
		currentLightGroup = 0;
	}
	~GraphicsState() {
	}

	string areaLightName, materialName;
	Properties areaLightProps, materialProps;

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
static boost::unordered_set<string> namedMaterials;

void ResetParser() {
	overwriteProps.Clear();

	*renderConfigProps <<
			Property("opencl.platform.index")(-1) <<
			Property("opencl.cpu.use")(true) <<
			Property("opencl.gpu.use")(true) <<
			Property("renderengine.type")("PATHOCL") <<
			Property("sampler.type")("RANDOM") <<
			Property("film.filter.type")("MITCHELL");

	graphicsStatesStack.clear();
	currentGraphicsState = GraphicsState();
	transformsStack.clear();
	currentTransform = Transform();

	namedCoordinateSystems.clear();
	freeLightGroupIndex = 1;
	namedLightGroups.clear();
	namedMaterials.clear();
	namedMaterials.insert("LUXCORE_PARSERLXS_DEFAULT_MATERIAL");

	// Define the default material
	*sceneProps <<
			Property("scene.materials.LUXCORE_PARSERLXS_DEFAULT_MATERIAL.type")("matte") <<
			Property("scene.materials.LUXCORE_PARSERLXS_DEFAULT_MATERIAL.kd")(Spectrum(.9f));
}

static Property GetTexture(const string &luxCoreName, const Property defaultProp, const Properties &props) {
	Property prop = props.Get(defaultProp);
	if (prop.GetValueType(0) == typeid(string)) {
		// It is a texture name
		return Property(luxCoreName)(prop.Get<string>());
	} else
		return prop.Renamed(luxCoreName);
}

static void DefineMaterial(const string &name, const Properties &props) {
	const string prefix = "scene.materials." + name;

	const string type = props.Get(Property("type")("matte")).Get<string>();
	if (type == "matte") {
		*sceneProps <<
				Property(prefix + ".type")("matte") <<
				GetTexture(prefix + ".kd", Property("Kd")(Spectrum(.9f)), props);
	} else if (type == "mirror") {
		*sceneProps <<
				Property(prefix + ".type")("mirror") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), props);
	} else if (type == "glass") {
		const bool isArchitectural = props.Get(Property("architectural")(false)).Get<bool>();

		*sceneProps <<
				Property(prefix + ".type")(isArchitectural ? "archglass" : "glass") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), props) <<
				Property(prefix +".ioroutside")(1.f) <<
				GetTexture(prefix + ".iorinside", Property("index")(1.5f), props);
	} else if (type == "metal") {
		Spectrum n, k;
		string presetName = props.Get("name").Get<string>();
		if (name == "amorphous carbon") {
			n = Spectrum(2.94553471f, 2.22816062f, 1.98665321f);
			k = Spectrum(0.876640677f, 0.799504995f, 0.821194172f);
		} else if (name == "silver") {
			n = Spectrum(0.155706137f, 0.115924977f, 0.138897374f);
			k = Spectrum(4.88647795f, 3.12787175f, 2.17797375f);
		} else if (name == "gold") {
			n = Spectrum(0.117958963f, 0.354153246f, 1.4389739f);
			k = Spectrum(4.03164577f, 2.39416027f, 1.61966884f);
		} else if (name == "copper") {
			n = Spectrum(0.134794354f, 0.928983212f, 1.10887861f);
			k = Spectrum(3.98125982f, 2.440979f, 2.16473627f);
		} else if (name == "aluminium") {
			n = Spectrum(1.69700277f, 0.879832864f, 0.5301736f);
			k = Spectrum(9.30200672f, 6.27604008f, 4.89433956f);
		} else {
			LC_LOG("Unknown metal type '" << presetName << "'. Using default (aluminium).");

			n = Spectrum(1.69700277f, 0.879832864f, 0.5301736f);
			k = Spectrum(9.30200672f, 6.27604008f, 4.89433956f);
		}			

		*sceneProps <<
				Property(prefix + ".type")("metal2") <<
				Property(prefix + ".n")(n) <<
				Property(prefix + ".k")(k) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), props) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), props);
	} else if (type == "mattetranslucent") {
		*sceneProps <<
				Property(prefix + ".type")("mattetranslucent") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), props);
	} else if (type == "null") {
		*sceneProps <<
				Property(prefix + ".type")("null");
	} else if (type == "mix") {
		*sceneProps <<
				Property(prefix + ".type")("mix") <<
				GetTexture(prefix + ".amount", Property("amount")(.5f), props) <<
				Property(prefix + ".material1")(props.Get(Property("namedmaterial1")("")).Get<string>()) <<
				Property(prefix + ".material2")(props.Get(Property("namedmaterial2")("")).Get<string>());
	} else if (type == "glossy") {
		*sceneProps <<
				Property(prefix + ".type")("glossy2") <<
				GetTexture(prefix + ".kd", Property("Kd")(.5f), props) <<
				GetTexture(prefix + ".ks", Property("Ks")(.5f), props) <<
				GetTexture(prefix + ".ka", Property("Ks")(0.f), props) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), props) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), props) <<
				GetTexture(prefix + ".d", Property("d")(0.f), props) <<
				GetTexture(prefix + ".index", Property("index")(0.f), props) <<
				Property(prefix +".multibounce")(props.Get(Property("multibounce")(false)).Get<bool>());
	} else if (type == "metal2") {
		const string colTexName = props.Get(Property("fresnel")(5.f)).Get<string>();
		*sceneProps <<
					Property("scene.textures.LUXCORE_PARSERLXS_fresnelapproxn-" + name + ".type")("fresnelapproxn") <<
					Property("scene.textures.LUXCORE_PARSERLXS_fresnelapproxn-" + name + ".texture")(colTexName) <<
					Property("scene.textures.LUXCORE_PARSERLXS_fresnelapproxk-" + name + ".type")("fresnelapproxk") <<
					Property("scene.textures.LUXCORE_PARSERLXS_fresnelapproxk-" + name + ".texture")(colTexName);

		*sceneProps <<
				Property(prefix + ".type")("metal2") <<
				Property(prefix + ".n")("LUXCORE_PARSERLXS_fresnelapproxn-" + name) <<
				Property(prefix + ".k")("LUXCORE_PARSERLXS_fresnelapproxk-" + name) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), props) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), props);
	} else if (type == "roughglass") {
		*sceneProps <<
				Property(prefix + ".type")("roughglass") <<
				GetTexture(prefix + ".kr", Property("Kr")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".kt", Property("Kt")(Spectrum(1.f)), props) <<
				Property(prefix +".ioroutside")(1.f) <<
				GetTexture(prefix + ".iorinside", Property("index")(1.5f), props) <<
				GetTexture(prefix + ".uroughness", Property("uroughness")(.1f), props) <<
				GetTexture(prefix + ".vroughness", Property("vroughness")(.1f), props);
	} else {
		LC_LOG("LuxCor::ParserLXS supports only Matte, Mirror, Glass, Metal, MatteTranslucent, Null, Mix, Glossy2, Metal2 and RoughGlass material (i.e. not " <<
			type << "). Replacing an unsupported material with matte.");

		*sceneProps <<
				Property(prefix + ".type")("matte") <<
				Property(prefix + ".kd")(Spectrum(.9f));
	}

	if (props.IsDefined("bumpmap")) {
		// TODO: check the kind of texture to understand if it is a bump map or a normal map
		*sceneProps << Property(prefix + ".bumptex")(props.Get("bumpmap").Get<string>());
		//*sceneProps << Property(prefix + ".normaltex")(props.Get("bumpmap").Get<string>());
	}
	
	namedMaterials.insert(name);
}

} }

using namespace luxcore::parselxs;

//------------------------------------------------------------------------------

extern int yylex(void);
u_int lineNum = 0;
string currentFile;

#define YYMAXDEPTH 100000000

void yyerror(const char *str)
{
	std::stringstream ss;
	ss << "Parsing error";
	if (currentFile != "")
		ss << " in file '" << currentFile << "'";
	if (lineNum > 0)
		ss << " at line " << lineNum;
	ss << ": " << str;
	LC_LOG(ss.str().c_str());
}

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

static void InitProperties(Properties &props, const u_int count, const ParamListElem *list);

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
		throw std::runtime_error("LuxCore supports only perspective camera");

	Properties props;
	InitProperties(props, CPS, CP);
	
	if (props.IsDefined("screenwindow")) {
		Property prop = props.Get("screenwindow");
		*sceneProps << Property("scene.camera.screenwindow")(
			prop.Get<float>(0), prop.Get<float>(1), prop.Get<float>(2), prop.Get<float>(3));
	}
	
	*sceneProps <<
			Property("scene.camera.fieldofview")(props.Get(Property("fov")(90.f)).Get<float>()) <<
			Property("scene.camera.lensradius")(props.Get(Property("lensradius")(0.00625f)).Get<float>()) <<
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
}
| FILM STRING paramlist
{
	Properties props;
	InitProperties(props, CPS, CP);

	*renderConfigProps <<
			Property("film.width")(props.Get(Property("xresoluton")(800)).Get<u_int>()) <<
			Property("film.height")(props.Get(Property("yresoluton")(600)).Get<u_int>());

	FreeArgs();
}
| IDENTITY
{
	currentTransform = Transform();
}
| INTERIOR STRING
{
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

	const string name($2);
	if ((name == "sunsky") || (name == "sunsky2")) {
		// Note: (1000000000.0f / (M_PI * 100.f * 100.f)) is in LuxCore code
		// for compatibility with past scene
		const float gainAdjustFactor = (1000000000.0f / (M_PI * 100.f * 100.f)) * INV_PI;

		*sceneProps <<
				Property("scene.sunlight.dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , -1.f))).Get<Vector>()) <<
				Property("scene.sunlight.turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property("scene.sunlight.relsize")(props.Get(Property("relsize")(1.f)).Get<float>()) <<
				Property("scene.sunlight.gain")(props.Get(Property("gain")(1.f)).Get<float>() * gainAdjustFactor) <<
				Property("scene.sunlight.transformation")(currentTransform.m) <<
				Property("scene.sunlight.id")(currentGraphicsState.currentLightGroup);

		*sceneProps <<
				Property("scene.skylight.dir")(props.Get(Property("sundir")(Vector(0.f, 0.f , -1.f))).Get<Vector>()) <<
				Property("scene.skylight.turbidity")(props.Get(Property("turbidity")(2.f)).Get<float>()) <<
				Property("scene.skylight.gain")(props.Get(Property("gain")(1.f)).Get<float>() * gainAdjustFactor) <<
				Property("scene.skylight.transformation")(currentTransform.m) <<
				Property("scene.skylight.id")(currentGraphicsState.currentLightGroup);
	} else if ((name == "infinite") || (name == "infinitesample")) {
		*sceneProps <<
				Property("scene.infinitelight.file")(props.Get(Property("mapname")("")).Get<string>()) <<
				Property("scene.infinitelight.gamma")(props.Get(Property("gamma")(1.f)).Get<float>()) <<
				Property("scene.infinitelight.gain")(props.Get(Property("gain")(1.f)).Get<float>() *
					props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property("scene.infinitelight.transformation")(currentTransform.m) <<
				Property("scene.infinitelight.id")(currentGraphicsState.currentLightGroup);
	}

	FreeArgs();
}
| LOOKAT NUM NUM NUM NUM NUM NUM NUM NUM NUM
{
	*sceneProps <<
			Property("scene.camera.lookat.orig")($2, $3, $4) <<
			Property("scene.camera.lookat.target")($5, $6, $7) <<
			Property("scene.camera.lookat.up")($8, $9, $10);
}
| MATERIAL STRING paramlist
{
	currentGraphicsState.materialName = "";
	InitProperties(currentGraphicsState.materialProps, CPS, CP);

	FreeArgs();
}
| MAKENAMEDMATERIAL STRING paramlist
{
	const string name($2);
	if (namedMaterials.count(name))
		throw runtime_error("Named material '" + name + "' already defined");

	Properties props;
	InitProperties(props, CPS, CP);
	DefineMaterial(name, props);

	FreeArgs();
}
| MAKENAMEDVOLUME STRING STRING paramlist
{
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
	const string name($2);
	if (!namedMaterials.count(name))
		throw std::runtime_error("Named material '" + name + "' unknown");

	currentGraphicsState.materialName = name;
}
| OBJECTBEGIN STRING
{
	//luxObjectBegin($2);
}
| OBJECTEND
{
	//luxObjectEnd();
}
| OBJECTINSTANCE STRING
{
	//luxObjectInstance($2);
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
		*sceneProps <<
				Property("film.filter.type")("BOX");
	} else if (name == "gaussian") {
		*sceneProps <<
				Property("film.filter.type")("GAUSSIAN");
	} else if (name == "mitchell") {
		const bool supersample = props.Get(Property("supersample")(false)).Get<bool>();
		*sceneProps <<
				Property("film.filter.type")(supersample ? "MITCHELL_SS" : "MITCHELL");
	} else {
		LC_LOG("LuxCore doesn't support the filter type " + name + ", using MITCHELL filter instead");
		*sceneProps <<
				Property("film.filter.type")("MITCHELL");
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
		*sceneProps <<
				Property("sampler.type")("METROPOLIS") <<
				Property("sampler.metropolis.maxconsecutivereject")(Property("maxconsecrejects")(512).Get<u_int>()) <<
				Property("sampler.metropolis.largesteprate")(Property("largemutationprob")(.4f).Get<float>()) <<
				Property("sampler.metropolis.imagemutationrate")(Property("mutationrange")(.1f).Get<float>());
	} else if ((name == "sobol") || (name == "lowdiscrepancy")) {
		*sceneProps <<
				Property("sampler.type")("SOBOL");
	} else if (name == "random") {
		*sceneProps <<
				Property("sampler.type")("RANDOM");
	} else {
		LC_LOG("LuxCore doesn't support the sampler type " + name + ", falling back to random sampler");
		*sceneProps <<
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
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Shape($2, params);
	FreeArgs();
}
| PORTALSHAPE STRING paramlist
{
	FreeArgs();
}
| SURFACEINTEGRATOR STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->SurfaceIntegrator($2, params);
	FreeArgs();
}
| TEXTURE STRING STRING STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Texture($2, $3, $4, params);
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
/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         luxcore_parserlxs_yyparse
#define yylex           luxcore_parserlxs_yylex
#define yyerror         luxcore_parserlxs_yyerror
#define yydebug         luxcore_parserlxs_yydebug
#define yynerrs         luxcore_parserlxs_yynerrs

#define yylval          luxcore_parserlxs_yylval
#define yychar          luxcore_parserlxs_yychar

/* Copy the first part of user declarations.  */
#line 22 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:339  */


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
static boost::unordered_map<string, Properties> namedMaterials;
// The named Textures
static boost::unordered_set<string> namedTextures;
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
	namedTextures.clear();
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

static void DefineMaterial(const string &name, const Properties &matProps, const Properties &lightProps) {
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
					Property(prefix + ".n")(fresnelTexName) <<
					Property(prefix + ".k")(Spectrum(.5f));
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
				Property(prefix + ".emission.id")(currentGraphicsState.currentLightGroup) <<
				Property(prefix + ".emission.mapfile")(lightProps.Get(Property("mapname")("")).Get<string>()) <<
				Property(prefix + ".emission.iesfile")(lightProps.Get(Property("iesname")("")).Get<string>()) <<
				Property(prefix + ".emission.flipz")(lightProps.Get(Property("flipz")(false)).Get<bool>());
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


#line 743 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "luxparse.hpp".  */
#ifndef YY_LUXCORE_PARSERLXS_YY_HOME_DAVID_PROJECTS_LUXCORERENDER_LUXCORE_SRC_LUXCORE_LUXPARSER_LUXPARSE_HPP_INCLUDED
# define YY_LUXCORE_PARSERLXS_YY_HOME_DAVID_PROJECTS_LUXCORERENDER_LUXCORE_SRC_LUXCORE_LUXPARSER_LUXPARSE_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int luxcore_parserlxs_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    STRING = 258,
    ID = 259,
    NUM = 260,
    LBRACK = 261,
    RBRACK = 262,
    ACCELERATOR = 263,
    AREALIGHTSOURCE = 264,
    ATTRIBUTEBEGIN = 265,
    ATTRIBUTEEND = 266,
    CAMERA = 267,
    CONCATTRANSFORM = 268,
    COORDINATESYSTEM = 269,
    COORDSYSTRANSFORM = 270,
    EXTERIOR = 271,
    FILM = 272,
    IDENTITY = 273,
    INTERIOR = 274,
    LIGHTSOURCE = 275,
    LOOKAT = 276,
    MATERIAL = 277,
    MAKENAMEDMATERIAL = 278,
    MAKENAMEDVOLUME = 279,
    MOTIONBEGIN = 280,
    MOTIONEND = 281,
    NAMEDMATERIAL = 282,
    OBJECTBEGIN = 283,
    OBJECTEND = 284,
    OBJECTINSTANCE = 285,
    PORTALINSTANCE = 286,
    MOTIONINSTANCE = 287,
    LIGHTGROUP = 288,
    PIXELFILTER = 289,
    RENDERER = 290,
    REVERSEORIENTATION = 291,
    ROTATE = 292,
    SAMPLER = 293,
    SCALE = 294,
    SEARCHPATH = 295,
    PORTALSHAPE = 296,
    SHAPE = 297,
    SURFACEINTEGRATOR = 298,
    TEXTURE = 299,
    TRANSFORMBEGIN = 300,
    TRANSFORMEND = 301,
    TRANSFORM = 302,
    TRANSLATE = 303,
    VOLUME = 304,
    VOLUMEINTEGRATOR = 305,
    WORLDBEGIN = 306,
    WORLDEND = 307,
    HIGH_PRECEDENCE = 308
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 691 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:355  */

char string[1024];
float num;
ParamArray *ribarray;

#line 843 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:355  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE luxcore_parserlxs_yylval;

int luxcore_parserlxs_yyparse (void);

#endif /* !YY_LUXCORE_PARSERLXS_YY_HOME_DAVID_PROJECTS_LUXCORERENDER_LUXCORE_SRC_LUXCORE_LUXPARSER_LUXPARSE_HPP_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 858 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  88
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   144

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  54
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  22
/* YYNRULES -- Number of rules.  */
#define YYNRULES  73
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  156

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   308

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   716,   716,   720,   728,   733,   738,   742,   747,   751,
     757,   762,   766,   769,   773,   779,   783,   788,   793,   797,
     800,   804,   810,   814,   819,   823,   826,   843,   846,   850,
     862,   869,   874,   885,   920,   933,   937,   946,   949,   995,
     999,  1002,  1015,  1179,  1186,  1193,  1206,  1210,  1214,  1217,
    1226,  1230,  1234,  1238,  1241,  1244,  1284,  1308,  1311,  1316,
    1342,  1347,  1350,  1439,  1443,  1469,  1831,  1835,  1844,  1858,
    1863,  1867,  1871,  1874
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "STRING", "ID", "NUM", "LBRACK",
  "RBRACK", "ACCELERATOR", "AREALIGHTSOURCE", "ATTRIBUTEBEGIN",
  "ATTRIBUTEEND", "CAMERA", "CONCATTRANSFORM", "COORDINATESYSTEM",
  "COORDSYSTRANSFORM", "EXTERIOR", "FILM", "IDENTITY", "INTERIOR",
  "LIGHTSOURCE", "LOOKAT", "MATERIAL", "MAKENAMEDMATERIAL",
  "MAKENAMEDVOLUME", "MOTIONBEGIN", "MOTIONEND", "NAMEDMATERIAL",
  "OBJECTBEGIN", "OBJECTEND", "OBJECTINSTANCE", "PORTALINSTANCE",
  "MOTIONINSTANCE", "LIGHTGROUP", "PIXELFILTER", "RENDERER",
  "REVERSEORIENTATION", "ROTATE", "SAMPLER", "SCALE", "SEARCHPATH",
  "PORTALSHAPE", "SHAPE", "SURFACEINTEGRATOR", "TEXTURE", "TRANSFORMBEGIN",
  "TRANSFORMEND", "TRANSFORM", "TRANSLATE", "VOLUME", "VOLUMEINTEGRATOR",
  "WORLDBEGIN", "WORLDEND", "HIGH_PRECEDENCE", "$accept", "start",
  "array_init", "string_array_init", "num_array_init", "array",
  "string_array", "real_string_array", "single_element_string_array",
  "string_list", "string_list_entry", "num_array", "real_num_array",
  "single_element_num_array", "num_list", "num_list_entry", "paramlist",
  "paramlist_init", "paramlist_contents", "paramlist_entry",
  "ri_stmt_list", "ri_stmt", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308
};
# endif

#define YYPACT_NINF -140

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-140)))

#define YYTABLE_NINF -6

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      92,     6,     8,  -140,  -140,    10,  -140,    13,    14,    15,
      16,  -140,    17,    18,    21,    19,    25,    26,  -140,  -140,
      31,    32,  -140,    33,    36,    37,    38,    39,    40,  -140,
      41,    42,    43,    44,    46,    47,    48,    49,  -140,  -140,
    -140,    50,    51,    54,  -140,  -140,    58,    92,  -140,  -140,
    -140,  -140,    53,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
    -140,  -140,    55,  -140,  -140,    59,  -140,  -140,  -140,  -140,
    -140,    56,  -140,  -140,  -140,    60,  -140,    61,  -140,  -140,
    -140,  -140,    64,    53,  -140,    63,  -140,  -140,  -140,  -140,
    -140,    66,  -140,  -140,  -140,    65,  -140,  -140,  -140,    67,
    -140,  -140,  -140,    68,  -140,  -140,  -140,    69,  -140,    70,
    -140,  -140,  -140,    73,    72,  -140,  -140,  -140,  -140,    66,
      57,  -140,  -140,    76,  -140,    75,    77,  -140,  -140,  -140,
       0,  -140,  -140,  -140,  -140,  -140,  -140,  -140,  -140,    78,
    -140,  -140,  -140,    79,    82,  -140,    81,    80,  -140,  -140,
      83,  -140,  -140,    84,    85,  -140
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,    31,    32,     0,     3,     0,     0,     0,
       0,    39,     0,     0,     0,     0,     0,     0,     3,    48,
       0,     0,    51,     0,     0,     0,     0,     0,     0,    57,
       0,     0,     0,     0,     0,     0,     0,     0,    66,    67,
       3,     0,     0,     0,    72,    73,     0,     2,    28,    23,
      23,    23,     5,    34,    15,    16,    35,    36,    37,    23,
      40,    23,     0,    23,    23,     0,    47,    49,    50,    52,
      53,     0,    23,    23,    23,     0,    23,     0,    61,    23,
      23,    23,     0,     0,    68,     0,    23,    23,     1,    27,
      29,    25,    30,    33,     5,     0,    18,    38,    42,     0,
      44,    45,    23,     0,    41,    55,    56,     0,    59,     0,
      63,    62,    64,     0,     0,    71,    70,     3,    22,    25,
       5,    20,    21,     0,    46,     0,     0,    60,    23,    69,
       4,    26,     6,     8,     9,     7,    24,    17,    19,     0,
      54,    58,    65,     4,     0,    11,     0,     4,    13,    14,
       0,    10,    12,     0,     0,    43
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -140,  -140,   -37,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
    -139,   -18,     4,  -140,  -140,   -87,   -49,  -140,   -56,  -140,
    -140,    24
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    46,    52,   144,    95,   131,   132,   133,   134,   147,
     145,    53,    54,    55,   120,    96,    90,    91,   118,   119,
      47,    48
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      66,    92,    93,    83,   148,    -5,   143,   121,   152,    49,
      97,    50,    98,    51,   100,   101,    56,    57,    58,    59,
      60,    61,    63,   104,   105,   106,    62,   108,    64,    65,
     110,   111,   112,   138,    67,    68,    69,   115,   116,    70,
      71,    72,    73,    74,    84,    76,    75,    78,    77,    79,
      80,    81,    82,   124,    86,    85,   121,    87,    88,    94,
      99,   103,   102,   136,   137,   107,   109,   113,   114,   117,
     122,    89,   123,   125,   126,   127,   128,   129,   140,   142,
     130,   139,   141,   146,    -5,   149,   150,   151,   153,   154,
     155,     0,     0,     0,     0,     0,     0,     0,     0,   135,
       1,     2,     3,     4,     5,     6,     7,     8,     9,    10,
      11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
      31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
      41,    42,    43,    44,    45
};

static const yytype_int16 yycheck[] =
{
      18,    50,    51,    40,   143,     5,     6,    94,   147,     3,
      59,     3,    61,     3,    63,    64,     3,     3,     3,     3,
       3,     3,     3,    72,    73,    74,     5,    76,     3,     3,
      79,    80,    81,   120,     3,     3,     3,    86,    87,     3,
       3,     3,     3,     3,    40,     3,     5,     3,     5,     3,
       3,     3,     3,   102,     3,     5,   143,     3,     0,     6,
       5,     5,     3,   119,     7,     5,     5,     3,     5,     3,
       5,    47,     5,     5,     5,     5,     3,     5,     3,   128,
     117,     5,     5,     5,     5,     3,     5,     7,     5,     5,
       5,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   117,
       8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     8,     9,    10,    11,    12,    13,    14,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    55,    74,    75,     3,
       3,     3,    56,    65,    66,    67,     3,     3,     3,     3,
       3,     3,     5,     3,     3,     3,    65,     3,     3,     3,
       3,     3,     3,     3,     3,     5,     3,     5,     3,     3,
       3,     3,     3,    56,    66,     5,     3,     3,     0,    75,
      70,    71,    70,    70,     6,    58,    69,    70,    70,     5,
      70,    70,     3,     5,    70,    70,    70,     5,    70,     5,
      70,    70,    70,     3,     5,    70,    70,     3,    72,    73,
      68,    69,     5,     5,    70,     5,     5,     5,     3,     5,
      56,    59,    60,    61,    62,    65,    72,     7,    69,     5,
       3,     5,    70,     6,    57,    64,     5,    63,    64,     3,
       5,     7,    64,     5,     5,     5
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    54,    55,    56,    57,    58,    59,    59,    60,    60,
      61,    62,    63,    63,    64,    65,    65,    66,    67,    68,
      68,    69,    70,    71,    72,    72,    73,    74,    74,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      75,    75,    75,    75
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     0,     0,     0,     1,     1,     1,     1,
       4,     2,     2,     1,     2,     1,     1,     4,     2,     2,
       1,     2,     2,     0,     2,     0,     2,     2,     1,     3,
       3,     1,     1,     3,     2,     2,     2,     2,     3,     1,
       2,     3,     3,    10,     3,     3,     4,     2,     1,     2,
       2,     1,     2,     2,     5,     3,     3,     1,     5,     3,
       4,     2,     3,     3,     3,     5,     1,     1,     2,     4,
       3,     3,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
{
  YYUSE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 717 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2048 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 3:
#line 721 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	if (curArray)
		ArrayFree(curArray);
	curArray = new ParamArray;
	arrayIsSingleString = false;
}
#line 2059 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 4:
#line 729 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	curArray->elementSize = sizeof(const char *);
}
#line 2067 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 5:
#line 734 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	curArray->elementSize = sizeof(float);
}
#line 2075 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 6:
#line 739 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = (yyvsp[0].ribarray);
}
#line 2083 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 7:
#line 743 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = (yyvsp[0].ribarray);
}
#line 2091 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 8:
#line 748 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = (yyvsp[0].ribarray);
}
#line 2099 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 9:
#line 752 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = ArrayDup(curArray);
	arrayIsSingleString = true;
}
#line 2108 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 10:
#line 758 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = ArrayDup(curArray);
}
#line 2116 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 11:
#line 763 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2123 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 12:
#line 767 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2130 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 13:
#line 770 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2137 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 14:
#line 774 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	char *toAdd = strdup((yyvsp[0].string));
	AddArrayElement(&toAdd);
}
#line 2146 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 15:
#line 780 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = (yyvsp[0].ribarray);
}
#line 2154 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 16:
#line 784 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = ArrayDup(curArray);
}
#line 2162 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 17:
#line 789 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	(yyval.ribarray) = ArrayDup(curArray);
}
#line 2170 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 18:
#line 794 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2177 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 19:
#line 798 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2184 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 20:
#line 801 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2191 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 21:
#line 805 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	float toAdd = (yyvsp[0].num);
	AddArrayElement(&toAdd);
}
#line 2200 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 22:
#line 811 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2207 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 23:
#line 815 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	CPS = 0;
}
#line 2215 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 24:
#line 820 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2222 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 25:
#line 823 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2229 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 26:
#line 827 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	void *arg = new char[(yyvsp[0].ribarray)->nelems * (yyvsp[0].ribarray)->elementSize];
	memcpy(arg, (yyvsp[0].ribarray)->array, (yyvsp[0].ribarray)->nelems * (yyvsp[0].ribarray)->elementSize);
	if (CPS >= CPAL) {
		CPAL = 2 * CPAL + 1;
		CP = static_cast<ParamListElem *>(realloc(CP,
			CPAL * sizeof(ParamListElem)));
	}
	CPT(CPS) = (yyvsp[-1].string);
	CPA(CPS) = arg;
	CPSZ(CPS) = (yyvsp[0].ribarray)->nelems;
	CPTH(CPS) = arrayIsSingleString;
	++CPS;
	ArrayFree((yyvsp[0].ribarray));
}
#line 2249 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 27:
#line 844 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2256 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 28:
#line 847 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2263 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 29:
#line 851 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	// Map kdtree and bvh to luxrays' bvh accel otherwise just use the default settings
	const string name((yyvsp[-1].string));
	if ((name =="kdtree") || (name =="bvh"))
		*renderConfigProps << Property("accelerator.type")("BVH");
		
	FreeArgs();
}
#line 2279 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 30:
#line 863 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	currentGraphicsState.areaLightName = (yyvsp[-1].string);
	InitProperties(currentGraphicsState.areaLightProps, CPS, CP);

	FreeArgs();
}
#line 2290 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 31:
#line 870 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	graphicsStatesStack.push_back(currentGraphicsState);
	transformsStack.push_back(currentTransform);
}
#line 2299 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 32:
#line 875 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
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
#line 2314 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 33:
#line 886 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	const string name((yyvsp[-1].string));
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
#line 2353 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 34:
#line 921 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	if (VerifyArrayLength((yyvsp[0].ribarray), 16, "ConcatTransform")) {
		const float *tr = static_cast<float *>((yyvsp[0].ribarray)->array);
		Transform t(Matrix4x4(tr[0], tr[4], tr[8], tr[12],
				tr[1], tr[5], tr[9], tr[13],
				tr[2], tr[6], tr[10], tr[14],
				tr[3], tr[7], tr[11], tr[15]));

		currentTransform = currentTransform * t;
	}
	ArrayFree((yyvsp[0].ribarray));
}
#line 2370 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 35:
#line 934 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	namedCoordinateSystems[(yyvsp[0].string)] = currentTransform;
}
#line 2378 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 36:
#line 938 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	const string name((yyvsp[0].string));
	if (namedCoordinateSystems.count(name))
		currentTransform = namedCoordinateSystems[name];
	else {
		throw runtime_error("Coordinate system '" + name + "' unknown");
	}
}
#line 2391 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 37:
#line 947 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2398 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 38:
#line 950 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
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

	FreeArgs();
}
#line 2448 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 39:
#line 996 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	currentTransform = Transform();
}
#line 2456 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 40:
#line 1000 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2463 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 41:
#line 1003 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	const string name((yyvsp[-1].string));
	if (namedLightGroups.count(name))
		currentGraphicsState.currentLightGroup = namedLightGroups[name];
	else {
		// It is a new light group
		currentGraphicsState.currentLightGroup = freeLightGroupIndex;
		namedLightGroups[name] = freeLightGroupIndex++;
	}

	FreeArgs();
}
#line 2480 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 42:
#line 1016 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
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

	const string name((yyvsp[-1].string));
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
		
		*sceneProps <<
				Property(prefix + ".position")(props.Get(Property("from")(Point(0.f, 0.f, 0.f))).Get<Point>()) <<
				Property(prefix + ".color")(props.Get(Property("L")(Spectrum(1.f))).Get<Spectrum>()) <<
				Property(prefix + ".gain")(Spectrum(props.Get(Property("gain")(1.f)).Get<float>())) <<
				Property(prefix + ".power")(props.Get(Property("power")(0.f)).Get<float>()) <<
				Property(prefix + ".efficency")(props.Get(Property("efficency")(0.f)).Get<float>()) <<
				Property(prefix + ".transformation")(currentTransform.m) <<
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
#line 2648 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 43:
#line 1180 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	*sceneProps <<
			Property("scene.camera.lookat.orig")((yyvsp[-8].num), (yyvsp[-7].num), (yyvsp[-6].num)) <<
			Property("scene.camera.lookat.target")((yyvsp[-5].num), (yyvsp[-4].num), (yyvsp[-3].num)) <<
			Property("scene.camera.up")((yyvsp[-2].num), (yyvsp[-1].num), (yyvsp[0].num));
}
#line 2659 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 44:
#line 1187 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	currentGraphicsState.materialName = "";
	InitProperties(currentGraphicsState.materialProps, CPS, CP);

	FreeArgs();
}
#line 2670 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 45:
#line 1194 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	string name = GetLuxCoreValidName((yyvsp[-1].string));
	if (namedMaterials.count(name))
		throw runtime_error("Named material '" + name + "' already defined");

	Properties props;
	InitProperties(props, CPS, CP);
	namedMaterials[name] = props;
	DefineMaterial(name, props, Properties());

	FreeArgs();
}
#line 2687 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 46:
#line 1207 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	FreeArgs();
}
#line 2695 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 47:
#line 1211 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	ArrayFree((yyvsp[0].ribarray));
}
#line 2703 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 48:
#line 1215 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2710 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 49:
#line 1218 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	const string name = GetLuxCoreValidName((yyvsp[0].string));
	if (!namedMaterials.count(name))
		throw runtime_error("Named material '" + name + "' unknown");

	currentGraphicsState.materialName = name;
	currentGraphicsState.materialProps = namedMaterials[name];
}
#line 2723 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 50:
#line 1227 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	//luxObjectBegin($2);
}
#line 2731 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 51:
#line 1231 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	//luxObjectEnd();
}
#line 2739 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 52:
#line 1235 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	//luxObjectInstance($2);
}
#line 2747 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 53:
#line 1239 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2754 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 54:
#line 1242 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2761 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 55:
#line 1245 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	const string name((yyvsp[-1].string));
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
#line 2805 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 56:
#line 1285 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	// Check the name of the renderer
	if ((strcmp((yyvsp[-1].string), "slg") == 0) || (strcmp((yyvsp[-1].string), "slg") == 0)) {
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
#line 2833 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 57:
#line 1309 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2840 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 58:
#line 1312 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Transform t(Rotate((yyvsp[-3].num), Vector((yyvsp[-2].num), (yyvsp[-1].num), (yyvsp[0].num))));
	currentTransform = currentTransform * t;
}
#line 2849 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 59:
#line 1317 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	const string name((yyvsp[-1].string));
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
#line 2879 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 60:
#line 1343 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Transform t(Scale((yyvsp[-2].num), (yyvsp[-1].num), (yyvsp[0].num)));
	currentTransform = currentTransform * t;
}
#line 2888 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 61:
#line 1348 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 2895 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 62:
#line 1351 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	// Define object name
	string objName;
	if (props.IsDefined("name")) {
		// LuxRender object names are not unique
		objName = props.Get("name").Get<string>();
	} else
		objName = "LUXCORE_OBJECT";
	objName += "_" + ToString(freeObjectID++);
	objName = GetLuxCoreValidName(objName);
	const string prefix = "scene.objects." + objName;

	// Define object material
	if (currentGraphicsState.materialName == "") {
		// Define the used material on-the-fly
		DefineMaterial("LUXCORE_MATERIAL_" + objName, currentGraphicsState.materialProps,
				currentGraphicsState.areaLightProps);
		*sceneProps <<
			Property(prefix + ".material")("LUXCORE_MATERIAL_" + objName);
	} else {
		// It is a named material
		if (currentGraphicsState.areaLightName == "") {
			// I can use the already defined material without emission
			*sceneProps <<
				Property(prefix + ".material")(currentGraphicsState.materialName);
		} else {
			// I have to define a new material with emission
			DefineMaterial("LUXCORE_MATERIAL_" + objName, currentGraphicsState.materialProps,
				currentGraphicsState.areaLightProps);
			*sceneProps <<
				Property(prefix + ".material")("LUXCORE_MATERIAL_" + objName);
		}
	}

	const string name((yyvsp[-1].string));
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
			if ((uvsProps.GetSize() == 0) || (uvsProps.GetSize() != pointsProp.GetSize()))
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
#line 2988 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 63:
#line 1440 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	FreeArgs();
}
#line 2996 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 64:
#line 1444 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	const string name((yyvsp[-1].string));
	if (name == "path") {
		// Path tracing
		*renderConfigProps <<
			Property("renderengine.type")("PATHOCL") <<
			Property("path.maxdepth")(props.Get(Property("maxdepth")(16u)).Get<u_int>());
	} else if (name == "bidirectional") {
		// Bidirectional path tracing
		*renderConfigProps <<
			Property("renderengine.type")("BIDIRVMCPU") <<
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
#line 3026 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 65:
#line 1470 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Properties props;
	InitProperties(props, CPS, CP);

	string name = GetLuxCoreValidName((yyvsp[-3].string));
	if (namedTextures.count(name))
		LC_LOG("Texture '" << name << "' being redefined.");
	namedTextures.insert(name);

	const string texType((yyvsp[-1].string));
	const string prefix = "scene.textures." + name;

	if (texType == "constant") {
		const string channels((yyvsp[-2].string));

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
			throw runtime_error("Insufficient wavelengths in an irregulardata texture");

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
		const u_int dimesion = props.Get(Property("dimension")(2)).Get<u_int>();

		*sceneProps <<
				Property(prefix + ".type")((dimesion == 2) ? "checkerboard2d" : "checkerboard3d") <<
				GetTexture(prefix + ".texture1", Property("tex1")(Spectrum(1.f)), props) <<
				GetTexture(prefix + ".texture2", Property("tex2")(Spectrum(0.f)), props) <<
				((dimesion == 2) ? GetTextureMapping2D(prefix, props) : GetTextureMapping3D(prefix, currentTransform, props));
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
		*sceneProps <<
				Property(prefix + ".type")("fresnelcolor") <<
				Property(prefix + ".kr")(props.Get(Property("Kr")(Spectrum(.5f))).Get<Spectrum>());
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
#line 3392 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 66:
#line 1832 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	transformsStack.push_back(currentTransform);
}
#line 3400 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 67:
#line 1836 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	if (!(transformsStack.size() > graphicsStatesStack.size())) {
		LC_LOG("Unmatched TransformEnd encountered. Ignoring it.");
	} else {
		currentTransform = transformsStack.back();
		transformsStack.pop_back();
	}
}
#line 3413 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 68:
#line 1845 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	if (VerifyArrayLength((yyvsp[0].ribarray), 16, "Transform")) {
		const float *tr = static_cast<float *>((yyvsp[0].ribarray)->array);
		Transform t(Matrix4x4(tr[0], tr[4], tr[8], tr[12],
				tr[1], tr[5], tr[9], tr[13],
				tr[2], tr[6], tr[10], tr[14],
				tr[3], tr[7], tr[11], tr[15]));

		currentTransform = t;
	}

	ArrayFree((yyvsp[0].ribarray));
}
#line 3431 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 69:
#line 1859 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	Transform t(Translate(Vector((yyvsp[-2].num), (yyvsp[-1].num), (yyvsp[0].num))));
	currentTransform = currentTransform * t;
}
#line 3440 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 70:
#line 1864 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	FreeArgs();
}
#line 3448 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 71:
#line 1868 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
	FreeArgs();
}
#line 3456 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 72:
#line 1872 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 3463 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;

  case 73:
#line 1875 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1646  */
    {
}
#line 3470 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
    break;


#line 3474 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.cpp" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1877 "/home/david/projects/luxcorerender/LuxCore/src/luxcore/luxparser/luxparse.y" /* yacc.c:1906  */


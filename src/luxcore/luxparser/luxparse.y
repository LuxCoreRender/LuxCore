
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
		reverseOrientation = false;
	}
	~GraphicsState() {
	}

	Properties areaLightProps;

	u_int currentLightGroup;
	bool reverseOrientation;
};

// The GraphicsState stack
static vector<GraphicsState> graphicsStatesStack;
static GraphicsState currentGraphicsState;
// The transformations stack
static vector<Transform> transformsStack;
static Transform currentTransform;
// The named coordinate systems
static boost::unordered_map<string, Transform> namedCoordinateSystems;

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
	if ((strcmp($2, "kdtree") == 0) || (strcmp($2, "bvh") == 0))
		*renderConfigProps << Property("accelerator.type")("BVH");
		
	FreeArgs();
}
| AREALIGHTSOURCE STRING paramlist
{
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
	currentGraphicsState = graphicsStatesStack.back();
	graphicsStatesStack.pop_back();
	currentTransform = transformsStack.back();
	transformsStack.pop_back();
}
| CAMERA STRING paramlist
{
	if (strcmp($2, "perspective") != 0)
		throw std::runtime_error("LuxCore supports only perspective camera");

	Properties props;
	InitProperties(props, CPS, CP);
	
	if (props.IsDefined("screenwindow")) {
		Property prop = props.Get("screenwindow");
		*sceneProps << Property("scene.camera.screenwindow")(
			prop.Get<float>(0), prop.Get<float>(1), prop.Get<float>(2), prop.Get<float>(3));
	}
	
	*sceneProps << Property("scene.camera.fieldofview")(props.Get(Property("fov")(90.f)).Get<float>()) <<
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
	string name($2);
	if (namedCoordinateSystems.find(name) != namedCoordinateSystems.end())
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
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->LightGroup($2, params);
	FreeArgs();
}
| LIGHTSOURCE STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->LightSource($2, params);
	FreeArgs();
}
| LOOKAT NUM NUM NUM NUM NUM NUM NUM NUM NUM
{
	//luxLookAt($2, $3, $4, $5, $6, $7, $8, $9, $10);
}
| MATERIAL STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Material($2, params);
	FreeArgs();
}
| MAKENAMEDMATERIAL STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->MakeNamedMaterial($2, params);
	FreeArgs();
}
| MAKENAMEDVOLUME STRING STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->MakeNamedVolume($2, $3, params);
	FreeArgs();
}
| MOTIONBEGIN num_array
{
	//luxMotionBegin($2->nelems, static_cast<float *>($2->array));
	ArrayFree($2);
}
| MOTIONEND
{
	//luxMotionEnd();
}
| NAMEDMATERIAL STRING
{
	//Context::GetActive()->NamedMaterial($2);
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
	//luxPortalInstance($2);
}
| MOTIONINSTANCE STRING NUM NUM STRING
{
	//luxMotionInstance($2, $3, $4, $5);
}
| PIXELFILTER STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->PixelFilter($2, params);
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
	//luxReverseOrientation();
}
| ROTATE NUM NUM NUM NUM
{
	//luxRotate($2, $3, $4, $5);
}
| SAMPLER STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Sampler($2, params);
	FreeArgs();
}
| SCALE NUM NUM NUM
{
	//luxScale($2, $3, $4);
}
| SEARCHPATH STRING
{
	//;//luxSearchPath($2);//FIXME - Unimplemented
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
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->PortalShape($2, params);
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
	//luxTransformBegin();
}
| TRANSFORMEND
{
	//luxTransformEnd();
}
| TRANSFORM real_num_array
{
	//if (VerifyArrayLength($2, 16, "Transform"))
	//	luxTransform(static_cast<float *>($2->array));
	ArrayFree($2);
}
| TRANSLATE NUM NUM NUM
{
	//luxTranslate($2, $3, $4);
}
| VOLUMEINTEGRATOR STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->VolumeIntegrator($2, params);
	FreeArgs();
}
| VOLUME STRING paramlist
{
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Volume($2, params);
	FreeArgs();
}
| WORLDBEGIN
{
	//luxWorldBegin();
}
| WORLDEND
{
	//luxWorldEnd();
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
			LC_LOG("Bad type for " << name << ". Changing it to a texture.");
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
					LC_LOG("Value '" << s << "' unknown for boolean parameter '" << list[i].token << "'. Using 'false'.");
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
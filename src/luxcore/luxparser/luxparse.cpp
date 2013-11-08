/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Copy the first part of user declarations.  */

/* Line 268 of yacc.c  */
#line 24 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"


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

	string areaLightName, materialName;
	Properties areaLightProps, materialProps;

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
		LC_LOG("LuxCore ParserLXS supports only Matte, Mirror, Glass, Metal, MatteTranslucent, Null, Mix, Glossy2, Metal2 and RoughGlass material (i.e. not " <<
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



/* Line 268 of yacc.c  */
#line 405 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.cpp"

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
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



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 293 of yacc.c  */
#line 357 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"

char string[1024];
float num;
ParamArray *ribarray;



/* Line 293 of yacc.c  */
#line 502 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.cpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


/* Copy the second part of user declarations.  */


/* Line 343 of yacc.c  */
#line 514 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.cpp"

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
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
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
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
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
/* YYNRULES -- Number of states.  */
#define YYNSTATES  156

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   308

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
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
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     6,     7,     8,    10,    12,    14,
      16,    21,    24,    27,    29,    32,    34,    36,    41,    44,
      47,    49,    52,    55,    56,    59,    60,    63,    66,    68,
      72,    76,    78,    80,    84,    87,    90,    93,    96,   100,
     102,   105,   109,   113,   124,   128,   132,   137,   140,   142,
     145,   148,   150,   153,   156,   162,   166,   170,   172,   178,
     182,   187,   190,   194,   198,   202,   208,   210,   212,   215,
     220,   224,   228,   230
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      55,     0,    -1,    74,    -1,    -1,    -1,    -1,    60,    -1,
      65,    -1,    61,    -1,    62,    -1,    56,     6,    63,     7,
      -1,    56,    64,    -1,    63,    64,    -1,    64,    -1,    57,
       3,    -1,    66,    -1,    67,    -1,    56,     6,    68,     7,
      -1,    56,    69,    -1,    68,    69,    -1,    69,    -1,    58,
       5,    -1,    71,    72,    -1,    -1,    73,    72,    -1,    -1,
       3,    59,    -1,    74,    75,    -1,    75,    -1,     8,     3,
      70,    -1,     9,     3,    70,    -1,    10,    -1,    11,    -1,
      12,     3,    70,    -1,    13,    65,    -1,    14,     3,    -1,
      15,     3,    -1,    16,     3,    -1,    17,     3,    70,    -1,
      18,    -1,    19,     3,    -1,    33,     3,    70,    -1,    20,
       3,    70,    -1,    21,     5,     5,     5,     5,     5,     5,
       5,     5,     5,    -1,    22,     3,    70,    -1,    23,     3,
      70,    -1,    24,     3,     3,    70,    -1,    25,    65,    -1,
      26,    -1,    27,     3,    -1,    28,     3,    -1,    29,    -1,
      30,     3,    -1,    31,     3,    -1,    32,     3,     5,     5,
       3,    -1,    34,     3,    70,    -1,    35,     3,    70,    -1,
      36,    -1,    37,     5,     5,     5,     5,    -1,    38,     3,
      70,    -1,    39,     5,     5,     5,    -1,    40,     3,    -1,
      42,     3,    70,    -1,    41,     3,    70,    -1,    43,     3,
      70,    -1,    44,     3,     3,     3,    70,    -1,    45,    -1,
      46,    -1,    47,    66,    -1,    48,     5,     5,     5,    -1,
      50,     3,    70,    -1,    49,     3,    70,    -1,    51,    -1,
      52,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   382,   382,   386,   394,   399,   404,   408,   413,   417,
     423,   428,   432,   435,   439,   445,   449,   454,   459,   463,
     466,   470,   476,   480,   485,   489,   492,   509,   512,   516,
     528,   535,   540,   547,   574,   587,   591,   600,   603,   614,
     618,   621,   634,   671,   678,   685,   697,   701,   705,   708,
     712,   716,   720,   724,   728,   732,   739,   763,   767,   771,
     778,   782,   786,   793,   800,   807,   814,   818,   822,   828,
     832,   839,   846,   850
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
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
  "ri_stmt_list", "ri_stmt", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
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

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
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

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
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

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,    46,    52,   144,    95,   131,   132,   133,   134,   147,
     145,    53,    54,    55,   120,    96,    90,    91,   118,   119,
      47,    48
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -140
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

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -140,  -140,   -37,  -140,  -140,  -140,  -140,  -140,  -140,  -140,
    -139,   -18,     4,  -140,  -140,   -87,   -49,  -140,   -56,  -140,
    -140,    24
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -6
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

#define yypact_value_is_default(yystate) \
  ((yystate) == (-140))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

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

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

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
#ifndef	YYINITDEPTH
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
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
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
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
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
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

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

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

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
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
  int yytoken;
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

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

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
      yychar = YYLEX;
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
  *++yyvsp = yylval;

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
     `$$ = $1'.

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

/* Line 1806 of yacc.c  */
#line 383 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 3:

/* Line 1806 of yacc.c  */
#line 387 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	if (curArray)
		ArrayFree(curArray);
	curArray = new ParamArray;
	arrayIsSingleString = false;
}
    break;

  case 4:

/* Line 1806 of yacc.c  */
#line 395 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	curArray->elementSize = sizeof(const char *);
}
    break;

  case 5:

/* Line 1806 of yacc.c  */
#line 400 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	curArray->elementSize = sizeof(float);
}
    break;

  case 6:

/* Line 1806 of yacc.c  */
#line 405 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = (yyvsp[(1) - (1)].ribarray);
}
    break;

  case 7:

/* Line 1806 of yacc.c  */
#line 409 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = (yyvsp[(1) - (1)].ribarray);
}
    break;

  case 8:

/* Line 1806 of yacc.c  */
#line 414 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = (yyvsp[(1) - (1)].ribarray);
}
    break;

  case 9:

/* Line 1806 of yacc.c  */
#line 418 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = ArrayDup(curArray);
	arrayIsSingleString = true;
}
    break;

  case 10:

/* Line 1806 of yacc.c  */
#line 424 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = ArrayDup(curArray);
}
    break;

  case 11:

/* Line 1806 of yacc.c  */
#line 429 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 12:

/* Line 1806 of yacc.c  */
#line 433 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 13:

/* Line 1806 of yacc.c  */
#line 436 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 14:

/* Line 1806 of yacc.c  */
#line 440 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	char *toAdd = strdup((yyvsp[(2) - (2)].string));
	AddArrayElement(&toAdd);
}
    break;

  case 15:

/* Line 1806 of yacc.c  */
#line 446 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = (yyvsp[(1) - (1)].ribarray);
}
    break;

  case 16:

/* Line 1806 of yacc.c  */
#line 450 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = ArrayDup(curArray);
}
    break;

  case 17:

/* Line 1806 of yacc.c  */
#line 455 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	(yyval.ribarray) = ArrayDup(curArray);
}
    break;

  case 18:

/* Line 1806 of yacc.c  */
#line 460 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 19:

/* Line 1806 of yacc.c  */
#line 464 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 20:

/* Line 1806 of yacc.c  */
#line 467 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 21:

/* Line 1806 of yacc.c  */
#line 471 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	float toAdd = (yyvsp[(2) - (2)].num);
	AddArrayElement(&toAdd);
}
    break;

  case 22:

/* Line 1806 of yacc.c  */
#line 477 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 23:

/* Line 1806 of yacc.c  */
#line 481 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	CPS = 0;
}
    break;

  case 24:

/* Line 1806 of yacc.c  */
#line 486 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 25:

/* Line 1806 of yacc.c  */
#line 489 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 26:

/* Line 1806 of yacc.c  */
#line 493 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	void *arg = new char[(yyvsp[(2) - (2)].ribarray)->nelems * (yyvsp[(2) - (2)].ribarray)->elementSize];
	memcpy(arg, (yyvsp[(2) - (2)].ribarray)->array, (yyvsp[(2) - (2)].ribarray)->nelems * (yyvsp[(2) - (2)].ribarray)->elementSize);
	if (CPS >= CPAL) {
		CPAL = 2 * CPAL + 1;
		CP = static_cast<ParamListElem *>(realloc(CP,
			CPAL * sizeof(ParamListElem)));
	}
	CPT(CPS) = (yyvsp[(1) - (2)].string);
	CPA(CPS) = arg;
	CPSZ(CPS) = (yyvsp[(2) - (2)].ribarray)->nelems;
	CPTH(CPS) = arrayIsSingleString;
	++CPS;
	ArrayFree((yyvsp[(2) - (2)].ribarray));
}
    break;

  case 27:

/* Line 1806 of yacc.c  */
#line 510 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 28:

/* Line 1806 of yacc.c  */
#line 513 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 29:

/* Line 1806 of yacc.c  */
#line 517 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	Properties props;
	InitProperties(props, CPS, CP);

	// Map kdtree and bvh to luxrays' bvh accel otherwise just use the default settings
	const string name((yyvsp[(2) - (3)].string));
	if ((name =="kdtree") || (name =="bvh"))
		*renderConfigProps << Property("accelerator.type")("BVH");
		
	FreeArgs();
}
    break;

  case 30:

/* Line 1806 of yacc.c  */
#line 529 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	currentGraphicsState.areaLightName = (yyvsp[(2) - (3)].string);
	InitProperties(currentGraphicsState.areaLightProps, CPS, CP);

	FreeArgs();
}
    break;

  case 31:

/* Line 1806 of yacc.c  */
#line 536 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	graphicsStatesStack.push_back(currentGraphicsState);
	transformsStack.push_back(currentTransform);
}
    break;

  case 32:

/* Line 1806 of yacc.c  */
#line 541 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	currentGraphicsState = graphicsStatesStack.back();
	graphicsStatesStack.pop_back();
	currentTransform = transformsStack.back();
	transformsStack.pop_back();
}
    break;

  case 33:

/* Line 1806 of yacc.c  */
#line 548 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	const string name((yyvsp[(2) - (3)].string));
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
    break;

  case 34:

/* Line 1806 of yacc.c  */
#line 575 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	if (VerifyArrayLength((yyvsp[(2) - (2)].ribarray), 16, "ConcatTransform")) {
		const float *tr = static_cast<float *>((yyvsp[(2) - (2)].ribarray)->array);
		Transform t(Matrix4x4(tr[0], tr[4], tr[8], tr[12],
				tr[1], tr[5], tr[9], tr[13],
				tr[2], tr[6], tr[10], tr[14],
				tr[3], tr[7], tr[11], tr[15]));

		currentTransform = currentTransform * t;
	}
	ArrayFree((yyvsp[(2) - (2)].ribarray));
}
    break;

  case 35:

/* Line 1806 of yacc.c  */
#line 588 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	namedCoordinateSystems[(yyvsp[(2) - (2)].string)] = currentTransform;
}
    break;

  case 36:

/* Line 1806 of yacc.c  */
#line 592 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	const string name((yyvsp[(2) - (2)].string));
	if (namedCoordinateSystems.count(name))
		currentTransform = namedCoordinateSystems[name];
	else {
		throw runtime_error("Coordinate system '" + name + "' unknown");
	}
}
    break;

  case 37:

/* Line 1806 of yacc.c  */
#line 601 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 38:

/* Line 1806 of yacc.c  */
#line 604 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	Properties props;
	InitProperties(props, CPS, CP);

	*renderConfigProps <<
			Property("film.width")(props.Get(Property("xresoluton")(800)).Get<u_int>()) <<
			Property("film.height")(props.Get(Property("yresoluton")(600)).Get<u_int>());

	FreeArgs();
}
    break;

  case 39:

/* Line 1806 of yacc.c  */
#line 615 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	currentTransform = Transform();
}
    break;

  case 40:

/* Line 1806 of yacc.c  */
#line 619 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 41:

/* Line 1806 of yacc.c  */
#line 622 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	const string name((yyvsp[(2) - (3)].string));
	if (namedLightGroups.count(name))
		currentGraphicsState.currentLightGroup = namedLightGroups[name];
	else {
		// It is a new light group
		currentGraphicsState.currentLightGroup = freeLightGroupIndex;
		namedLightGroups[name] = freeLightGroupIndex++;
	}

	FreeArgs();
}
    break;

  case 42:

/* Line 1806 of yacc.c  */
#line 635 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	Properties props;
	InitProperties(props, CPS, CP);

	const string name((yyvsp[(2) - (3)].string));
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
    break;

  case 43:

/* Line 1806 of yacc.c  */
#line 672 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	*sceneProps <<
			Property("scene.camera.lookat.orig")((yyvsp[(2) - (10)].num), (yyvsp[(3) - (10)].num), (yyvsp[(4) - (10)].num)) <<
			Property("scene.camera.lookat.target")((yyvsp[(5) - (10)].num), (yyvsp[(6) - (10)].num), (yyvsp[(7) - (10)].num)) <<
			Property("scene.camera.lookat.up")((yyvsp[(8) - (10)].num), (yyvsp[(9) - (10)].num), (yyvsp[(10) - (10)].num));
}
    break;

  case 44:

/* Line 1806 of yacc.c  */
#line 679 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	currentGraphicsState.materialName = (yyvsp[(2) - (3)].string);
	InitProperties(currentGraphicsState.materialProps, CPS, CP);

	FreeArgs();
}
    break;

  case 45:

/* Line 1806 of yacc.c  */
#line 686 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	const string name((yyvsp[(2) - (3)].string));
	if (namedMaterials.count(name))
		throw runtime_error("Named material '" + name + "' already defined");

	Properties props;
	InitProperties(props, CPS, CP);
	DefineMaterial(name, props);

	FreeArgs();
}
    break;

  case 46:

/* Line 1806 of yacc.c  */
#line 698 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	FreeArgs();
}
    break;

  case 47:

/* Line 1806 of yacc.c  */
#line 702 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	ArrayFree((yyvsp[(2) - (2)].ribarray));
}
    break;

  case 48:

/* Line 1806 of yacc.c  */
#line 706 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
}
    break;

  case 49:

/* Line 1806 of yacc.c  */
#line 709 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//Context::GetActive()->NamedMaterial($2);
}
    break;

  case 50:

/* Line 1806 of yacc.c  */
#line 713 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxObjectBegin($2);
}
    break;

  case 51:

/* Line 1806 of yacc.c  */
#line 717 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxObjectEnd();
}
    break;

  case 52:

/* Line 1806 of yacc.c  */
#line 721 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxObjectInstance($2);
}
    break;

  case 53:

/* Line 1806 of yacc.c  */
#line 725 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxPortalInstance($2);
}
    break;

  case 54:

/* Line 1806 of yacc.c  */
#line 729 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxMotionInstance($2, $3, $4, $5);
}
    break;

  case 55:

/* Line 1806 of yacc.c  */
#line 733 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->PixelFilter($2, params);
	FreeArgs();
}
    break;

  case 56:

/* Line 1806 of yacc.c  */
#line 740 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	Properties props;
	InitProperties(props, CPS, CP);

	// Check the name of the renderer
	if ((strcmp((yyvsp[(2) - (3)].string), "slg") == 0) || (strcmp((yyvsp[(2) - (3)].string), "slg") == 0)) {
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
    break;

  case 57:

/* Line 1806 of yacc.c  */
#line 764 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxReverseOrientation();
}
    break;

  case 58:

/* Line 1806 of yacc.c  */
#line 768 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxRotate($2, $3, $4, $5);
}
    break;

  case 59:

/* Line 1806 of yacc.c  */
#line 772 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Sampler($2, params);
	FreeArgs();
}
    break;

  case 60:

/* Line 1806 of yacc.c  */
#line 779 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxScale($2, $3, $4);
}
    break;

  case 61:

/* Line 1806 of yacc.c  */
#line 783 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//;//luxSearchPath($2);//FIXME - Unimplemented
}
    break;

  case 62:

/* Line 1806 of yacc.c  */
#line 787 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Shape($2, params);
	FreeArgs();
}
    break;

  case 63:

/* Line 1806 of yacc.c  */
#line 794 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->PortalShape($2, params);
	FreeArgs();
}
    break;

  case 64:

/* Line 1806 of yacc.c  */
#line 801 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->SurfaceIntegrator($2, params);
	FreeArgs();
}
    break;

  case 65:

/* Line 1806 of yacc.c  */
#line 808 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Texture($2, $3, $4, params);
	FreeArgs();
}
    break;

  case 66:

/* Line 1806 of yacc.c  */
#line 815 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxTransformBegin();
}
    break;

  case 67:

/* Line 1806 of yacc.c  */
#line 819 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxTransformEnd();
}
    break;

  case 68:

/* Line 1806 of yacc.c  */
#line 823 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//if (VerifyArrayLength($2, 16, "Transform"))
	//	luxTransform(static_cast<float *>($2->array));
	ArrayFree((yyvsp[(2) - (2)].ribarray));
}
    break;

  case 69:

/* Line 1806 of yacc.c  */
#line 829 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxTranslate($2, $3, $4);
}
    break;

  case 70:

/* Line 1806 of yacc.c  */
#line 833 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->VolumeIntegrator($2, params);
	FreeArgs();
}
    break;

  case 71:

/* Line 1806 of yacc.c  */
#line 840 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//ParamSet params;
	//InitParamSet(params, CPS, CP);
	//Context::GetActive()->Volume($2, params);
	FreeArgs();
}
    break;

  case 72:

/* Line 1806 of yacc.c  */
#line 847 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxWorldBegin();
}
    break;

  case 73:

/* Line 1806 of yacc.c  */
#line 851 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"
    {
	//luxWorldEnd();
}
    break;



/* Line 1806 of yacc.c  */
#line 2689 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.cpp"
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

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
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

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

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

  *++yyvsp = yylval;


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

#if !defined(yyoverflow) || YYERROR_VERBOSE
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
  /* Do not reclaim the symbols of the rule which action triggered
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
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



/* Line 2067 of yacc.c  */
#line 854 "/home/david/projects/luxrender-dev/luxrays/src/luxcore/luxparser/luxparse.y"


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

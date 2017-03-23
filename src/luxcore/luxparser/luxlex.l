/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

%option prefix="luxcore_parserlxs_yy"

%{

#define YY_MAIN 0
#define YY_NEVER_INTERACTIVE 1

#if defined(WIN32) && !defined(__CYGWIN__)
#pragma warning ( disable: 4244 )
#endif

#include <sstream>

#include <boost/foreach.hpp>
	
#include "luxcore/luxcore.h"
#include "luxrays/utils/utils.h"

#ifdef WIN32
#define YY_NO_UNISTD_H
#include <io.h>
#define isatty _isatty
#endif

struct ParamArray; // This is used but not defined inside luxparse.hpp
// luxparse.hpp is under "src" directory and not "include" because otherwise
// the parser sources are always recompiled (it is a bug in BISON_TARGET() cmake macro)
#include "luxparse.hpp"

using namespace std;
using namespace luxcore;

namespace luxcore { namespace parselxs {
	
struct IncludeInfo {
	string filename;
	YY_BUFFER_STATE bufState;
	u_int lineNum;
};
vector<IncludeInfo> includeStack;

extern u_int lineNum;
extern string currentFile;
int str_pos;

void AddStringChar( char c )
{
	if (str_pos == 1023) {
		LC_LOG("String is too long.");
		++str_pos;
	} else if (str_pos < 1023) {
		luxcore_parserlxs_yylval.string[str_pos++] = c;
		luxcore_parserlxs_yylval.string[str_pos] = '\0';
	}
}

extern void yyerror( const char *str );

void IncludePush(char *filename) {
	if (includeStack.size() > 32) {
		LC_LOG("Only 32 levels of nested Include allowed in scene files.");
    } else {
        FILE *newFile = fopen(filename, "r");
        if (!newFile) {
            LC_LOG("Unable to open included scene file: " << filename);
        } else {
            IncludeInfo ii;
            ii.filename = currentFile;
            ii.bufState = YY_CURRENT_BUFFER;
            ii.lineNum = lineNum;
            includeStack.push_back(ii);

            currentFile = filename;
            lineNum = 1;

        	yyin = newFile;
        	yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));
        }
    }
}

void IncludePop(void)
{
	// Close include file
	fclose(yyin);
	yy_delete_buffer(YY_CURRENT_BUFFER);
	yy_switch_to_buffer(includeStack.back().bufState);
	currentFile = includeStack.back().filename;
	lineNum = includeStack.back().lineNum;
	includeStack.pop_back();
}

void IncludeClear()
{
	while (!includeStack.empty()) {
		yy_delete_buffer(includeStack.back().bufState);
		includeStack.pop_back();		
	}
}

} }

using namespace luxcore::parselxs;

%}

%option nounput
WHITESPACE [ \t\r]+
NUMBER [-+]?([0-9]+|(([0-9]+\.[0-9]*)|(\.[0-9]+)))([eE][-+]?[0-9]+)?
IDENT [a-zA-Z_][a-zA-Z_0-9]*
%x STR COMMENT INCL INCL_FILE
%%

Include { BEGIN(INCL); }
<INCL>{WHITESPACE} ;
<INCL>\" { BEGIN(INCL_FILE); }
<INCL>. { LC_LOG("Illegal character following Include directive"); }
<INCL_FILE>\" { BEGIN INITIAL; }
<INCL_FILE>. {  LC_LOG("Illegal character in Include file name"); }
<INCL_FILE>[^\n\"]+ {
	BEGIN(INITIAL);
	luxcore::parselxs::IncludePush(yytext);
}
"#" { BEGIN COMMENT; }
<COMMENT>. /* eat it up */
<COMMENT>\n { lineNum++; BEGIN INITIAL; }
Accelerator		{ return ACCELERATOR; }
AreaLightSource		{ return AREALIGHTSOURCE;	}
AttributeBegin 		{ return ATTRIBUTEBEGIN; 	}
AttributeEnd 		{ return ATTRIBUTEEND; 		}
Camera			{ return CAMERA;		}
ConcatTransform 	{ return CONCATTRANSFORM; 	}
CoordinateSystem	{ return COORDINATESYSTEM;	}
CoordSysTransform	{ return COORDSYSTRANSFORM;	}
Exterior		{ return EXTERIOR;		}
Film	 		{ return FILM;	 		}
Identity		{ return IDENTITY;		}
Interior		{ return INTERIOR;		}
LightGroup		{ return LIGHTGROUP;		}
LightSource 		{ return LIGHTSOURCE;		}
LookAt			{ return LOOKAT;		}
Material		{ return MATERIAL;		}
MakeNamedMaterial	{ return MAKENAMEDMATERIAL;	}
MakeNamedVolume		{ return MAKENAMEDVOLUME;	}
MotionBegin			{ return MOTIONBEGIN;       }
MotionEnd			{ return MOTIONEND;         }
NamedMaterial		{ return NAMEDMATERIAL;		}
ObjectBegin 		{ return OBJECTBEGIN;		}
ObjectEnd 		{ return OBJECTEND;		}
ObjectInstance 		{ return OBJECTINSTANCE;	}
PortalInstance		{ return PORTALINSTANCE;	}
MotionInstance 		{ return MOTIONINSTANCE;	}
PixelFilter 		{ return PIXELFILTER; 		}
Renderer		{ return RENDERER;	}
ReverseOrientation 	{ return REVERSEORIENTATION;	}
Rotate			{ return ROTATE;		}
Sampler			{ return SAMPLER;		}
SearchPath		{ return SEARCHPATH;		}
Scale 			{ return SCALE;			}
PortalShape		{ return PORTALSHAPE;           }
Shape			{ return SHAPE;			}
SurfaceIntegrator	{ return SURFACEINTEGRATOR;	}
Texture			{ return TEXTURE;		}
TransformBegin	{ return TRANSFORMBEGIN;	}
TransformEnd	{ return TRANSFORMEND;	}
Transform		{ return TRANSFORM;		}
Translate		{ return TRANSLATE; 		}
Volume			{ return VOLUME;		}
VolumeIntegrator	{ return VOLUMEINTEGRATOR;	}
WorldBegin 		{ return WORLDBEGIN; 		}
WorldEnd 		{ return WORLDEND;		}
{WHITESPACE} /* do nothing */
\n { lineNum++; }
{NUMBER} {
  luxcore_parserlxs_yylval.num = static_cast<float>(atof(yytext));
  return NUM;
}
{IDENT} {
	strcpy(luxcore_parserlxs_yylval.string, yytext);
	return ID;
}
"[" { return LBRACK; }
"]" { return RBRACK; }
\" { BEGIN STR; str_pos = 0; luxcore_parserlxs_yylval.string[0] = '\0';
	/* I have to initialize the string as an empty one in order to handle empty strings (i.e. "") */ }
<STR>\\n {AddStringChar('\n');}
<STR>\\t {AddStringChar('\t');}
<STR>\\r {AddStringChar('\r');}
<STR>\\b {AddStringChar('\b');}
<STR>\\f {AddStringChar('\f');}
<STR>\\\" {AddStringChar('\"');}
<STR>\\\\ {AddStringChar('\\');}
<STR>\\[0-9]{3} {
  char val = static_cast<char>(atoi(yytext+1) & 0xff);
  AddStringChar(val);
}
<STR>\\\n {lineNum++;}
<STR>\\. { AddStringChar(yytext[1]);}
<STR>\" {BEGIN INITIAL; return STRING;}
<STR>. {AddStringChar(yytext[0]);}
<STR>\n { LC_LOG("Unterminated string !");}

. { LC_LOG("Illegal character " << (currentFile != "" ? "in file '" + std::string(currentFile) + "' " : "") << "at line " << lineNum << ": "<<yytext[0]); }
%%
int yywrap(void)
{
	if (includeStack.size() ==0) return 1;
	IncludePop();
	BEGIN(INCL_FILE);
	return 0;
}

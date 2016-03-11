/***************************************************************************
 *   Copyright (C) 1998-2013,2016 by authors (see AUTHORS.txt)                  *
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

#ifndef LUXCOREPLACEHOLDER_H
#define LUXCOREPLACEHOLDER_H 1

#define LUX_DEBUG			-1		/* Debugging output */

#define LUX_INFO            0       /* Rendering stats & other info */
#define LUX_WARNING         1       /* Something seems wrong, maybe okay */
#define LUX_ERROR           2       /* Problem.  Results may be wrong */
#define LUX_SEVERE          3       /* So bad you should probably abort */

#define LUX_NOERROR         0

#define LUX_NOMEM           1       /* Out of memory */
#define LUX_SYSTEM          2       /* Miscellaneous system error */
#define LUX_NOFILE          3       /* File nonexistant */
#define LUX_BADFILE         4       /* Bad file format */
#define LUX_BADVERSION      5       /* File version mismatch */
#define LUX_DISKFULL        6       /* Target disk is full */

#define LUX_UNIMPLEMENT    12       /* Unimplemented feature */
#define LUX_LIMIT          13       /* Arbitrary program limit */
#define LUX_BUG            14       /* Probably a bug in renderer */

#define LUX_NOTSTARTED     23       /* luxInit() not called */
#define LUX_NESTING        24       /* Bad begin-end nesting - jromang will be used in API v2 */
#define LUX_NOTOPTIONS     25       /* Invalid state for options - jromang will be used in API v2 */
#define LUX_NOTATTRIBS     26       /* Invalid state for attributes - jromang will be used in API v2 */
#define LUX_NOTPRIMS       27       /* Invalid state for primitives - jromang will be used in API v2 */
#define LUX_ILLSTATE       28       /* Other invalid state - jromang will be used in API v2 */
#define LUX_BADMOTION      29       /* Badly formed motion block - jromang will be used in API v2 */
#define LUX_BADSOLID       30       /* Badly formed solid block - jromang will be used in API v2 */

#define LUX_BADTOKEN       41       /* Invalid token for request */
#define LUX_RANGE          42       /* Parameter out of range */
#define LUX_CONSISTENCY    43       /* Parameters inconsistent */
#define LUX_BADHANDLE      44       /* Bad object/light handle */
#define LUX_NOPLUGIN       45       /* Can't load requested plugin */
#define LUX_MISSINGDATA    46       /* Required parameters not provided */
#define LUX_SYNTAX         47       /* Declare type syntax error */

#define LUX_MATH           61       /* Zerodivide, noninvert matrix, etc. */

//! Macros that convert a number in input into a string. 
#define XVERSION_STR(v) #v
#define VERSION_STR(v) XVERSION_STR(v)

#define LUX_VN_MAJOR 1
#define LUX_VN_MINOR 6
#define LUX_VN_PATCH 0
#define LUX_VN_BUILD 0
#define LUX_VN_LABEL "RC1"

#define LUX_SERVER_PROTOCOL_VERSION  1011

#define LUX_VERSION_STRING           VERSION_STR(LUX_VN_MAJOR)     \
                                     "." VERSION_STR(LUX_VN_MINOR) \
                                     "." VERSION_STR(LUX_VN_PATCH) \
                                     " " LUX_VN_LABEL \
                                     " Build " VERSION_STR(LUX_VN_BUILD)

//! Renderfarms rely on the 'protocol' part of in server version string
#define LUX_SERVER_VERSION_STRING    VERSION_STR(LUX_VN_MAJOR)     \
                                     "." VERSION_STR(LUX_VN_MINOR) \
                                     "." VERSION_STR(LUX_VN_PATCH) \
                                     " " LUX_VN_LABEL \
									 " (protocol: " VERSION_STR(LUX_SERVER_PROTOCOL_VERSION) ")"


#define    LUX_HISTOGRAM_RGB    	1
#define    LUX_HISTOGRAM_RGB_ADD	2
#define    LUX_HISTOGRAM_RED    	4
#define    LUX_HISTOGRAM_GREEN  	8
#define    LUX_HISTOGRAM_BLUE   	16
#define    LUX_HISTOGRAM_VALUE  	32
#define    LUX_HISTOGRAM_LOG    	64

#ifndef Q_MOC_RUN
#include <boost/program_options.hpp>
#include <boost/filesystem/operations.hpp>
#endif

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/luxrays/core/color/color.h"

enum luxComponent {LUX_FILM};

enum luxComponentParameters {	LUX_FILM_TM_TONEMAPKERNEL,
				LUX_FILM_TM_REINHARD_PRESCALE,
				LUX_FILM_TM_REINHARD_POSTSCALE,
				LUX_FILM_TM_REINHARD_BURN,
				LUX_FILM_TM_LINEAR_SENSITIVITY,
				LUX_FILM_TM_LINEAR_EXPOSURE,
				LUX_FILM_TM_LINEAR_FSTOP,
				LUX_FILM_TM_LINEAR_GAMMA,
				LUX_FILM_TM_CONTRAST_YWA,
				LUX_FILM_TORGB_X_WHITE,
				LUX_FILM_TORGB_Y_WHITE,
				LUX_FILM_TORGB_X_RED,
				LUX_FILM_TORGB_Y_RED,
				LUX_FILM_TORGB_X_GREEN,
				LUX_FILM_TORGB_Y_GREEN,
				LUX_FILM_TORGB_X_BLUE,
				LUX_FILM_TORGB_Y_BLUE,
				LUX_FILM_TORGB_GAMMA,
				LUX_FILM_UPDATEBLOOMLAYER,
				LUX_FILM_DELETEBLOOMLAYER,
				LUX_FILM_BLOOMRADIUS,
				LUX_FILM_BLOOMWEIGHT,
				LUX_FILM_VIGNETTING_ENABLED,
				LUX_FILM_VIGNETTING_SCALE,
				LUX_FILM_ABERRATION_ENABLED,
				LUX_FILM_ABERRATION_AMOUNT,
				LUX_FILM_UPDATEGLARELAYER,
				LUX_FILM_DELETEGLARELAYER,
				LUX_FILM_GLARE_AMOUNT,
				LUX_FILM_GLARE_RADIUS,
				LUX_FILM_GLARE_BLADES,
				LUX_FILM_HISTOGRAM_ENABLED,
				LUX_FILM_NOISE_CHIU_ENABLED,
				LUX_FILM_NOISE_CHIU_RADIUS,
				LUX_FILM_NOISE_CHIU_INCLUDECENTER,
				LUX_FILM_NOISE_GREYC_ENABLED,
				LUX_FILM_NOISE_GREYC_AMPLITUDE,
				LUX_FILM_NOISE_GREYC_NBITER,
				LUX_FILM_NOISE_GREYC_SHARPNESS,
				LUX_FILM_NOISE_GREYC_ANISOTROPY,
				LUX_FILM_NOISE_GREYC_ALPHA,
				LUX_FILM_NOISE_GREYC_SIGMA,
				LUX_FILM_NOISE_GREYC_FASTAPPROX,
				LUX_FILM_NOISE_GREYC_GAUSSPREC,
				LUX_FILM_NOISE_GREYC_DL,
				LUX_FILM_NOISE_GREYC_DA,
				LUX_FILM_NOISE_GREYC_INTERP,
				LUX_FILM_NOISE_GREYC_TILE,
				LUX_FILM_NOISE_GREYC_BTILE,
				LUX_FILM_NOISE_GREYC_THREADS,
				LUX_FILM_LG_COUNT,
				LUX_FILM_LG_ENABLE,
				LUX_FILM_LG_NAME,
				LUX_FILM_LG_SCALE,
				LUX_FILM_LG_SCALE_RED,
				LUX_FILM_LG_SCALE_BLUE,
				LUX_FILM_LG_SCALE_GREEN,
				LUX_FILM_LG_TEMPERATURE,
				LUX_FILM_LG_SCALE_X,
				LUX_FILM_LG_SCALE_Y,
				LUX_FILM_LG_SCALE_Z,
				LUX_FILM_GLARE_THRESHOLD,
				LUX_FILM_CAMERA_RESPONSE_ENABLED,
				LUX_FILM_CAMERA_RESPONSE_FILE,
				LUX_FILM_LDR_CLAMP_METHOD,
				LUX_FILM_TM_FALSE_METHOD,
				LUX_FILM_TM_FALSE_COLORSCALE,
				LUX_FILM_TM_FALSE_MAX,
				LUX_FILM_TM_FALSE_MIN,
				LUX_FILM_TM_FALSE_MAXSAT,
				LUX_FILM_TM_FALSE_MINSAT,
				LUX_FILM_TM_FALSE_AVGLUM,
				LUX_FILM_TM_FALSE_AVGEMI,
				LUX_FILM_GLARE_MAP,
				LUX_FILM_GLARE_PUPIL,
				LUX_FILM_GLARE_LASHES
};

void luxSetParameterValue(luxComponent comp,luxComponentParameters param, double value, unsigned int index = 0);

void luxSetStringAttribute(const char * objectName, const char * attributeName, const char * value);

void luxSetStringParameterValue(luxComponent comp, luxComponentParameters param, const char* value, unsigned int index = 0);

unsigned int luxGetStringParameterValue(luxComponent comp, luxComponentParameters param, char* dst, unsigned int dstlen, unsigned int index = 0);

int luxGetIntAttribute(const char * objectName, const char * attributeName); /* Returns the value of an int attribute */

void luxInit();

void luxGetHistogramImage(unsigned char *outPixels, unsigned int width, unsigned int height, int options);

struct clConfig
{
	clConfig() :
		slave(false), binDump(false), log2console(false), writeFlmFile(false),
		verbosity(0), pollInterval(luxGetIntAttribute("render_farm", "pollingInterval")),
		tcpPort(luxGetIntAttribute("render_farm", "defaultTcpPort")), threadCount(0) {};

	boost::program_options::variables_map vm;

	bool slave;
	bool binDump;
	bool log2console;
	bool writeFlmFile;
	bool fixedSeed;
	int verbosity;
	unsigned int pollInterval;
	unsigned int tcpPort;
	unsigned int threadCount;
	std::string password;
	std::string cacheDir;
	std::vector< std::string > queueFiles;
	std::vector< std::string > inputFiles;
	std::vector< std::string > slaveNodeList;
};

namespace featureSet {
	enum {
		RENDERER     = (1u << 0),
		MASTERNODE   = (1u << 1),
		SLAVENODE    = (1u << 2),
		INTERACTIVE  = (1u << 3)
	};
}

struct RenderingServerInfo {
	int serverIndex;

	// Dade - connection information
	const char *name; // Dade - name/ip address of the server
	const char *port; // Dade - tcp port of the server
	const char *sid; // Dade - session id for the server

	double numberOfSamplesReceived;
	double calculatedSamplesPerSecond;
	unsigned int secsSinceLastContact;
	unsigned int secsSinceLastSamples;
};

//bool ProcessCommandLine(int argc, char** argv, clConfig& config, unsigned int features, std::streambuf* infoBuf = std::cout.rdbuf(), std::streambuf* warnBuf = std::cerr.rdbuf());

double luxStatistics(const char *statName);

double luxGetDefaultParameterValue(luxComponent comp, luxComponentParameters param, unsigned int index = 0);

double luxGetParameterValue(luxComponent comp, luxComponentParameters param, unsigned int index = 0);

unsigned int luxGetDefaultStringParameterValue(luxComponent comp, luxComponentParameters param, char* dst, unsigned int dstlen, unsigned int index = 0);

typedef void (*LuxErrorHandler)(int code, int severity, const char *msg);

void luxErrorHandler(LuxErrorHandler handler);

extern LuxErrorHandler luxError;

void luxSetIntAttribute(const char * objectName, const char * attributeName, int value); /* Sets an int attribute value */

int luxGetIntAttributeDefault(const char * objectName, const char * attributeName); /* Returns the default value of an int attribute */

void luxErrorPrint(int code, int severity, const char *message);

void luxSetHaltSamplesPerPixel(int haltspp, bool haveEnoughSamplesPerPixel, bool suspendThreadsWhenDone);

void luxStart();

void luxPause();

void luxExit();

void luxAbort();

void luxWait();

int luxSaveEXR(const char* name, bool useHalfFloat, bool includeZBuffer, int compressionType, bool tonemapped);

void luxUpdateFramebuffer();

void luxDisableRandomMode();

void luxCleanup();

int luxParse(const char *filename);

void luxLoadFLM(const char* name);

void luxSaveFLM(const char* name);

void luxAddServer(const char * name);

void luxRemoveServer(const char * name);

void luxOverrideResumeFLM(const char *name);

void luxUpdateLogFromNetwork();

unsigned int luxAddThread();

void luxRemoveThread();

double luxGetDoubleAttribute(const char * objectName, const char * attributeName); /* Returns the value of a double attribute */

double luxGetDoubleAttributeDefault(const char * objectName, const char * attributeName); 

double luxMagnitudeReduce(double number);

const char* luxMagnitudePrefix(double number);

void luxUpdateStatisticsWindow();

bool luxHasObject(const char * objectName);

bool luxGetBoolAttributeDefault(const char * objectName, const char * attributeName);

void luxSetBoolAttribute(const char * objectName, const char * attributeName, bool value);

void luxSetFloatAttribute(const char * objectName, const char * attributeName, float value);

unsigned int luxGetRenderingServersStatus(RenderingServerInfo *info, unsigned int maxInfoCount);

void luxResetServer(const char * name, const char * password);

float luxGetFloatAttribute(const char * objectName, const char * attributeName); 

float luxGetFloatAttributeDefault(const char * objectName, const char * attributeName);

unsigned int luxGetStringAttribute(const char * objectName, const char * attributeName, char * dst, unsigned int dstlen);

unsigned int luxGetStringAttributeDefault(const char * objectName, const char * attributeName, char * dst, unsigned int dstlen);

unsigned int luxGetAttributeDescription(const char * objectName, const char * attributeName, char * dst, unsigned int dstlen);

const char* luxVersion();

float* luxAlphaBuffer();

bool luxGetBoolAttribute(const char * objectName, const char * attributeName);

bool luxGetBoolAttributeDefault(const char * objectName, const char * attributeName);

unsigned char* luxFramebuffer();

float *luxGetUserSamplingMap();

void luxSetUserSamplingMap(const float *map);

bool ProcessCommandLine(int argc, char** argv, clConfig& config, unsigned int features, std::streambuf* infoBuf = std::cout.rdbuf(), std::streambuf* warnBuf = std::cerr.rdbuf());

namespace lux
{
	extern int luxLogFilter;
	//Logging class to use when displaying infos and error, syntax :
	//LOG(LUX_SEVERE,LUX_NOMEM)<<"one "<<23;

	enum FalseColorScale {	Scale_STD=0, Scale_LMK, Scale_RED, Scale_WHITE, Scale_YELLOW, Scale_SPEOS };


	class Log {
		public:
		
		~Log();

		inline std::ostringstream& get(int _severity, int _code) {
			severity =_severity;
			code=_code;
			return os;
		}

		private:
		   int severity, code;
		   std::ostringstream os;
	};



	struct nullstream: std::ostream {
		
		nullstream(): std::ios(0), std::ostream(0) {}	

	};

	enum FalseScaleMethod { Method_Linear = 0, Method_Log, Method_Log3 };

	float InverseValueScale(FalseScaleMethod scalemethod, float value);
	
	luxrays::RGBColor ValuetoRGB(FalseColorScale colorscale, float value);

	extern nullstream nullStream;

}

//LOG macro. The filtering test uses the ?: operator instead of if{}else{}
//to avoid ambiguity when invoked in if{}/else{} constructs
#define LOG(severity,code) (severity<lux::luxLogFilter)?(lux::nullStream):lux::Log().get(severity, code)


#endif

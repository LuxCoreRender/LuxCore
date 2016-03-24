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

#include "luxcorePlaceholder.h"

void luxSetParameterValue(luxComponent comp,luxComponentParameters param, double value, unsigned int index) {
	return;
}

void luxSetStringAttribute(const char * objectName, const char * attributeName, const char * value) {
	return;
}

void luxSetStringParameterValue(luxComponent comp, luxComponentParameters param, const char* value, unsigned int index) {
	return;
}

unsigned int luxGetStringParameterValue(luxComponent comp,luxComponentParameters param, char* dst, unsigned int dstlen,	unsigned int index) {
	return 0;
}

int luxGetIntAttribute(const char * objectName, const char * attributeName) {
	return 0;
}

void luxInit() {
	return;
}

//bool ProcessCommandLine(int argc, char** argv, clConfig& config, unsigned int features, std::streambuf* infoBuf, std::streambuf* warnBuf) {
//	return true;
//}

double luxStatistics(const char *statName){
	return 0.;
}

double luxGetDefaultParameterValue(luxComponent comp,luxComponentParameters param, unsigned int index) {
	return 0;
}

double luxGetParameterValue(luxComponent comp, luxComponentParameters param, unsigned int index) {
	return 0;
}

unsigned int luxGetDefaultStringParameterValue(luxComponent comp, luxComponentParameters param, char* dst, unsigned int dstlen, unsigned int index) {
	return 0;
}

void luxErrorHandler(LuxErrorHandler handler){
	return;
}

void luxSetIntAttribute(const char * objectName, const char * attributeName, int value) {
	return;
}

int luxGetIntAttributeDefault(const char * objectName, const char * attributeName) {
	return 0;
}

void luxErrorPrint(int code, int severity, const char *message) {
	return;
}

void luxSetHaltSamplesPerPixel(int haltspp, bool haveEnoughSamplesPerPixel, bool suspendThreadsWhenDone) {
	return;
}

void luxStart() {
	return;
}

void luxPause() {
	return;
}

void luxExit() {
	return;
}

void luxAbort() {
	return;
}

void luxWait() {
	return;
}

int luxSaveEXR(const char* name, bool useHalfFloat, bool includeZBuffer, int compressionType, bool tonemapped) {
	return 0;
}

void luxUpdateFramebuffer() {
	return;
}

void luxDisableRandomMode() {
	return;
}

void luxCleanup() {
	return;
}

int luxParse(const char *filename) {
	return 0;
}

void luxLoadFLM(const char* name) {
	return;
}

void luxSaveFLM(const char* name) {
	return;
}

void luxAddServer(const char * name) {
	return;
}

void luxRemoveServer(const char * name) {
	return;
}

void luxOverrideResumeFLM(const char *name) {
	return;
}

void luxUpdateLogFromNetwork() {
	return;
}

unsigned int luxAddThread() {
	return 0;
}

void luxRemoveThread() {
	return;
}

double luxGetDoubleAttribute(const char * objectName, const char * attributeName) {
	return 0.0;
}

double luxGetDoubleAttributeDefault(const char * objectName, const char * attributeName) {
	return 0.0;
}

double luxMagnitudeReduce(double number) {
	return 0.0;
}

const char* luxMagnitudePrefix(double number) {
	return NULL;
}

void luxUpdateStatisticsWindow() {
	return;
}

bool luxHasObject(const char * objectName) {
	return true;
}

bool luxGetBoolAttributeDefault(const char * objectName, const char * attributeName) {
	return true;
}

void luxSetBoolAttribute(const char * objectName, const char * attributeName, bool value) {
	return;
}

void luxSetFloatAttribute(const char * objectName, const char * attributeName, float value) {
	return;
}

unsigned int luxGetRenderingServersStatus(RenderingServerInfo *info, unsigned int maxInfoCount) {
	return 0;
}

void luxResetServer(const char * name, const char * password) {
	return;
}

float luxGetFloatAttribute(const char * objectName, const char * attributeName) {
	return 0.0;
}

float luxGetFloatAttributeDefault(const char * objectName, const char * attributeName) {
	return 0.0;
}

unsigned int luxGetStringAttribute(const char * objectName, const char * attributeName, char * dst, unsigned int dstlen) {
	return 0;
}

unsigned int luxGetStringAttributeDefault(const char * objectName, const char * attributeName, char * dst, unsigned int dstlen) {
	return 0;
}

unsigned int luxGetAttributeDescription(const char * objectName, const char * attributeName, char * dst, unsigned int dstlen) {
	return 0;
}

const char* luxVersion() {
	return "luxDev";
}

float* luxAlphaBuffer() {
	return NULL;
}

bool luxGetBoolAttribute(const char * objectName, const char * attributeName) {
	return true;
}

unsigned char* luxFramebuffer() {
	return NULL;
}

float *luxGetUserSamplingMap() {
	return NULL;
}

void luxSetUserSamplingMap(const float *map) {
	return;
}

void luxGetHistogramImage(unsigned char *outPixels, unsigned int width, unsigned int height, int options) {
	return;
}

bool ProcessCommandLine(int argc, char** argv, clConfig& config, unsigned int features, std::streambuf* infoBuf, std::streambuf* warnBuf) {
	return true;
}

namespace lux {
	
	nullstream nullStream;

	int luxLogFilter;

	LuxErrorHandler luxError;
	
	Log::~Log() { /*luxError(code, severity, os.str().c_str());*/ }

	float InverseValueScale(FalseScaleMethod scalemethod, float value) {
	
		float result = 0;

		return result;
	}

	luxrays::RGBColor ValuetoRGB(FalseColorScale colorscale, float value)
	{
		luxrays::RGBColor color;

		return color;
	}


}


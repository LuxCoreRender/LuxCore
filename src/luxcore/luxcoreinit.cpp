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

#include <iostream>
#include <boost/format.hpp>
#include <boost/thread/mutex.hpp>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/null_sink.h"

#include "luxrays/luxrays.h"
#include "slg/slg.h"
#include "slg/core/sdl.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcorecallbacksink.h"
#include "luxcore/luxcorelogger.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// Initialization and logging
//------------------------------------------------------------------------------

// Call back sink parameters
static void (*loggerCallBackHandler)(const char *msg) = nullptr;

// File sink parameters
static string loggerFileName = "";
static unsigned int loggerFileMaxSize = 0;
static unsigned int loggerFileMaxCount = 0;

// Time of luxcore::Init() call
static double lcInitTime;

// LuxCore logger
shared_ptr<spdlog::logger> luxcoreLogger;

static void DefaultDebugHandler(const char *msg) {
	cerr << msg << endl;
}

static void LuxRaysDebugHandler(const char *msg) {
	luxcoreLogger->info((boost::format("[LuxRays][%.3f] %s") % (WallClockTime() - lcInitTime) % msg).str());
}

static void SDLDebugHandler(const char *msg) {
	luxcoreLogger->info((boost::format("[SDL][%.3f] %s") % (WallClockTime() - lcInitTime) % msg).str());
}

static void SLGDebugHandler(const char *msg) {
	luxcoreLogger->info((boost::format("[LuxCore][%.3f] %s") % (WallClockTime() - lcInitTime) % msg).str());
}

static void UpdateLuxCoreLogger() {
	vector<spdlog::sink_ptr> sinks;

	if (loggerCallBackHandler)
		sinks.push_back(std::make_shared<spdlog::sinks::luxcore_callback_sink_mt>(loggerCallBackHandler));
	
	if ((loggerFileName != "") && (loggerFileMaxSize > 0) && (loggerFileMaxCount > 0))
		sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(loggerFileName, loggerFileMaxSize, loggerFileMaxCount));

	if (sinks.size() == 0)
		sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());

	luxcoreLogger = std::make_shared<spdlog::logger>("LuxCoreLogger", begin(sinks), end(sinks));
	luxcoreLogger->set_pattern("%v");
}

void luxcore::Init(void (*LogHandler)(const char *)) {
	// To be thread safe
	static boost::mutex initMutex;
	boost::unique_lock<boost::mutex> lock(initMutex);

	slg::Init();

	lcInitTime = WallClockTime();
	
	slg::LuxRays_DebugHandler = ::LuxRaysDebugHandler;
	slg::SLG_DebugHandler = ::SLGDebugHandler;
	slg::SLG_SDLDebugHandler = ::SDLDebugHandler;	

	if (LogHandler)
		SetLogHandler(LogHandler);
	else
		SetLogHandler(DefaultDebugHandler);
}

void luxcore::SetLogHandler(void (*LogHandler)(const char *)) {
	// User provided handler
	loggerCallBackHandler = LogHandler;
	
	UpdateLuxCoreLogger();
}

void luxcore::SetFileLog(const string &fileName, const unsigned int size,
		const unsigned int count) {
	loggerFileName = fileName;
	loggerFileMaxSize = size;
	loggerFileMaxCount = count;

	UpdateLuxCoreLogger();
}

void luxcore::PrintLogMsg(const std::string &msg) {
	luxcoreLogger->info(msg);
}

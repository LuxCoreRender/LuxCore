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

#ifndef _LUXCOREAPP_H
#define	_LUXCOREAPP_H

#include <GLFW/glfw3.h>
#include <imgui.h>

#include "luxcore/luxcore.h"
#include "logwindow.h"
#include "statswindow.h"
#include "pixelfilterwindow.h"
#include "renderenginewindow.h"
#include "samplerwindow.h"
#include "helpwindow.h"
#include "ocldevicewindow.h"
#include "epsilonwindow.h"
#include "lightstrategywindow.h"
#include "acceleratorwindow.h"
#include "filmoutputswindow.h"
#include "filmchannelswindow.h"
#include "filmradiancegroupswindow.h"

#define LA_ARRAYSIZE(_ARR)	((int)(sizeof(_ARR) / sizeof(*_ARR)))

class LuxCoreApp {
public:
	LuxCoreApp(luxcore::RenderConfig *renderConfig,
			luxcore::RenderState *startState, luxcore::Film *startFilm);
	~LuxCoreApp();

	void RunApp();

	static void LogHandler(const char *msg);
	static void ColoredLabelText(const ImVec4 &col, const char *label, const char *fmt, ...) IM_PRINTFARGS(3);
	static void ColoredLabelText(const char *label, const char *fmt, ...) IM_PRINTFARGS(2);
	static void HelpMarker(const char *desc);
	
	static ImVec4 colLabel;

	// Mouse "grab" mode. This is the natural way cameras are usually manipulated
	// The flag is off by default but can be turned on by using the -m switch
	bool optMouseGrabMode;
	bool optFullScreen;

	friend class AcceleratorWindow;
	friend class EpsilonWindow;
	friend class FilmChannelWindow;
	friend class FilmChannelsWindow;
	friend class FilmOutputWindow;
	friend class FilmOutputsWindow;
	friend class FilmRadianceGroupsWindow;
	friend class ImageWindow;
	friend class LightStrategyWindow;
	friend class OCLDeviceWindow;
	friend class PixelFilterWindow;
	friend class RenderEngineWindow;
	friend class SamplerWindow;
	friend class StatsWindow;

private:
	typedef enum {
		TOOL_CAMERA_EDIT,
		TOOL_OBJECT_SELECTION,
		TOOL_IMAGE_VIEW
	} AppToolType;

	static void ToolCameraEditKeys(GLFWwindow *window, int key, int scanCode, int action, int mods);

	static void GLFW_KeyCallBack(GLFWwindow *window, int key, int scanCode, int action, int mods);
	static void GLFW_MouseButtonCallBack(GLFWwindow *window, int button, int action, int mods);
	static void GLFW_MousePositionCallBack(GLFWwindow *window, double x, double y);

	void DrawBackgroundLogo();
	void UpdateMoveStep();
	void SetRenderingEngineType(const std::string &engineType);
	void RenderConfigParse(const luxrays::Properties &samplerProps);
	void RenderSessionParse(const luxrays::Properties &samplerProps);
	void AdjustFilmResolutionToWindowSize(u_int *filmWidth, u_int *filmHeight);
	void SetFilmResolution(const u_int filmWidth, const u_int filmHeight);
	void IncScreenRefreshInterval();
	void DecScreenRefreshInterval();
	void CloseAllRenderConfigEditors();

	void LoadRenderConfig(const std::string &configFileName,
			luxcore::RenderState *startState = NULL, luxcore::Film *startFilm = NULL);
	void StartRendering(luxcore::RenderState *startState = NULL, luxcore::Film *startFilm = NULL);
	void DeleteRendering();

	void RefreshRenderingTexture();
	void DrawRendering();
	void DrawTiles(const luxrays::Property &propCoords,
		const luxrays::Property &propPasses, const luxrays::Property &propErrors,
		const u_int tileCount, const u_int tileWidth, const u_int tileHeight,
		const ImU32 col);
	void DrawTiles();
	void DrawCaptions();

	void MenuRendering();
	void MenuEngine();
	void MenuSampler();
	void MenuTiles();
	void MenuFilm();
	void MenuImagePipeline();
	void MenuScreen();
	void MenuTool();
	void MenuWindow();
	void MainMenuBar();

	static LogWindow *currentLogWindow;

	AcceleratorWindow acceleratorWindow;
	EpsilonWindow epsilonWindow;
	FilmChannelsWindow filmChannelsWindow;
	FilmOutputsWindow filmOutputsWindow;
	FilmRadianceGroupsWindow filmRadianceGroupsWindow;
	LightStrategyWindow lightStrategyWindow;
	OCLDeviceWindow oclDeviceWindow;
	PixelFilterWindow pixelFilterWindow;
	RenderEngineWindow renderEngineWindow;
	SamplerWindow samplerWindow;
	StatsWindow statsWindow;
	LogWindow logWindow;
	HelpWindow helpWindow;

	luxcore::RenderConfig *config;
	luxcore::RenderState *startState;
	luxcore::Film *startFilm;

	luxcore::RenderSession *session;

	GLuint renderFrameBufferTexID;
	GLenum renderFrameBufferTexMinFilter, renderFrameBufferTexMagFilter;
	GLuint backgroundLogoTexID;

	u_int selectionFilmWidth, selectionFilmHeight;
	float *selectionBuffer;
	
	GLFWwindow *window;

	AppToolType currentTool;

	// ImGui height information
	int menuBarHeight, captionHeight;
	bool mouseHoverRenderingWindow;

	// ImGui inputs
	int menuFilmWidth, menuFilmHeight;
	
	int targetFilmWidth, targetFilmHeight;

	bool optRealTimeMode;
	// Used by RT modes to keep track of dropped frames (or not). '+' sign means
	// dropped frames while '-' not dropped
	int droppedFramesCount;
	u_int refreshDecoupling;

	// Mouse related information
	float optMoveScale;
	float optMoveStep;
	float optRotateStep;

	bool mouseButton0, mouseButton2;
	double mouseGrabLastX, mouseGrabLastY;
	double lastMouseUpdate;

	// The index of the image pipeline to show
	u_int imagePipelineIndex;

	// Same GUI loop statistic
	double guiLoopTimeShortAvg, guiLoopTimeLongAvg, guiSleepTime, guiFilmUpdateTime;
};

#define LA_LOG(a) { std::stringstream _LUXCOREUI_LOG_LOCAL_SS; _LUXCOREUI_LOG_LOCAL_SS << a; LuxCoreApp::LogHandler(_LUXCOREUI_LOG_LOCAL_SS.str().c_str()); }

#endif	/* _LUXCOREAPP_H */


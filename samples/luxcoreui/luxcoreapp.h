/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXCOREAPP_H
#define	_LUXCOREAPP_H

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <luxrays/utils/utils.h>
#include <luxcore/luxcore.h>

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
#include "haltconditionswindow.h"
#include "importancewindow.h"

#define LA_ARRAYSIZE(_ARR)	((int)(sizeof(_ARR) / sizeof(*_ARR)))

class LuxCoreApp {
public:
	LuxCoreApp(luxcore::RenderConfig *renderConfig);
	~LuxCoreApp();

	void RunApp(luxcore::RenderState *startState = NULL, luxcore::Film *startFilm = NULL);

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
	friend class HaltConditionsWindow;
	friend class StatsWindow;
	friend class UserImportancePaintWindow;

private:
	typedef enum {
		TOOL_CAMERA_EDIT,
		TOOL_OBJECT_SELECTION,
		TOOL_IMAGE_VIEW,
		TOOL_USER_IMPORTANCE_PAINT
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
	void AdjustFilmResolutionToWindowSize(unsigned int *filmWidth, unsigned int *filmHeight);
	void SetFilmResolution(const unsigned int filmWidth, const unsigned int filmHeight);
	void IncScreenRefreshInterval();
	void DecScreenRefreshInterval();
	void CloseAllRenderConfigEditors();

	void LoadRenderConfig(const std::string &configFileName);
	void StartRendering(luxcore::RenderState *startState = NULL, luxcore::Film *startFilm = NULL);
	void DeleteRendering();

	void RefreshRenderingTexture();
	void DrawRendering();
	void DrawTiles(const luxrays::Property &propCoords,
		const luxrays::Property &propPasses,
		const luxrays::Property &propPendingPasses,
		const luxrays::Property &propErrors,
		const unsigned int tileCount, const unsigned int tileWidth, const unsigned int tileHeight,
		const ImU32 col);
	void DrawTiles();
	void DrawCaptions();

	void MenuRendering();
	void MenuEngine();
	void MenuSampler();
	void MenuCamera();
	void MenuTiles();
	void MenuFilm();
	void MenuImagePipelines();
	void MenuScreen();
	void MenuTool();
	void MenuWindow();
	void MainMenuBar();

	void BakeAllSceneObjects();
	
	static LogWindow *currentLogWindow;

	AcceleratorWindow acceleratorWindow;
	EpsilonWindow epsilonWindow;
	FilmChannelsWindow filmChannelsWindow;
	FilmOutputsWindow filmOutputsWindow;
	FilmRadianceGroupsWindow filmRadianceGroupsWindow;
	LightStrategyWindow lightStrategyWindow;
#if !defined(LUXRAYS_DISABLE_OPENCL)
	OCLDeviceWindow oclDeviceWindow;
#endif
	PixelFilterWindow pixelFilterWindow;
	RenderEngineWindow renderEngineWindow;
	SamplerWindow samplerWindow;
	HaltConditionsWindow haltConditionsWindow;
	StatsWindow statsWindow;
	LogWindow logWindow;
	HelpWindow helpWindow;
	UserImportancePaintWindow userImportancePaintWindow;

	luxcore::RenderConfig *config;

	luxcore::RenderSession *session;

	GLuint renderFrameBufferTexID;
	GLenum renderFrameBufferTexMinFilter, renderFrameBufferTexMagFilter;
	GLuint backgroundLogoTexID;

	unsigned int renderImageWidth, renderImageHeight;
	float *renderImageBuffer;
	
	GLFWwindow *window;

	AppToolType currentTool;

	// ImGui height information
	int menuBarHeight, captionHeight;
	bool mouseHoverRenderingWindow;
	bool popupMenuBar;

	// ImGui inputs
	int menuFilmWidth, menuFilmHeight;
	
	int targetFilmWidth, targetFilmHeight;

	bool optRealTimeMode;
	// Used by RT modes to keep track of dropped frames (or not). '+' sign means
	// dropped frames while '-' not dropped
	int droppedFramesCount;
	unsigned int refreshDecoupling;

	// Mouse related information
	float optMoveScale;
	float optMoveStep;
	float optRotateStep;

	bool mouseButton0, mouseButton2;
	double mouseGrabLastX, mouseGrabLastY;
	double lastMouseUpdate;

	// The index of the image pipeline to show
	unsigned int imagePipelineIndex;

	// Same GUI loop statistic
	double guiLoopTimeShortAvg, guiLoopTimeLongAvg, guiSleepTime, guiFilmUpdateTime;
};

#define LA_LOG(a) { std::stringstream _LUXCOREUI_LOG_LOCAL_SS; _LUXCOREUI_LOG_LOCAL_SS << a; LuxCoreApp::LogHandler(_LUXCOREUI_LOG_LOCAL_SS.str().c_str()); }

#endif	/* _LUXCOREAPP_H */


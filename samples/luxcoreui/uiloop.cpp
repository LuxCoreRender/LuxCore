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

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp> 
#include <boost/function.hpp>

#include <imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

extern unsigned int LuxLogo_imageWidth;
extern unsigned int LuxLogo_imageHeight;
extern unsigned char LuxLogo_image[];

const string windowTitle = "LuxCore UI v" LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR " (http://www.luxcorerender.org)";

static void GLFWErrorCallback(int error, const char *description) {
  cout <<
      "GLFW Error: " << error << "\n"
      "Description: " << description << "\n";
}

void LuxCoreApp::DrawBackgroundLogo() {
  int frameBufferWidth, frameBufferHeight;
  glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

  ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
  ImGui::SetNextWindowSize(ImVec2(frameBufferWidth, frameBufferHeight), ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

  bool opened = true;
  if (ImGui::Begin("Background_logo", &opened,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs)) {
    const float pad = .2f;
    const float ratio = frameBufferWidth / (float)frameBufferHeight;
    const float border = (1.f - ratio) * .5f;

    // There seems to be a bug in ImGui, I have to move down the window or
    // it will hide the menu bar
    ImGui::SetCursorScreenPos(ImVec2(0, 25));
    ImGui::Image((ImTextureID)(intptr_t)backgroundLogoTexID,
        ImVec2(frameBufferWidth, frameBufferHeight),
        ImVec2(border - pad, 1.f + pad), ImVec2(1.f - border + pad, 0.f - pad));
  }
  ImGui::End();

  ImGui::PopStyleVar(1);
}

void LuxCoreApp::RefreshRenderingTexture() {
  const unsigned int filmWidth = session->GetFilm().GetWidth();
  const unsigned int filmHeight = session->GetFilm().GetHeight();
  const float *pixels = session->GetFilm().GetChannel<float>(Film::CHANNEL_IMAGEPIPELINE, imagePipelineIndex);

  if ((currentTool == TOOL_OBJECT_SELECTION) || (currentTool == TOOL_USER_IMPORTANCE_PAINT)) {
    // Allocate the renderImageBuffer if needed
    if (!renderImageBuffer || (renderImageWidth != filmWidth) || (renderImageHeight != filmHeight)) {
      delete[] renderImageBuffer;
      renderImageWidth = filmWidth;
      renderImageHeight = filmHeight;
      renderImageBuffer = new float[renderImageWidth * renderImageHeight * 3];
    }
  }

  if (currentTool == TOOL_OBJECT_SELECTION) {
    // Get the mouse coordinates
    int frameBufferWidth, frameBufferHeight;
    glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

    const ImVec2 imGuiScale(filmWidth /  (float)frameBufferWidth,  filmHeight / (float)frameBufferHeight);
    const int mouseX = Floor2Int(ImGui::GetIO().MousePos.x * imGuiScale.x);
    const int mouseY = Floor2Int((frameBufferHeight - ImGui::GetIO().MousePos.y - 1) * imGuiScale.y);

    // Get the selected object ID
    const unsigned int *objIDpixels = session->GetFilm().GetChannel<unsigned int>(Film::CHANNEL_OBJECT_ID);
    // 0xffffffffu is LuxRays NULL_INDEX
    unsigned int objID = 0xffffffffu;
    if ((mouseX >= 0) && (mouseX < (int)renderImageWidth) &&
        (mouseY >= 0) && (mouseY < (int)renderImageHeight))
      objID = objIDpixels[mouseX + mouseY * renderImageWidth];

    // 0xffffffffu is LuxRays NULL_INDEX
    if (objID != 0xffffffffu) {
      // Blend the current selection over the rendering
      for (unsigned int y = 0; y < filmHeight; ++y) {
        for (unsigned int x = 0; x < filmWidth; ++x) {
          const unsigned int index = x + y * renderImageWidth;
          const unsigned int index3 = index * 3;

          if (objIDpixels[index] == objID) {
            renderImageBuffer[index3] = Lerp(.5f, pixels[index3], 1.f);
            renderImageBuffer[index3 + 1] = Lerp(.5f, pixels[index3 + 1], 1.f);
            renderImageBuffer[index3 + 2] = pixels[index3 + 2];            
          } else {
            renderImageBuffer[index3] = pixels[index3];
            renderImageBuffer[index3 + 1] = pixels[index3 + 1];
            renderImageBuffer[index3 + 2] = pixels[index3 + 2];
          }
        }
      }

      pixels = renderImageBuffer;
    }
  } else if (currentTool == TOOL_USER_IMPORTANCE_PAINT) {
    if (userImportancePaintWindow.showOverlay) {
      userImportancePaintWindow.BlendImportanceMap(pixels, renderImageBuffer);
  
      pixels = renderImageBuffer;
    }
  }

  glBindTexture(GL_TEXTURE_2D, renderFrameBufferTexID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, filmWidth, filmHeight, 0, GL_RGB, GL_FLOAT, pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, renderFrameBufferTexMinFilter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, renderFrameBufferTexMagFilter);

  const double t1 = WallClockTime();
  session->UpdateStats();
  const double t2 = WallClockTime();

  // The average over last 10 frames
  guiFilmUpdateTime += (1.0 / 10.0) * (t2 - t1 - guiFilmUpdateTime);
}

void LuxCoreApp::DrawRendering() {
  int frameBufferWidth, frameBufferHeight;
  glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

  const bool adjustFilmRatio = (currentTool != TOOL_IMAGE_VIEW);
  if (adjustFilmRatio) {
    // Draw the rendering to fill all the window area

    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
    ImGui::SetNextWindowSize(ImVec2(frameBufferWidth, frameBufferHeight), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

    if (ImGui::Begin("Rendering", NULL,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoInputs)) {
      ImGui::Image((ImTextureID)(intptr_t)renderFrameBufferTexID,
          ImVec2(frameBufferWidth, frameBufferHeight),
          ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

      DrawTiles();
      
      mouseHoverRenderingWindow = true;
    }
    ImGui::End();
  } else {
    // Draw the rendering with padding to fill all the window area

    const unsigned int filmWidth = session->GetFilm().GetWidth();
    const unsigned int filmHeight = session->GetFilm().GetHeight();

    ImGui::SetNextWindowPos(ImVec2(0.f, menuBarHeight));
    ImGui::SetNextWindowContentSize(ImVec2(filmWidth, 0.f));
    ImGui::SetNextWindowSize(ImVec2(frameBufferWidth, frameBufferHeight - menuBarHeight - captionHeight), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    if (ImGui::Begin("Rendering", NULL,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_HorizontalScrollbar)) {

      ImGui::Image((ImTextureID)(intptr_t)renderFrameBufferTexID,
          ImVec2(filmWidth, filmHeight),
          ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

      DrawTiles();

      mouseHoverRenderingWindow = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    }
    ImGui::End();
  }

  ImGui::PopStyleVar(1);
}

void LuxCoreApp::DrawTiles(const Property &propCoords, const Property &propPasses,
    const Property &propPendingPasses,  const Property &propErrors,
    const unsigned int tileCount, const unsigned int tileWidth, const unsigned int tileHeight, const ImU32 col) {
  const bool showPassCount = config->GetProperties().Get(Property("screen.tiles.passcount.show")(false)).Get<bool>();
  const bool showError = config->GetProperties().Get(Property("screen.tiles.error.show")(false)).Get<bool>();

  int frameBufferWidth, frameBufferHeight;
  glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

  const unsigned int filmWidth = session->GetFilm().GetWidth();
  const unsigned int filmHeight = session->GetFilm().GetHeight();

  const bool adjustFilmRatio = (currentTool != TOOL_IMAGE_VIEW);
  ImVec2 imGuiScale;
  if (adjustFilmRatio) {
    imGuiScale.x = frameBufferWidth / (float)filmWidth;
    imGuiScale.y = frameBufferHeight / (float)filmHeight;
  } else {
    imGuiScale.x = 1.f;
    imGuiScale.y = 1.f;
  }

  for (unsigned int i = 0; i < tileCount; ++i) {
    const unsigned int xStart = propCoords.Get<unsigned int>(i * 2);
    const unsigned int yStart = propCoords.Get<unsigned int>(i * 2 + 1);
    const unsigned int width = Min(tileWidth, filmWidth - xStart - 1);
    const unsigned int height = Min(tileHeight, filmHeight - yStart - 1);

    ImGui::GetWindowDrawList()->AddRect(
        ImVec2(xStart * imGuiScale.x, (filmHeight - yStart - 1) * imGuiScale.y),
        ImVec2((xStart + width) * imGuiScale.x, (filmHeight - (yStart + height) - 1) * imGuiScale.y),
        col);

    if (showPassCount || showError) {
      float xs = xStart * imGuiScale.x + 3.f;
      float ys = (filmHeight - yStart - 1.f) * imGuiScale.y - ImGui::GetTextLineHeight() - 3.f;

      if (showError) {
        const float error = propErrors.Get<float>(i) * 256.f;
        const string errorStr = boost::str(boost::format("[%.2f]") % error);

        ImGui::SetCursorPos(ImVec2(xs, ys));
        ImGui::TextUnformatted(errorStr.c_str());
        
        ys -= ImGui::GetTextLineHeight();
      }

      if (showPassCount) {
        const unsigned int pass = propPasses.Get<unsigned int>(i);
        const unsigned int pendingPasses = propPendingPasses.Get<unsigned int>(i);
        const string passStr = boost::lexical_cast<string>(pass) + "+" + boost::lexical_cast<string>(pendingPasses);
        
        ImGui::SetCursorPos(ImVec2(xs, ys));
        ImGui::TextUnformatted(passStr.c_str());
      }
    }
  }
}

void LuxCoreApp::DrawTiles() {
  // Draw the pending, converged and not converged tiles for TILEPATHCPU or TILEPATHOCL
  const Properties &stats = session->GetStats();

  const string engineType = config->ToProperties().Get("renderengine.type").Get<string>();
  if ((engineType == "TILEPATHCPU") || (engineType == "TILEPATHOCL")) {
    const unsigned int tileWidth = stats.Get("stats.tilepath.tiles.size.x").Get<unsigned int>();
    const unsigned int tileHeight = stats.Get("stats.tilepath.tiles.size.y").Get<unsigned int>();

    if (config->GetProperties().Get(Property("screen.tiles.converged.show")(false)).Get<bool>()) {
      // Draw converged tiles borders
      glColor3f(0.f, 1.f, 0.f);
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 1.f, 0.f, 1.f));

      DrawTiles(stats.Get("stats.tilepath.tiles.converged.coords"),
          stats.Get("stats.tilepath.tiles.converged.pass"),
          stats.Get("stats.tilepath.tiles.converged.pendingpasses"),
          stats.Get("stats.tilepath.tiles.converged.error"),
          stats.Get("stats.tilepath.tiles.converged.count").Get<unsigned int>(),
          tileWidth, tileHeight, 0xff00ff00);

      ImGui::PopStyleColor();
    }

    if (config->GetProperties().Get(Property("screen.tiles.notconverged.show")(false)).Get<bool>()) {
      // Draw converged tiles borders
      glColor3f(1.f, 0.f, 0.f);
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));

      DrawTiles(stats.Get("stats.tilepath.tiles.notconverged.coords"),
          stats.Get("stats.tilepath.tiles.notconverged.pass"),
          stats.Get("stats.tilepath.tiles.notconverged.pendingpasses"),
          stats.Get("stats.tilepath.tiles.notconverged.error"),
          stats.Get("stats.tilepath.tiles.notconverged.count").Get<unsigned int>(),
          tileWidth, tileHeight, 0xff0000ff);

      ImGui::PopStyleColor();
    }

    // Draw pending tiles borders
    glColor3f(1.f, 1.f, 0.f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 0.f, 1.f));

    DrawTiles(stats.Get("stats.tilepath.tiles.pending.coords"),
        stats.Get("stats.tilepath.tiles.pending.pass"),
        stats.Get("stats.tilepath.tiles.pending.pendingpasses"),
        stats.Get("stats.tilepath.tiles.pending.error"),
        stats.Get("stats.tilepath.tiles.pending.count").Get<unsigned int>(),
        tileWidth, tileHeight, 0xff00ffff);

    ImGui::PopStyleColor();
  }
}

void LuxCoreApp::DrawCaptions() {
  const Properties &stats = session->GetStats();
  int frameBufferWidth, frameBufferHeight;
  glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);

  // Top screen label (to use only in full-screen mode)
  /*ImGui::SetNextWindowPos(ImVec2(0.f, -8.f));
  ImGui::SetNextWindowSize(ImVec2(frameBufferWidth, 0.f), ImGuiCond_Always);

  bool topOpened = true;
  if (ImGui::Begin("Top screen label", &topOpened,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs)) {
    ImGui::TextUnformatted(windowTitle.c_str());
  }
  ImGui::End();*/

  // Bottom screen label

  const u_int eyePass = stats.Get("stats.renderengine.pass.eye").Get<u_int>();
  const u_int lightPass = stats.Get("stats.renderengine.pass.light").Get<u_int>();
  const string msgPass = ((eyePass > 0) && (lightPass > 0)) ?
    boost::str(boost::format("[Pass %d (%d+%d)]") %
      stats.Get("stats.renderengine.pass").Get<u_int>() % eyePass % lightPass) :
    boost::str(boost::format("[Pass %d]") % stats.Get("stats.renderengine.pass").Get<u_int>());

  const u_int eyeSemplesSec = stats.Get("stats.renderengine.total.samplesec.eye").Get<double>();
  const u_int lightSemplesSec = stats.Get("stats.renderengine.total.samplesec.light").Get<double>();

  const string msgSemplesSec = ((eyePass > 0) && (lightPass > 0)) ?
    boost::str(boost::format("[Avg. samples/sec % 3.2fM (%.2fM+%.2fM)]") %
      (stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0) %
      (eyeSemplesSec / 1000000.0) % (lightSemplesSec / 1000000.0)) :
    boost::str(boost::format("[Avg. samples/sec % 3.2fM]") %
      (stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0));
  
  const string msgTriSec = boost::str(boost::format("[Avg. rays/sample %.2f on %.1fK tris]") %
    (stats.Get("stats.renderengine.performance.total").Get<double>() /
      stats.Get("stats.renderengine.total.samplesec").Get<double>()) %
    (stats.Get("stats.dataset.trianglecount").Get<double>() / 1000.0));
  
  const string msg = msgPass + msgSemplesSec + msgTriSec;

  ImVec2 textSize = ImGui::CalcTextSize(msg.c_str());
  const ImVec2 windowSize = ImVec2(frameBufferWidth, textSize.y + 9);

  ImGui::SetNextWindowPos(ImVec2(0.f, frameBufferHeight - windowSize.y));
  ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

  bool bottomOpened = true;
  if (ImGui::Begin("Bottom screen label", &bottomOpened,
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs)) {

    ImGui::TextUnformatted(msg.c_str());

    // The caption height is adjusted by 13 pixels to account for borders, etc.
    // I don't know how to obtain this kind of information from ImGUI.
    captionHeight = windowSize.y;
  }
  ImGui::End();
}

static void CenterWindow(GLFWwindow *window) {
  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  int windowWidth, windowHeight;
  glfwGetWindowSize(window, &windowWidth, &windowHeight);

  glfwSetWindowPos(window, (mode->width - windowWidth) / 2, (mode->height - windowHeight) / 2);
}

//------------------------------------------------------------------------------
// UI loop
//------------------------------------------------------------------------------

void LuxCoreApp::RunApp(luxcore::RenderState *startState, luxcore::Film *startFilm) {
  //--------------------------------------------------------------------------
  // Initialize GLFW
  //--------------------------------------------------------------------------

  // It is important to initialize OpenGL before OpenCL
  // (required in case of OpenGL/OpenCL interoperability)

  glfwSetErrorCallback(GLFWErrorCallback);
  if (!glfwInit())
    exit(EXIT_FAILURE);

#ifdef __APPLE__
  // working around a glf bug which sets wrong default depth
  glfwWindowHint(GLFW_DEPTH_BITS, 32);
#else
  glfwWindowHint(GLFW_DEPTH_BITS, 0);
  glfwWindowHint(GLFW_ALPHA_BITS, 0);
#endif

  //--------------------------------------------------------------------------
  // Create the window
  //--------------------------------------------------------------------------

  // Decide the window size
  unsigned int windowWidth, windowHeight;
  if (config) {
    if (optFullScreen) {
      GLFWmonitor *monitor = glfwGetPrimaryMonitor();
      const GLFWvidmode *mode = glfwGetVideoMode(monitor);

      windowWidth = mode->width;
      windowHeight = mode->height;

      targetFilmWidth = windowWidth / 2;
      targetFilmHeight = windowHeight / 2;
      config->Parse(Properties() <<
          Property("film.width")(targetFilmWidth) <<
          Property("film.height")(targetFilmHeight));
    } else {
      config->GetFilmSize(&windowWidth, &windowHeight, NULL);
      targetFilmWidth = windowWidth;
      targetFilmHeight = windowHeight;
    }
  } else {
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    if (optFullScreen) {
      windowWidth = mode->width;
      windowHeight = mode->height;
    } else {
      windowWidth = mode->width / 2;
      windowHeight = mode->height / 2;
    }

    targetFilmWidth = windowWidth;
    targetFilmHeight = windowHeight;
  }
  menuFilmWidth = targetFilmWidth;
  menuFilmHeight = targetFilmHeight;

  // Create the window
  window = glfwCreateWindow(windowWidth, windowHeight, windowTitle.c_str(),
      optFullScreen ? glfwGetPrimaryMonitor() : NULL, NULL);
  if (!window) {
    glfwTerminate();
    throw runtime_error("Error while opening GLFW window");
  }

  glfwSetWindowUserPointer(window, this);
  glfwMakeContextCurrent(window);

  // TODO
  glfwSetMouseButtonCallback(window, GLFW_MouseButtonCallBack);
  glfwSetScrollCallback(window, ImGui_ImplGlfw_ScrollCallback);
  glfwSetKeyCallback(window, GLFW_KeyCallBack);
  glfwSetCharCallback(window, ImGui_ImplGlfw_CharCallback);
  glfwSetCursorPosCallback(window, GLFW_MousePositionCallBack);

  CenterWindow(window);

  // Setup ImGui binding
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGui_ImplGlfw_InitForOpenGL(window, false);
  ImGui_ImplOpenGL2_Init();

  ImGui::GetIO().IniFilename = NULL;
  ImGui::GetIO().FontAllowUserScaling = true;

  int lastFrameBufferWidth, lastFrameBufferHeight;
  glfwGetFramebufferSize(window, &lastFrameBufferWidth, &lastFrameBufferHeight);

  if (config) {
    //----------------------------------------------------------------------
    // Start the rendering
    //----------------------------------------------------------------------

    StartRendering(startState, startFilm);
    // The render session will take care of freeing the startState and startFilm memory
  }

  //--------------------------------------------------------------------------
  // Initialize OpenGL
  //--------------------------------------------------------------------------

  glGenTextures(1, &renderFrameBufferTexID);

  // Define the background logo texture
  glGenTextures(1, &backgroundLogoTexID);
  glBindTexture(GL_TEXTURE_2D, backgroundLogoTexID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, LuxLogo_imageWidth, LuxLogo_imageHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, LuxLogo_image);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // The is a dirty trick to work around Windows prehistoric OpenGL headers
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glViewport(0, 0, lastFrameBufferWidth, lastFrameBufferHeight);

  //--------------------------------------------------------------------------
  // Refresh loop
  //--------------------------------------------------------------------------

  double currentTime;
  double lastLoop = WallClockTime();
  double lastScreenRefresh = WallClockTime();
  double lastFrameBufferSizeRefresh = WallClockTime();
  unsigned int currentFrame = 0;
  popupMenuBar = true;
  while (!glfwWindowShouldClose(window)) {
    //----------------------------------------------------------------------
    // Refresh the screen
    //----------------------------------------------------------------------


    int currentFrameBufferWidth, currentFrameBufferHeight;
    glfwGetFramebufferSize(window, &currentFrameBufferWidth, &currentFrameBufferHeight);
    // This call is outside the block below because the UI is drawn at every loop
    // and not only every 1 secs.
    glViewport(0, 0, currentFrameBufferWidth, currentFrameBufferHeight);

    // (127, 127, 127) is the color of the background Lux logo
    glClearColor(.5, .5f, .5f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);



    if (session) {
      // Refresh the frame buffer size at 1HZ
      if (WallClockTime() - lastFrameBufferSizeRefresh > 1.0) {
        // Check if the frame buffer has been resized
        // (Windows returns 0 x 0 size when the window is minimized)
        if ((currentFrameBufferWidth != 0) && (currentFrameBufferHeight != 0) &&
            ((currentFrameBufferWidth != lastFrameBufferWidth) ||
            (currentFrameBufferHeight != lastFrameBufferHeight))) {
          CloseAllRenderConfigEditors();

          // Check if I have to adjust the film ratio
          if (currentTool != TOOL_IMAGE_VIEW) {
            // Adjust the width and height to match the window width and height ratio
            unsigned int filmWidth = targetFilmWidth;
            unsigned int filmHeight = targetFilmHeight;
            AdjustFilmResolutionToWindowSize(&filmWidth, &filmHeight);

            RenderSessionParse(Properties() <<
                Property("film.width")(filmWidth) <<
                Property("film.height")(filmHeight));
          }

          lastFrameBufferWidth = currentFrameBufferWidth;
          lastFrameBufferHeight = currentFrameBufferHeight;
        }

        lastFrameBufferSizeRefresh = WallClockTime();
      }
    }

    //----------------------------------------------------------------------
    // Draw the UI
    //----------------------------------------------------------------------

    ImGui_ImplOpenGL2_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (session) {
      DrawRendering();
      DrawCaptions();
    } else {
      DrawBackgroundLogo();
    }

    acceleratorWindow.Draw();
    epsilonWindow.Draw();
    filmChannelsWindow.Draw();
    filmOutputsWindow.Draw();
    filmRadianceGroupsWindow.Draw();
    lightStrategyWindow.Draw();
    oclDeviceWindow.Draw();
    pixelFilterWindow.Draw();
    renderEngineWindow.Draw();
    samplerWindow.Draw();
    haltConditionsWindow.Draw();
    logWindow.Draw();
    helpWindow.Draw();
    if (session) {
      statsWindow.Draw();
    }
    if (session && (currentTool == TOOL_USER_IMPORTANCE_PAINT)) {
      userImportancePaintWindow.Draw();
    }

    MainMenuBar();

    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
    glfwPollEvents();

    //----------------------------------------------------------------------
    // Rendering texture refresh
    //----------------------------------------------------------------------

    // Refresh (and WaitNewFrame() for RT modes) the rendering texture only
    // if we have an active session
    if (session) {
      // Real-time modes refresh the rendering texture at every frame
      if (optRealTimeMode) {
        // Check if I'm running to slow and the GUI is becoming not responsive
        // (i.e. when the loop runs at less than 50Hz)
        if (guiLoopTimeShortAvg > 0.02) {
          // Increase the drop count proportional to the delay
          int dropAmount = Clamp(int((guiLoopTimeShortAvg - 0.02) / 0.02), 1, 20);
          //LA_LOG("Drop amount: " << dropAmount);
          droppedFramesCount += dropAmount;

          // If I have dropped more than 20 frames increase the refresh decoupling
          if (droppedFramesCount > 20) {
            ++refreshDecoupling;
            droppedFramesCount = 0;
          }
        } else {
          // I use long avg. for this check: fast to increase the
          // decoupling and slow to decrease
          if (guiLoopTimeLongAvg <= 0.02) {
            --droppedFramesCount;

            // If I have done ok more than 150 frames (about 3 secs)
            // decrease the refresh decoupling
            if (droppedFramesCount < -150) {
              refreshDecoupling = Max(1u, refreshDecoupling - 1);
              droppedFramesCount = 0;
            }
          }
        }
        //LA_LOG("Dropped frames count: " << droppedFramesCount << " (Refresh decoupling = " << refreshDecoupling << ")");

        // Refresh the rendering texture only if I'm not dropping frames
        if (currentFrame % refreshDecoupling == 0) {
          session->WaitNewFrame();
          RefreshRenderingTexture();
        }
      } else {
        // I refresh the rendering once every 5secs in image view mode
        const double screenRefreshTime = (currentTool == TOOL_IMAGE_VIEW) ?
          5.0 :
          config->ToProperties().Get("screen.refresh.interval").Get<unsigned int>() / 1000.0;

        currentTime = WallClockTime();
        if (currentTime - lastScreenRefresh >= screenRefreshTime) {
          RefreshRenderingTexture();
          lastScreenRefresh = currentTime;
        }
      }
    }

    //----------------------------------------------------------------------
    // Check for how long to sleep
    //----------------------------------------------------------------------

    if (session) {
      currentTime = WallClockTime();
      const double loopTime = currentTime - lastLoop;
      //LA_LOG("Loop time: " << loopTime * 1000.0 << "ms");
      lastLoop = currentTime;

      // The average over last 20 frames
      guiLoopTimeShortAvg += (1.0 / 20.0) * (loopTime - guiLoopTimeShortAvg);
      // The average over last 200 frames
      guiLoopTimeLongAvg += (1.0 / 200.0) * (loopTime - guiLoopTimeLongAvg);

      if (optRealTimeMode) {
        guiSleepTime = 0.0;
      } else {
        // The UI loop runs at 50HZ
        if (guiLoopTimeLongAvg < 0.02) {
          const double sleepTime = (0.02 - guiLoopTimeLongAvg) * 0.99;
          const unsigned int msSleepTime = (unsigned int)(sleepTime * 1000.0);
          //LA_LOG("Sleep time: " << msSleepTime << "ms");

          if (msSleepTime > 0) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(msSleepTime));
          }

          // The average over last 200 frames
          guiSleepTime += (1.0 / 200.0) * (sleepTime - guiSleepTime);
        } else {
          guiSleepTime -= (1.0 / 200.0) * guiSleepTime;
        } // if guiLoopTimeLongAvg
      }
    } else {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(20));
    }

    ++currentFrame;
  }

  //--------------------------------------------------------------------------
  // Stop the rendering
  //--------------------------------------------------------------------------

  delete session;
  session = NULL;

  //--------------------------------------------------------------------------
  // Exit
  //--------------------------------------------------------------------------

  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
}

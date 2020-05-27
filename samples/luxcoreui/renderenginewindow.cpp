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
#include <boost/algorithm/string/predicate.hpp>
#include <boost/thread.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// RenderEngineWindow
//------------------------------------------------------------------------------

RenderEngineWindow::RenderEngineWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Render Engine") {
	typeTable
#if !defined(LUXRAYS_DISABLE_OPENCL)
		.Add("PATHOCL", 0)
#endif
		.Add("LIGHTCPU", 1)
		.Add("PATHCPU", 2)
		.Add("BIDIRCPU", 3)
		.Add("BIDIRVMCPU", 4)
#if !defined(LUXRAYS_DISABLE_OPENCL)
		.Add("RTPATHOCL", 5)
#endif
		.Add("TILEPATHCPU", 6)
#if !defined(LUXRAYS_DISABLE_OPENCL)
		.Add("TILEPATHOCL", 7)
#endif
		.Add("RTPATHCPU", 8)
		.Add("BAKECPU", 9)
		.SetDefault("PATHCPU");
}

void RenderEngineWindow::Open() {
	ObjectEditorWindow::Open();

	suggestedVerianceClampingValue = 0.f;
}

Properties RenderEngineWindow::GetAllRenderEngineProperties(const Properties &cfgProps) const {
	Properties props = 
			cfgProps.GetAllProperties("renderengine") <<
			cfgProps.GetAllProperties("path") <<
			cfgProps.GetAllProperties("light") <<
			cfgProps.GetAllProperties("bidirvm") <<
			cfgProps.GetAllProperties("rtpath") <<
			cfgProps.GetAllProperties("tilepath") << 
			cfgProps.GetAllProperties("tile") <<
			cfgProps.GetAllProperties("native.threads.count") <<
#if !defined(LUXRAYS_DISABLE_OPENCL)
			cfgProps.GetAllProperties("opencl.task.count") <<
			cfgProps.GetAllProperties("opencl.native.threads.count") <<
#endif
			cfgProps.GetAllProperties("batch");

	if (props.IsDefined("renderengine.type")) {
		const string renderEngineType = props.Get("renderengine.type").Get<string>();
		if ((renderEngineType == "TILEPATHCPU")
#if !defined(LUXRAYS_DISABLE_OPENCL)
				|| (renderEngineType == "TILEPATHOCL")
				|| (renderEngineType == "RTPATHOCL")
#endif
				)
			props << Property("sampler.type")("TILEPATHSAMPLER");
		else if (renderEngineType == "RTPATHCPU")
			props << Property("sampler.type")("RTPATHCPUSAMPLER");
		else
			props << Property("sampler.type")("SOBOL");
	}

	if (props.IsDefined("tile.multipass.convergencetest.threshold")) {
		const float t = props.Get("tile.multipass.convergencetest.threshold").Get<float>();
		props.Delete("tile.multipass.convergencetest.threshold");
		props << Property("tile.multipass.convergencetest.threshold256")(t * 256.0);
	}

	return props;
}

void RenderEngineWindow::RefreshObjectProperties(Properties &props) {
	RenderConfig *config = app->config;
	try {
		props = GetAllRenderEngineProperties(config->ToProperties());
	} catch(exception &ex) {
		LA_LOG("RenderEngine parsing error: " << endl << ex.what());

		// Just revert to the initialized properties (note: they will include the error)
		props = GetAllRenderEngineProperties(config->GetProperties());
	}
}

void RenderEngineWindow::ParseObjectProperties(const Properties &props) {
	app->RenderConfigParse(GetAllRenderEngineProperties(props));
}

void RenderEngineWindow::DrawVarianceClampingSuggestedValue(const string &prefix, Properties &props, bool &modifiedProps) {
	ImGui::TextDisabled("Suggested value: %f", suggestedVerianceClampingValue);

	// Note: I should estimate a value only if clamping is disabled
	ImGui::SameLine();
	if (ImGui::Button("Estimate")) {
		const float Y = app->session->GetFilm().GetFilmY();
		LA_LOG("Image luminance: " << Y);
		if (Y <= 0.f)
			suggestedVerianceClampingValue = 0.f;
		else {
			const float v = Y * 10.f;
			suggestedVerianceClampingValue = v * v;
		}
	}

	if (suggestedVerianceClampingValue > 0.f) {
		ImGui::SameLine();
		if (ImGui::Button("Use")) {
			props.Set(Property(prefix + ".clamping.variance.maxvalue")(suggestedVerianceClampingValue));
			modifiedProps = true;
		}
	}
}

void RenderEngineWindow::PathGUI(Properties &props, bool &modifiedProps) {
	bool bval;
	float fval;
	int ival;

	if (ImGui::CollapsingHeader("Path Bounces", NULL, true, true)) {
		ival = props.Get("path.pathdepth.total").Get<int>();
		if (ImGui::InputInt("Total maximum recursion bounces", &ival)) {
			props.Set(Property("path.pathdepth.total")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.pathdepth.total");

		ival = props.Get("path.pathdepth.diffuse").Get<int>();
		if (ImGui::InputInt("Total maximum diffuse recursion bounces", &ival)) {
			props.Set(Property("path.pathdepth.diffuse")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.pathdepth.diffuse");

		ival = props.Get("path.pathdepth.glossy").Get<int>();
		if (ImGui::InputInt("Total maximum glossy recursion bounces", &ival)) {
			props.Set(Property("path.pathdepth.glossy")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.pathdepth.glossy");

		ival = props.Get("path.pathdepth.specular").Get<int>();
		if (ImGui::InputInt("Total maximum specular recursion bounces", &ival)) {
			props.Set(Property("path.pathdepth.specular")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.pathdepth.specular");
	}

	if (ImGui::CollapsingHeader("Russian Roulette", NULL, true, true)) {
		ival = props.Get("path.russianroulette.depth").Get<int>();
		if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
			props.Set(Property("path.russianroulette.depth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.depth");

		fval = props.Get("path.russianroulette.cap").Get<float>();
		if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
			props.Set(Property("path.russianroulette.cap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.cap");
	}

	if (ImGui::CollapsingHeader("Clamping", NULL, true, true)) {
		fval = props.Get("path.clamping.variance.maxvalue").Get<float>();
		if (ImGui::InputFloat("Variance clamping", &fval)) {
			props.Set(Property("path.clamping.variance.maxvalue")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.clamping.variance.maxvalue");
		DrawVarianceClampingSuggestedValue("path", props, modifiedProps);
	}

	if (ImGui::CollapsingHeader("Options", NULL, true, true)) {
		bval = props.Get("path.forceblackbackground.enable").Get<bool>();
		if (ImGui::Checkbox("Force black background", &bval)) {
			props.Set(Property("path.forceblackbackground.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.forceblackbackground.enable");
	}
}

void RenderEngineWindow::TilePathGUI(Properties &props, bool &modifiedProps) {
	PathGUI(props, modifiedProps);

	bool bval;
	float fval;
	int ival;

	if (ImGui::CollapsingHeader("Sampling", NULL, true, true)) {
		ival = props.Get("tilepath.sampling.aa.size").Get<int>();
		if (ImGui::InputInt(("x" + ToString(ival) + " Anti-aliasing").c_str(), &ival)) {
			props.Set(Property("tilepath.sampling.aa.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tilepath.sampling.aa.size");
	}

	if (ImGui::CollapsingHeader("Tiles", NULL, true, true)) {
		if (props.IsDefined("tile.size.x") || props.IsDefined("tile.size.y")) {
			ival = props.Get("tile.size.x").Get<int>();
			if (ImGui::SliderInt("Tile width", &ival, 8, 512, "%.0f pixels")) {
				props.Set(Property("tile.size.x")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tile.size.x");

			ival = props.Get("tile.size.y").Get<int>();
			if (ImGui::SliderInt("Tile height", &ival, 8, 512, "%.0f pixels")) {
				props.Set(Property("tile.size.y")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tile.size.y");
		} else {
			ival = props.Get("tile.size").Get<int>();
			if (ImGui::SliderInt("Tile size", &ival, 8, 512, "%.0f pixels")) {
				props.Set(Property("tile.size")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tile.size");

			if (ImGui::Button("Separate tile horizontal and vertical size")) {
				props.Set(Property("tile.size.x")(32));
				props.Set(Property("tile.size.y")(64));
				modifiedProps = true;
			}
		}

		bval = props.Get("tile.multipass.enable").Get<bool>();
		if (ImGui::Checkbox("Multi-pass rendering", &bval)) {
			props.Set(Property("tile.multipass.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tile.multipass.enable");

		if (props.Get("tile.multipass.enable").Get<bool>()) {
			fval = props.Get("tile.multipass.convergencetest.threshold256").Get<float>();
			if (ImGui::SliderFloat("Convergence test threshold", &fval, 0.f, 256.f)) {
				props.Set(Property("tile.multipass.convergencetest.threshold256")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tile.multipass.convergencetest.threshold256");

			fval = props.Get("tile.multipass.convergencetest.threshold.reduction").Get<float>();
			if (ImGui::InputFloat("Convergence test threshold reduction", &fval)) {
				props.Set(Property("tile.multipass.convergencetest.threshold.reduction")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tile.multipass.convergencetest.threshold.reduction");

			ival = props.Get("tile.multipass.convergencetest.warmup.count").Get<int>();
			if (ImGui::SliderInt("Convergence test warmup", &ival, 8, 512, "%.0f samples/pixel")) {
				props.Set(Property("tile.multipass.convergencetest.warmup.count")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tile.multipass.convergencetest.warmup.count");
		}
	}
}

#if !defined(LUXRAYS_DISABLE_OPENCL)
void RenderEngineWindow::PathOCLGUI(Properties &props, bool &modifiedProps) {
	PathGUI(props, modifiedProps);

	if (ImGui::CollapsingHeader("OpenCL Options", NULL, true, true)) {
		bool bval;
		int ival;

		bval = props.Get("pathocl.pixelatomics.enable").Get<float>();
		if (ImGui::Checkbox("Use pixel atomics", &bval)) {
			props.Set(Property("pathocl.pixelatomics.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("pathocl.pixelatomics.enable");

		bool autoOCLTaskCount = (props.Get("opencl.task.count").Get<string>() == "AUTO") ? true : false;
		if (ImGui::Checkbox("Automatic OpenCL task count", &autoOCLTaskCount)) {
			if (autoOCLTaskCount)
				props.Set(Property("opencl.task.count")("AUTO"));
			else
				props.Set(Property("opencl.task.count")(128 * 1024 * 1024));

			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("opencl.task.count");

		if (!autoOCLTaskCount) {
			ival = props.Get("opencl.task.count").Get<int>();
			if (ImGui::InputInt("OpenCL threads count", &ival, 8 * 1024, 128 * 1024)) {
				props.Set(Property("opencl.task.count")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("opencl.task.count");
		}

		ival = props.Get("opencl.native.threads.count").Get<int>();
		if (ImGui::SliderInt("Native rendering threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("opencl.native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("opencl.native.threads.count");
	}
}

void RenderEngineWindow::TilePathOCLGUI(Properties &props, bool &modifiedProps) {
	TilePathGUI(props, modifiedProps);

	if (ImGui::CollapsingHeader("OpenCL Options", NULL, true, true)) {
		int ival;

		ival = props.Get("tilepathocl.devices.maxtiles").Get<int>();
		if (ImGui::SliderInt("Max. number of tiles sent to an OpenCL device", &ival, 1, 32)) {
			props.Set(Property("tilepathocl.devices.maxtiles")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("tilepathocl.devices.maxtiles");

		ival = props.Get("opencl.native.threads.count").Get<int>();
		if (ImGui::SliderInt("Native rendering threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("opencl.native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("opencl.native.threads.count");
	}
}
#endif

void RenderEngineWindow::BiDirGUI(Properties &props, bool &modifiedProps) {
	float fval;
	int ival;

	if (ImGui::CollapsingHeader("Path Depths", NULL, true, true)) {
		ival = props.Get("path.maxdepth").Get<int>();
		if (ImGui::InputInt("Maximum eye path recursion depth", &ival)) {
			props.Set(Property("path.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.maxdepth");

		ival = props.Get("light.maxdepth").Get<int>();
		if (ImGui::InputInt("Maximum light path recursion depth", &ival)) {
			props.Set(Property("light.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("light.maxdepth");
	}

	if (ImGui::CollapsingHeader("Russian Roulette", NULL, true, true)) {
		ival = props.Get("path.russianroulette.depth").Get<int>();
		if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
			props.Set(Property("path.russianroulette.depth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.depth");

		fval = props.Get("path.russianroulette.cap").Get<float>();
		if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
			props.Set(Property("path.russianroulette.cap")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.russianroulette.cap");
	}

	ThreadsGUI(props, modifiedProps);
}

void RenderEngineWindow::ThreadsGUI(Properties &props, bool &modifiedProps) {
	if (ImGui::CollapsingHeader("Threads", NULL, true, true)) {
		int ival = props.Get("native.threads.count").Get<int>();
		if (ImGui::SliderInt("Threads count", &ival, 1, boost::thread::hardware_concurrency())) {
			props.Set(Property("native.threads.count")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("native.threads.count");
	}
}

bool RenderEngineWindow::DrawObjectGUI(Properties &props, bool &modifiedProps) {
	//--------------------------------------------------------------------------
	// renderengine.type
	//--------------------------------------------------------------------------

	const string currentRenderEngineType = props.Get(Property("renderengine.type")(typeTable.GetDefaultTag())).Get<string>();
	int typeIndex = typeTable.GetVal(currentRenderEngineType);

	if (ImGui::Combo("Render Engine type", &typeIndex, typeTable.GetTagList())) {
		const string newRenderEngineType = typeTable.GetTag(typeIndex);

		if ((newRenderEngineType == "TILEPATHCPU")
#if !defined(LUXRAYS_DISABLE_OPENCL)
				|| (newRenderEngineType == "TILEPATHOCL")
				|| (newRenderEngineType == "RTPATHOCL")
#endif
				)
			app->samplerWindow.Close();
#if !defined(LUXRAYS_DISABLE_OPENCL)
		if (!boost::ends_with(newRenderEngineType, "OCL"))
			app->oclDeviceWindow.Close();
#endif

		props.Clear();
		props << Property("renderengine.type")(newRenderEngineType);

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("renderengine.type");

	//--------------------------------------------------------------------------
	// RTPATHOCL
	//--------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (typeIndex == typeTable.GetVal("RTPATHOCL")) {
		TilePathOCLGUI(props, modifiedProps);

		if (ImGui::CollapsingHeader("Sampling", NULL, true, true)) {
			int ival = props.Get("tilepath.sampling.aa.size").Get<int>();
			if (ImGui::InputInt(("x" + ToString(ival) + " Anti-aliasing").c_str(), &ival)) {
				props.Set(Property("tilepath.sampling.aa.size")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("tilepath.sampling.aa.size");
		}

		if (ImGui::CollapsingHeader("Real Time", NULL, true, true)) {
			// Preview phase

			int ival = props.Get("rtpath.resolutionreduction.preview").Get<int>();
			if (ImGui::SliderInt("Resolution preview zoom", &ival, 1, 64)) {
				ival = RoundUpPow2(ival);
				props.Set(Property("rtpath.resolutionreduction.preview")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction.preview");

			ival = props.Get("rtpath.resolutionreduction.preview.step").Get<int>();
			if (ImGui::SliderInt("Resolution preview length", &ival, 1, 64)) {
				props.Set(Property("rtpath.resolutionreduction.preview.step")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction.preview.step");

			// Normal phase

			ival = props.Get("rtpath.resolutionreduction").Get<int>();
			if (ImGui::SliderInt("Resolution zoom", &ival, 1, 64)) {
				ival = RoundUpPow2(ival);
				props.Set(Property("rtpath.resolutionreduction")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction");
		}

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor"))
			app->oclDeviceWindow.Open();
	}
#endif

	//--------------------------------------------------------------------------
	// TILEPATHOCL
	//--------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (typeIndex == typeTable.GetVal("TILEPATHOCL")) {
		TilePathOCLGUI(props, modifiedProps);

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor"))
			app->oclDeviceWindow.Open();
	}
#endif

	//--------------------------------------------------------------------------
	// TILEPATHCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("TILEPATHCPU")) {
		TilePathGUI(props, modifiedProps);
		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//--------------------------------------------------------------------------
	// PATHOCL
	//--------------------------------------------------------------------------

#if !defined(LUXRAYS_DISABLE_OPENCL)
	if (typeIndex == typeTable.GetVal("PATHOCL")) {
		PathOCLGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor"))
			app->oclDeviceWindow.Open();
	}
#endif

	//--------------------------------------------------------------------------
	// PATHCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("PATHCPU")) {
		PathGUI(props, modifiedProps);

		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//--------------------------------------------------------------------------
	// RTPATHCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("RTPATHCPU")) {
		PathGUI(props, modifiedProps);
		ThreadsGUI(props, modifiedProps);

		if (ImGui::CollapsingHeader("Real Time", NULL, true, true)) {
			int ival = props.Get("rtpathcpu.zoomphase.size").Get<int>();
			if (ImGui::InputInt("Zoom phase size", &ival)) {
				props.Set(Property("rtpathcpu.zoomphase.size")(Max(ival, 1)));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpathcpu.zoomphase.size");

			float fval = props.Get("rtpathcpu.zoomphase.weight").Get<float>();
			if (ImGui::InputFloat("Zoom phase weight", &fval)) {
				props.Set(Property("rtpathcpu.zoomphase.weight")(Max(fval, .0001f)));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpathcpu.zoomphase.weight");
		}
	}

	//--------------------------------------------------------------------------
	// BIDIRCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIDIRCPU")) {
		BiDirGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//--------------------------------------------------------------------------
	// BIDIRVMCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIDIRVMCPU")) {
		BiDirGUI(props, modifiedProps);

		if (ImGui::CollapsingHeader("BiDirVM Options", NULL, true, true)) {
			float fval;
			int ival;

			ival = props.Get("bidirvm.lightpath.count").Get<int>();
			if (ImGui::SliderInt("Light path count for each pass", &ival, 256, 128 * 1024)) {
				props.Set(Property("bidirvm.lightpath.count")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("bidirvm.lightpath.count");

			fval = props.Get("bidirvm.startradius.scale").Get<float>();
			if (ImGui::InputFloat("Start radius scale", &fval)) {
				props.Set(Property("bidirvm.startradius.scale")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("bidirvm.startradius.scale");

			fval = props.Get("bidirvm.alpha").Get<float>();
			if (ImGui::SliderFloat("Radius reduction", &fval, 0.f, 1.f)) {
				props.Set(Property("bidirvm.alpha")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("bidirvm.alpha");
		}

		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//--------------------------------------------------------------------------
	// LIGHTCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("LIGHTCPU")) {
		float fval;
		int ival;

		if (ImGui::CollapsingHeader("Path Depth", NULL, true, true)) {
			ival = props.Get("path.pathdepth.total").Get<int>();
			if (ImGui::InputInt("Maximum light path recursion depth", &ival)) {
				props.Set(Property("path.pathdepth.total")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("path.pathdepth.total");
		}

		if (ImGui::CollapsingHeader("Russian Roulette", NULL, true, true)) {
			ival = props.Get("path.russianroulette.depth").Get<int>();
			if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
				props.Set(Property("path.russianroulette.depth")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("path.russianroulette.depth");

			fval = props.Get("path.russianroulette.cap").Get<float>();
			if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
				props.Set(Property("path.russianroulette.cap")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("path.russianroulette.cap");
		}

		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//--------------------------------------------------------------------------
	// BAKECPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BAKECPU")) {
		PathGUI(props, modifiedProps);

		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	return false;
}

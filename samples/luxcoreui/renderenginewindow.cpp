/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include <iostream>
#include <boost/algorithm/string/predicate.hpp>

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

//------------------------------------------------------------------------------
// RenderEngineWindow
//------------------------------------------------------------------------------

RenderEngineWindow::RenderEngineWindow(LuxCoreApp *a) : ObjectEditorWindow(a, "Render Engine") {
	typeTable
		.Add("PATHOCL", 0)
		.Add("LIGHTCPU", 1)
		.Add("PATHCPU", 2)
		.Add("BIDIRCPU", 3)
		.Add("BIDIRVMCPU", 4)
		.Add("RTPATHOCL", 5)
		.Add("BIASPATHCPU", 6)
		.Add("BIASPATHOCL", 7)
		.Add("RTBIASPATHOCL", 8)
		.Add("RTPATHCPU", 9)
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
			cfgProps.GetAllProperties("biaspath") << 
			cfgProps.GetAllProperties("tile") <<
			cfgProps.GetAllProperties("native.threads.count") <<
			cfgProps.GetAllProperties("opencl.task.count");

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
		const u_int filmWidth = app->session->GetFilm().GetWidth();
		const u_int filmHeight = app->session->GetFilm().GetHeight();

		float *pixels = new float[filmWidth * filmHeight * 3];
		app->session->GetFilm().GetOutput<float>(Film::OUTPUT_RGB, pixels, 0);

		float Y = 0.f;
		for (u_int i = 0; i < filmWidth * filmHeight; ++i) {
			const Spectrum *p = (const Spectrum *)&pixels[i];

			const float y = p->Y();
			if ((y <= 0.f) || isinf(y))
				continue;

			Y += y;
		}
		delete[] pixels;

		const u_int pixelCount = filmWidth * filmHeight;
		Y /= pixelCount;
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

	if (ImGui::CollapsingHeader("Path Depth", NULL, true, true)) {
		ival = props.Get("path.maxdepth").Get<int>();
		if (ImGui::InputInt("Maximum recursion depth", &ival)) {
			props.Set(Property("path.maxdepth")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.maxdepth");
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

		fval = props.Get("path.clamping.pdf.value").Get<float>();
		if (ImGui::InputFloat("PDF clamping", &fval)) {
			props.Set(Property("path.clamping.pdf.value")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.clamping.pdf.value");
	}

	if (ImGui::CollapsingHeader("Options", NULL, true, true)) {
		bval = props.Get("path.fastpixelfilter.enable").Get<bool>();
		if (ImGui::Checkbox("Use fast pixel filter", &bval)) {
			props.Set(Property("path.fastpixelfilter.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.fastpixelfilter.enable");

		bval = props.Get("path.forceblackbackground.enable").Get<bool>();
		if (ImGui::Checkbox("Force black background", &bval)) {
			props.Set(Property("path.forceblackbackground.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("path.forceblackbackground.enable");
	}
}

void RenderEngineWindow::BiasPathGUI(Properties &props, bool &modifiedProps, const bool rtMode) {
	bool bval;
	float fval;
	int ival;

	if (ImGui::CollapsingHeader("Path Depths", NULL, true, true)) {
		ival = props.Get("biaspath.pathdepth.total").Get<int>();
		if (ImGui::InputInt("Maximum total recursion depth", &ival)) {
			props.Set(Property("biaspath.pathdepth.total")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.pathdepth.total");

		ival = props.Get("biaspath.pathdepth.diffuse").Get<int>();
		if (ImGui::InputInt("Maximum diffuse recursion depth", &ival)) {
			props.Set(Property("biaspath.pathdepth.diffuse")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.pathdepth.diffuse");

		ival = props.Get("biaspath.pathdepth.glossy").Get<int>();
		if (ImGui::InputInt("Maximum glossy recursion depth", &ival)) {
			props.Set(Property("biaspath.pathdepth.glossy")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.pathdepth.glossy");

		ival = props.Get("biaspath.pathdepth.specular").Get<int>();
		if (ImGui::InputInt("Maximum specular recursion depth", &ival)) {
			props.Set(Property("biaspath.pathdepth.specular")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.pathdepth.specular");
	}

	if (ImGui::CollapsingHeader("Sampling", NULL, true, true)) {
		ival = props.Get("biaspath.sampling.aa.size").Get<int>();
		if (ImGui::InputInt(("x" + ToString(ival) + " Anti-aliasing").c_str(), &ival)) {
			props.Set(Property("biaspath.sampling.aa.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.sampling.aa.size");

		ival = props.Get("biaspath.sampling.diffuse.size").Get<int>();
		if (ImGui::InputInt(("x" + ToString(ival) + " Diffuse samples").c_str(), &ival)) {
			props.Set(Property("biaspath.sampling.diffuse.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.sampling.diffuse.size");

		ival = props.Get("biaspath.sampling.glossy.size").Get<int>();
		if (ImGui::InputInt(("x" + ToString(ival) + " Glossy samples").c_str(), &ival)) {
			props.Set(Property("biaspath.sampling.glossy.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.sampling.glossy.size");

		ival = props.Get("biaspath.sampling.specular.size").Get<int>();
		if (ImGui::InputInt(("x" + ToString(ival) + " Specular samples").c_str(), &ival)) {
			props.Set(Property("biaspath.sampling.specular.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.sampling.specular.size");

		ival = props.Get("biaspath.sampling.directlight.size").Get<int>();
		if (ImGui::InputInt(("x" + ToString(ival) + " Direct light samples").c_str(), &ival)) {
			props.Set(Property("biaspath.sampling.directlight.size")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.sampling.directlight.size");
	}

	if (ImGui::CollapsingHeader("Clamping", NULL, true, true)) {
		fval = props.Get("biaspath.clamping.variance.maxvalue").Get<float>();
		if (ImGui::InputFloat("Variance clamping", &fval)) {
			props.Set(Property("biaspath.clamping.variance.maxvalue")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.clamping.variance.maxvalue");
		DrawVarianceClampingSuggestedValue("biaspath", props, modifiedProps);

		fval = props.Get("biaspath.clamping.pdf.value").Get<float>();
		if (ImGui::InputFloat("PDF clamping", &fval)) {
			props.Set(Property("biaspath.clamping.pdf.value")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.clamping.pdf.value");
	}

	if (ImGui::CollapsingHeader("Lights", NULL, true, true)) {
		fval = props.Get("biaspath.lights.lowthreshold").Get<float>();
		if (ImGui::InputFloat("Light low intensity threshold", &fval)) {
			props.Set(Property("biaspath.lights.lowthreshold")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.lights.lowthreshold");

		fval = props.Get("biaspath.lights.nearstart").Get<float>();
		if (ImGui::InputFloat("Light distance threshold", &fval)) {
			props.Set(Property("biaspath.lights.nearstart")(fval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.lights.nearstart");

		ival = props.Get("biaspath.lights.firstvertexsamples").Get<int>();
		if (ImGui::InputInt("First hit direct light samples", &ival)) {
			props.Set(Property("biaspath.lights.firstvertexsamples")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.lights.firstvertexsamples");
	}

	if (!rtMode && ImGui::CollapsingHeader("Tiles", NULL, true, true)) {
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

	if (ImGui::CollapsingHeader("Options", NULL, true, true)) {
		bval = props.Get("biaspath.forceblackbackground.enable").Get<bool>();
		if (ImGui::Checkbox("Force black background", &bval)) {
			props.Set(Property("biaspath.forceblackbackground.enable")(bval));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspath.forceblackbackground.enable");
	}
}

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
	}
}

void RenderEngineWindow::BiasPathOCLGUI(Properties &props, bool &modifiedProps, const bool rtMode) {
	BiasPathGUI(props, modifiedProps, rtMode);

	if (!rtMode && ImGui::CollapsingHeader("OpenCL Options", NULL, true, true)) {
		int ival;

		ival = props.Get("biaspathocl.devices.maxtiles").Get<int>();
		if (ImGui::SliderInt("Max. number of tiles sent to an OpenCL device", &ival, 1, 32)) {
			props.Set(Property("biaspathocl.devices.maxtiles")(ival));
			modifiedProps = true;
		}
		LuxCoreApp::HelpMarker("biaspathocl.devices.maxtiles");
	}
}

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

		if (boost::starts_with(newRenderEngineType, "BIAS"))
			app->samplerWindow.Close();
		if (!boost::ends_with(newRenderEngineType, "OCL"))
			app->oclDeviceWindow.Close();

		props.Clear();
		props << Property("renderengine.type")(newRenderEngineType);

		return true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("renderengine.type");

	//--------------------------------------------------------------------------
	// RTBIASPATHOCL
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("RTBIASPATHOCL")) {
		BiasPathOCLGUI(props, modifiedProps, true);

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
			if (ImGui::SliderInt("Resolution preview length", &ival, 1, 32)) {
				props.Set(Property("rtpath.resolutionreduction.preview.step")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction.preview.step");

			bool bval = props.Get("rtpath.resolutionreduction.preview.dlonly.enable").Get<float>();
			if (ImGui::Checkbox("Use direct light sampling only on preview", &bval)) {
				props.Set(Property("rtpath.resolutionreduction.preview.dlonly.enable")(bval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction.preview.dlonly.enable");

			// Normal phase

			ival = props.Get("rtpath.resolutionreduction").Get<int>();
			if (ImGui::SliderInt("Resolution zoom", &ival, 1, 64)) {
				ival = RoundUpPow2(ival);
				props.Set(Property("rtpath.resolutionreduction")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction");

			// Long run phase

			ival = props.Get("rtpath.resolutionreduction.longrun").Get<int>();
			if (ImGui::SliderInt("Resolution long run zoom", &ival, 16, 128)) {
				ival = RoundUpPow2(ival);
				props.Set(Property("rtpath.resolutionreduction.longrun")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction.longrun");

			ival = props.Get("rtpath.resolutionreduction.longrun.step").Get<int>();
			if (ImGui::SliderInt("Resolution long run length", &ival, 0, 32)) {
				props.Set(Property("rtpath.resolutionreduction.longrun.step")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.resolutionreduction.longrun.step");
		}

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor"))
			app->oclDeviceWindow.Open();
	}

	//--------------------------------------------------------------------------
	// BIASPATHOCL
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIASPATHOCL")) {
		BiasPathOCLGUI(props, modifiedProps, false);

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor"))
			app->oclDeviceWindow.Open();
	}

	//--------------------------------------------------------------------------
	// BIASPATHCPU
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("BIASPATHCPU")) {
		BiasPathGUI(props, modifiedProps, false);
		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	//--------------------------------------------------------------------------
	// RTPATHOCL
	//--------------------------------------------------------------------------

	if (typeIndex == typeTable.GetVal("RTPATHOCL")) {
		PathOCLGUI(props, modifiedProps);

		if (ImGui::CollapsingHeader("Real Time", NULL, true, true)) {
			int ival = props.Get("rtpath.miniterations").Get<int>();
			if (ImGui::InputInt("Min. pass count per frame", &ival)) {
				props.Set(Property("rtpath.miniterations")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("rtpath.miniterations");
		}

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open OpenCL device editor"))
			app->oclDeviceWindow.Open();
	}

	//--------------------------------------------------------------------------
	// PATHOCL
	//--------------------------------------------------------------------------

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
			ival = props.Get("light.maxdepth").Get<int>();
			if (ImGui::InputInt("Maximum light path recursion depth", &ival)) {
				props.Set(Property("light.maxdepth")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("light.maxdepth");
		}

		if (ImGui::CollapsingHeader("Russian Roulette", NULL, true, true)) {
			ival = props.Get("light.russianroulette.depth").Get<int>();
			if (ImGui::InputInt("Russian Roulette start depth", &ival)) {
				props.Set(Property("light.russianroulette.depth")(ival));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("light.russianroulette.depth");

			fval = props.Get("light.russianroulette.cap").Get<float>();
			if (ImGui::SliderFloat("Russian Roulette threshold", &fval, 0.f, 1.f)) {
				props.Set(Property("light.russianroulette.cap")(fval));
				modifiedProps = true;
			}
			LuxCoreApp::HelpMarker("light.russianroulette.cap");
		}

		ThreadsGUI(props, modifiedProps);

		if (ImGui::Button("Open Sampler editor"))
			app->samplerWindow.Open();
		ImGui::SameLine();
		if (ImGui::Button("Open Pixel Filter editor"))
			app->pixelFilterWindow.Open();
	}

	return false;
}

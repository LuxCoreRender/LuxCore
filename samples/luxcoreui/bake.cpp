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

#include <string>
#include <regex>
 
#include "luxcore/luxcore.h"

#include "luxcoreapp.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

static string SanitizeName(const string &str) {
	return regex_replace(str, regex("[^a-zA-Z_0-9]+"), "__");
}

//------------------------------------------------------------------------------
// Bake all scene objects
//------------------------------------------------------------------------------

void LuxCoreApp::BakeAllSceneObjects() {
	if (!session)
		return;

	// Stop the current rendering
	session->Stop();

	// The render configuration and the scene properties
	const RenderConfig &renderConfig = session->GetRenderConfig();
	const Properties &cfgProps =  renderConfig.ToProperties();
	const Properties &sceneProps =  renderConfig.GetScene().ToProperties();

	// Build the list of scene objects
	vector<string> objKeys = sceneProps.GetAllUniqueSubNames("scene.objects");
	LC_LOG("Number of objects to bake: " + objKeys.size())
	if (objKeys.size() == 0) {
		// There are not object definitions ... quite strange
		return;
	}

	vector<string> objectNames;
	for (auto const &key : objKeys) {
		// Extract the object name
		const string objName = Property::ExtractField(key, 2);
		if (objName == "")
			throw runtime_error("Syntax error in " + key);

		objectNames.push_back(objName);
	}

	// Build the properties for bake rendering
	Properties bakeProps;
	bakeProps <<
			Property("renderengine.type")("BAKECPU") <<
			Property("sampler.type")("SOBOL") <<
			Property("film.filter.type")("BLACKMANHARRIS") <<
			Property("film.filter.width")(2.f) <<
			Property("batch.haltspp")(32) <<
			Property("batch.haltnoisethreshold")(.01f) <<
			Property("bake.minmapautosize")(64) <<
			Property("bake.maxmapautosize")(1024) <<
			Property("bake.powerof2autosize.enable")(true) <<
			Property("bake.skipexistingmapfiles")(true);

	// Check the number of image pipelines
	//
	// Note: I assume the index starts from 0
	const u_int imagePiplinesCount = cfgProps.GetAllUniqueSubNames("film.imagepipelines").size();
	LC_LOG("Number of image pipelines: " + imagePiplinesCount)

	// Add a NOP image pipeline
	bakeProps <<
			Property("film.imagepipelines." + ToString(imagePiplinesCount) + ".0.type")("NOP");

	// Build the properties for baking all objects
	u_int objectIndex = 0;
	for (auto const &objectName : objectNames) {
		const string prefix = "bake.maps." + ToString(objectIndex);

		bakeProps <<
			Property(prefix + ".type")("COMBINED") <<
			Property(prefix + ".filename")(SanitizeName(objectName) + ".exr") <<
			Property(prefix + ".imagepipelineindex")(imagePiplinesCount) <<
			Property(prefix + ".width")(512) <<
			Property(prefix + ".height")(512) <<
			Property(prefix + ".autosize.enabled")(true) <<
			Property(prefix + ".uvindex")(0) <<
			Property(prefix + ".objectnames")(objectName);
		
		++objectIndex;
	}

	// Write the complete new render config to file
	Properties completeBakeCfgProps;
	completeBakeCfgProps <<
			cfgProps <<
			bakeProps;
	completeBakeCfgProps.Save("render-bakeallobjects.cfg");

	// Write the complete new baked scene to file
	Properties completeBakedSceneProps;
	completeBakedSceneProps << sceneProps;
	for (auto const &objectName : objectNames) {
		const string prefix = "scene.objects." + objectName;

		completeBakedSceneProps <<
				Property(prefix + ".bake.combined.file")(SanitizeName(objectName) + ".exr") <<
				Property(prefix + ".bake.combined.gamma")(1.f) <<
				Property(prefix + ".bake.combined.wrap")("clamp");
	}
	completeBakedSceneProps.Save("scene-bakedallobjects.scn");
	
	// Write the complete new baked config file to file
	Properties completeBakedCfgProps;
	completeBakedCfgProps <<
			cfgProps <<
			Property("scene.file")("scene-bakedallobjects.scn");
	completeBakedCfgProps.Save("render-bakedallobjects.cfg");
	
	// Start the backing rendering
	RenderConfigParse(completeBakeCfgProps);
}

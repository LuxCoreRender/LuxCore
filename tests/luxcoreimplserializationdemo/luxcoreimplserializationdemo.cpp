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

// This is a test of SLG internal serialization. The code used here is not part of
// LuxCore API and should be ignored aside from LuxRender core developers.
//
// Note: work in progress

#include <fstream>
#include <memory>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/serialization/version.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
#include "slg/scene/scene.h"
#include "slg/renderconfig.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace slg;

/*static void TestPropertiesSerialization() {
	Properties props;
	props <<
			Property("test1.prop1")(true) <<
			Property("test2.prop2")(1.f, 2.f, 3.f) <<
			Property("test3.prop3")("test");

	// Serialize to a file
	{
		BOOST_OFSTREAM outFile;
		outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
		outFile.open("test-ser.txt", BOOST_OFSTREAM::binary);


		// Enable compression
		boost::iostreams::filtering_ostream outStream;
		outStream.push(outFile);

		// Use portable archive
		eos::polymorphic_portable_oarchive outArchive(outStream);

		Properties *propsPtr = &props;
		outArchive << propsPtr;

		if (!outStream.good())
			throw runtime_error("Error while saving serialized properties");

		flush(outStream);
	}

	// De-serialize from a file

	Properties *propsCpy;
	{
		BOOST_IFSTREAM inFile;
		inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
		inFile.open("test-ser.txt", BOOST_IFSTREAM::binary);

		// Create an input filtering stream
		boost::iostreams::filtering_istream inStream;

		// Enable compression
		inStream.push(inFile);

		// Use portable archive
		eos::polymorphic_portable_iarchive inArchive(inStream);

		inArchive >> propsCpy;

		if (!inStream.good())
			throw runtime_error("Error while loading serialized properties");
	}

	if (propsCpy->GetSize() != props.GetSize())
		throw runtime_error("Wrong properties size");
	if (propsCpy->Get("test1.prop1").Get<bool>() != props.Get("test1.prop1").Get<bool>())
		throw runtime_error("Wrong properties [0] value");
	if (propsCpy->Get("test2.prop2").Get<float>(2) != props.Get("test2.prop2").Get<float>(2))
		throw runtime_error("Wrong properties [1] value");
	if (propsCpy->Get("test3.prop3").Get<string>() != props.Get("test3.prop3").Get<string>())
		throw runtime_error("Wrong properties [2] value");
}

static void TestFilmSerialization() {
	// Create a film
	SLG_LOG("Create a film");

	Film film(512, 512);
	film.oclEnable = false;
	film.AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film.AddChannel(Film::IMAGEPIPELINE);

	ImagePipeline *ip = new ImagePipeline();
	//ip->AddPlugin(new AutoLinearToneMap());
	ip->AddPlugin(new GammaCorrectionPlugin());
	film.SetImagePipelines(ip);

	film.Init();

	for (u_int y = 0; y < 512; ++y) {
		for (u_int x = 0; x < 512; ++x) {
			const float rgb[3] = { x / 512.f, y / 512.f, 0.f };
			film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->AddWeightedPixel(x, y, rgb, 1.f);
		}
	}

	film.ExecuteImagePipeline(0);
	film.Output("film-orig.png", FilmOutputs::RGB_IMAGEPIPELINE);

	// Write the film
	SLG_LOG("Write the film");
	Film::SaveSerialized("film.flm", &film);

	// Read the film
	SLG_LOG("Read the film");
	auto_ptr<Film> filmCopy(Film::LoadSerialized("film.flm"));
	filmCopy->oclEnable = false;
	
	filmCopy->ExecuteImagePipeline(0);
	filmCopy->Output("film-copy.png", FilmOutputs::RGB_IMAGEPIPELINE);
}*/

static void TestSceneSerialization() {
	// Create the scene file
	{
		SLG_LOG("Create a scene");
		//Scene scene("scenes/cornell/cornell.scn");
		Scene scene("scenes/cat/cat.scn");

		// Write the scene
		SLG_LOG("Write the scene");
		Scene::SaveSerialized("scene.bsc", &scene);
	}

	// Read the scene
	SLG_LOG("Read the scene");
	auto_ptr<Scene> sceneCopy(Scene::LoadSerialized("scene.bsc"));
}

/*static void TestRenderConfigSerialization() {
	// Create the scene file
	{
		SLG_LOG("Create a render cofiguration");
		RenderConfig renderConfig(Properties("scenes/cornell/cornell.cfg"));
		//Scene scene("scenes/cat/cat.cfg");

		// Write the scene
		SLG_LOG("Write the render configuration");
		RenderConfig::SaveSerialized("renderconfig.bcf", &renderConfig);
	}

	// Read the scene
	SLG_LOG("Read the scene");
	auto_ptr<RenderConfig> renderConfigCopy(RenderConfig::LoadSerialized("renderconfig.bsc"));
}*/

int main(int argc, char *argv[]) {
	luxcore::Init();

	//TestPropertiesSerialization();
	//TestFilmSerialization();
	TestSceneSerialization();
	//TestRenderConfigSerialization();

	SLG_LOG("Done.");

	return EXIT_SUCCESS;
}

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
// LuxCore API and should be ignored aside from LuxCoreRender core developers.
//
// Note: work in progress

#include <fstream>
#include <memory>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
#include "slg/imagemap/imagemap.h"
#include "slg/scene/scene.h"
#include "slg/renderconfig.h"
#include "luxcore/luxcore.h"

using namespace std;
using namespace luxrays;
using namespace slg;

static void TestPropertiesSerialization() {
	Properties props;
	props <<
			Property("test1.prop1")(true) <<
			Property("test2.prop2")(1.f, 2.f, 3.f) <<
			Property("test3.prop3")("test");

	// Serialize to a file
	{
		SerializationOutputFile sof("test-ser.txt");

		Properties *propsPtr = &props;
		sof.GetArchive() << propsPtr;

		if (!sof.IsGood())
			throw runtime_error("Error while saving serialized properties");

		sof.Flush();
	}

	// De-serialize from a file

	Properties *propsCpy;
	{
		SerializationInputFile sif("test-ser.txt");

		sif.GetArchive() >> propsCpy;

		if (!sif.IsGood())
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
	film.hwEnable = false;
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

	//film.ExecuteImagePipeline(0);
	film.AsyncExecuteImagePipeline(0);
	film.WaitAsyncExecuteImagePipeline();
//	while(!film.HasDoneAsyncExecuteImagePipeline())
//		SLG_LOG("Waiting...");
	film.Output("film-orig.png", FilmOutputs::RGB_IMAGEPIPELINE, NULL, false);

	// Write the film
	SLG_LOG("Write the film");
	Film::SaveSerialized("film.flm", &film);

	// Read the film
	SLG_LOG("Read the film");
	unique_ptr<Film> filmCopy(Film::LoadSerialized("film.flm"));
	filmCopy->hwEnable = false;
	
	filmCopy->ExecuteImagePipeline(0);
	filmCopy->Output("film-copy.png", FilmOutputs::RGB_IMAGEPIPELINE);
}

void TestImageMapSerialization(const ImageMapStorage::StorageType storageType) {
//	First test result.
//	4096x4096
//
//	u_char =>
//	[LuxCore][2.726] ImageMap saved: 556 Kbytes
//	[LuxCore][2.726] ImageMap save time: 2.67359 secs
//	[LuxCore][4.656] ImageMap load time: 1.92929 secs
//	half =>
//	[LuxCore][9.968] ImageMap saved: 4180 Kbytes
//	[LuxCore][9.969] ImageMap save time: 5.16866 secs
//	[LuxCore][14.803] ImageMap load time: 4.83383 secs
//	float =>
//	[LuxCore][19.605] ImageMap saved: 21896 Kbytes
//	[LuxCore][19.605] ImageMap save time: 4.62094 secs
//	[LuxCore][22.474] ImageMap load time: 2.86875 secs
//
//	No difference using BOOST_CLASS_IMPLEMENTATION(..., boost::serialization::object_serializable)
//
//	No difference using boost::serialization::make_array()
//
//	Using boost::archive::binary_*archive instead of eos::portable_*archive
//	u_char =>
//	[LuxCore][2.466] ImageMap saved: 1372 Kbytes
//	[LuxCore][2.466] ImageMap save time: 2.41233 secs
//	[LuxCore][3.852] ImageMap load time: 1.38616 secs
//	half =>
//	[LuxCore][10.418] ImageMap saved: 14796 Kbytes
//	[LuxCore][10.418] ImageMap save time: 6.42068 secs
//	[LuxCore][14.886] ImageMap load time: 4.46732 secs
//	float =>
//	[LuxCore][19.325] ImageMap saved: 28400 Kbytes
//	[LuxCore][19.325] ImageMap save time: 4.25468 secs
//	[LuxCore][21.552] ImageMap load time: 2.22647 secs

	// Create the image map file
	{
		SLG_LOG("Create an image map");
		ImageMap *imgMap = ImageMap::AllocImageMap(3, 4096, 4096,
				ImageMapConfig(1.f,
					storageType,
					ImageMapStorage::WrapType::REPEAT,
					ImageMapStorage::ChannelSelectionType::DEFAULT));
		// Write some data
		for (u_int y = 0; y < imgMap->GetHeight(); ++y)
			for (u_int x = 0; x < imgMap->GetWidth(); ++x)
				imgMap->GetStorage()->SetFloat(x, y, 1.f);

		// Write the scene
		SLG_LOG("Write the image map");

		const double t1 = WallClockTime();
		SerializationOutputFile sof("imgmap.bin");
		sof.GetArchive() << imgMap;
		if(!sof.IsGood())
			throw runtime_error("Error while saving image map: imgmap.bin"); 
		
		sof.Flush();
		SLG_LOG("ImageMap saved: " << (sof.GetPosition() / 1024) << " Kbytes");
		SLG_LOG("ImageMap save time: " << (WallClockTime() - t1) << " secs");

		delete imgMap;
	}

	// Read the image map
	SLG_LOG("Read the image map");

	const double t1 = WallClockTime();
	SerializationInputFile sif("imgmap.bin");
	
	ImageMap *imgMapCopy;
	sif.GetArchive() >> imgMapCopy;
	if(!sif.IsGood())
		throw runtime_error("Error while reading image map: imgmap.bin");

	SLG_LOG("ImageMap load time: " << (WallClockTime() - t1) << " secs");

	// Check data
	for (u_int y = 0; y < imgMapCopy->GetHeight(); ++y) {
		for (u_int x = 0; x < imgMapCopy->GetWidth(); ++x) {
			const float data = imgMapCopy->GetStorage()->GetFloat(x, y);
			const float expectedValue = 1.f;

			if (data != expectedValue)
				throw runtime_error("Error while checking image map (" +
						ToString(x) + ", " + ToString(y) +
						") pixel: " + ToString(data) + " (expected value: " + ToString(expectedValue) + ")");
		}
	}

	delete imgMapCopy;
}

static void TestSceneSerialization() {
	// Create the scene file
	{
		SLG_LOG("Create a scene");
		//Scene scene("scenes/cornell/cornell.scn");
		Scene scene(Properties("scenes/cat/cat.scn"));

		// Write the scene
		SLG_LOG("Write the scene");
		Scene::SaveSerialized("scene.bsc", &scene);
	}

	// Read the scene
	SLG_LOG("Read the scene");
	unique_ptr<Scene> sceneCopy(Scene::LoadSerialized("scene.bsc"));
}

static void TestRenderConfigSerialization() {
	// Create the render configuration
	{
		SLG_LOG("Create a render configuration");
		RenderConfig renderConfig(Properties("scenes/cornell/cornell.cfg"));
		//Scene scene("scenes/cat/cat.cfg");

		// Write render configuration
		SLG_LOG("Write the render configuration");
		RenderConfig::SaveSerialized("renderconfig.bcf", &renderConfig);
	}

	// Read the scene
	SLG_LOG("Read the render configuration");
	unique_ptr<RenderConfig> config(RenderConfig::LoadSerialized("renderconfig.bcf"));
}

int main(int argc, char *argv[]) {
	luxcore::Init();

	TestPropertiesSerialization();
	TestFilmSerialization();
	TestImageMapSerialization(ImageMapStorage::StorageType::BYTE);
	TestImageMapSerialization(ImageMapStorage::StorageType::HALF);
	TestImageMapSerialization(ImageMapStorage::StorageType::FLOAT);
	TestSceneSerialization();
	TestRenderConfigSerialization();

	SLG_LOG("Done.");

	return EXIT_SUCCESS;
}

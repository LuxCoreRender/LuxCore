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

// This is a test of SLG internal serialization. The code used here is not part of
// LuxCore API and should be ignored aside from LuxRender core developers.
//
// Note: work in progress

#include <fstream>
#include <memory>

#include "luxcore/luxcore.h"

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"
//#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void TestFilmSerialization() {
	// Create a film
	LC_LOG("Create a film");

	Film film(512, 512);
	film.oclEnable = false;
	film.AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film.AddChannel(Film::IMAGEPIPELINE);

	ImagePipeline *ip = new ImagePipeline();
//	ip->AddPlugin(new AutoLinearToneMap());
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
	LC_LOG("Write the film");
	Film::SaveSerialized("film.flm", &film);

	// Read the film
	LC_LOG("Read the film");
	auto_ptr<Film> filmCopy(Film::LoadSerialized("film.flm"));
	filmCopy->oclEnable = false;
	
	filmCopy->ExecuteImagePipeline(0);
	filmCopy->Output("film-copy.png", FilmOutputs::RGB_IMAGEPIPELINE);
}

//void TestSceneSerialization() {
//	// Create a film
//	LC_LOG("Create a scene");
//	
//	Scene scene("scenes/luxball/luxball-hdr.cfg");
//	
//	// Write the scene
//	LC_LOG("Write the scene");
//	{
//		ofstream outFile;
//		outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
//
//		outFile.open("scene.bsc");
//
//		boost::archive::binary_oarchive outArchive(outFile);
//
//		outArchive << scene;
//	}
//
//	// Read the scene
//	LC_LOG("Read the scene");
//	Scene sceneCopy;
//	{
//		ifstream inFile;
//		inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
//
//		inFile.open("scene.bsc");
//
//		boost::archive::binary_iarchive inArchive(inFile);
//
//		inArchive >> sceneCopy;
//	}
//}

int main(int argc, char *argv[]) {
	luxcore::Init();

	cout << "LuxCore " << LUXCORE_VERSION_MAJOR << "." << LUXCORE_VERSION_MINOR << "\n" ;

	TestFilmSerialization();
	//TestSceneSerialization();

	LC_LOG("Done.");

	return EXIT_SUCCESS;
}

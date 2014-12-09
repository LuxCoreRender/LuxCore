/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

// Note: work in progress

#include <fstream>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "luxcore/luxcore.h"

#include "slg/film/film.h"
#include "slg/film/tonemap.h"
#include "slg/film/imagepipelineplugins.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void TestFilmSerialization() {
	// Create a film
	LC_LOG("Create a film");

	Film film(512, 512);
	film.AddChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	film.AddChannel(Film::RGB_TONEMAPPED);

	ImagePipeline *ip = new ImagePipeline();
	ip->AddPlugin(new AutoLinearToneMap());
	ip->AddPlugin(new GammaCorrectionPlugin());
	film.SetImagePipeline(ip);

	film.Init();

	for (u_int y = 0; y < 512; ++y) {
		for (u_int x = 0; x < 512; ++x) {
			const float rgb[3] = { x / 512.f, y /512.f, 0.f };
			film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[0]->AddWeightedPixel(x, y, rgb, 1.f);
		}
	}
	
	film.ExecuteImagePipeline();
	film.Output(FilmOutputs::RGB_TONEMAPPED, "film-orig.png");

	// Write the film
	LC_LOG("Write the film");
	{
		ofstream outFile;
		outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);

		outFile.open("film.flm");

		boost::archive::binary_oarchive outArchive(outFile);

		outArchive << film;
	}

	// Read the film
	LC_LOG("Read the film");
	slg::Film filmCopy;
	{
		ifstream inFile;
		inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);

		inFile.open("film.flm");

		boost::archive::binary_iarchive inArchive(inFile);

		inArchive >> filmCopy;
	}
	
	filmCopy.Output(FilmOutputs::RGB_TONEMAPPED, "film-copy.png");
}

int main(int argc, char *argv[]) {
	luxcore::Init();

	cout << "LuxCore " << LUXCORE_VERSION_MAJOR << "." << LUXCORE_VERSION_MINOR << "\n" ;

	TestFilmSerialization();

	LC_LOG("Done.");

	return EXIT_SUCCESS;
}

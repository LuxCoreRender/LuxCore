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

#include <memory>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Scene serialization
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Scene)

Scene *Scene::LoadSerialized(const std::string &fileName) {
	BOOST_IFSTREAM inFile;
	inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
	inFile.open(fileName.c_str(), BOOST_IFSTREAM::binary);

	// Create an input filtering stream
	boost::iostreams::filtering_istream inStream;

	// Enable compression
	inStream.push(boost::iostreams::gzip_decompressor());
	inStream.push(inFile);

	// Use portable archive
	eos::polymorphic_portable_iarchive inArchive(inStream);
	//boost::archive::binary_iarchive inArchive(inStream);

	Scene *scene;
	inArchive >> scene;

	if (!inStream.good())
		throw runtime_error("Error while loading serialized scene: " + fileName);

	return scene;
}

void Scene::SaveSerialized(const std::string &fileName, const Scene *scene) {
	// Serialize the scene
	BOOST_OFSTREAM outFile;
	outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
	outFile.open(fileName.c_str(), BOOST_OFSTREAM::binary);

	const streampos startPosition = outFile.tellp();
	
	// Enable compression
	boost::iostreams::filtering_ostream outStream;
	outStream.push(boost::iostreams::gzip_compressor(4));
	outStream.push(outFile);

	// Use portable archive
	eos::polymorphic_portable_oarchive outArchive(outStream);
	//boost::archive::binary_oarchive outArchive(outStream);

	outArchive << scene;

	if (!outStream.good())
		throw runtime_error("Error while saving serialized scene: " + fileName);

	flush(outStream);

	const streamoff size = outFile.tellp() - startPosition;
	SLG_LOG("Scene saved: " << (size / 1024) << " Kbytes");
}

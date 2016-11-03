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

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "luxrays/luxrays.h"
#include "slg/renderstate.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::RenderState)

RenderState::RenderState(const std::string &tag) : engineTag(tag) {
}

RenderState::~RenderState() {
}

void RenderState::CheckEngineTag(const std::string &tag) {
	if (tag != engineTag)
		throw runtime_error("Wrong engine type in a render state: " + engineTag + " instead of " + tag);
}

RenderState *RenderState::LoadSerialized(const std::string &fileName) {
	ifstream inFile;
	inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
	inFile.open(fileName.c_str());

	// Create an output filtering stream
	boost::iostreams::filtering_istream inStream;

	// Enable compression
	inStream.push(boost::iostreams::gzip_decompressor());
	inStream.push(inFile);

	// Use portable archive
	eos::polymorphic_portable_iarchive inArchive(inStream);
	//boost::archive::binary_iarchive inArchive(inStream);

	RenderState *renderState;
	inArchive >> renderState;

	if (!inStream.good())
		throw runtime_error("Error while loading serialized render state: " + fileName);

	return renderState;
}

void RenderState::SaveSerialized(const std::string &fileName) {
	SLG_LOG("Saving render state: " << fileName);

	// Serialize the render state
	ofstream outFile;
	outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
	outFile.open(fileName.c_str());

	const streampos startPosition = outFile.tellp();

	// Enable compression
	boost::iostreams::filtering_ostream outStream;
	outStream.push(boost::iostreams::gzip_compressor(4));
	outStream.push(outFile);

	// Use portable archive
	eos::polymorphic_portable_oarchive outArchive(outStream);
	//boost::archive::binary_oarchive outArchive(outStream);

	// The following line is a workaround to a clang bug
	RenderState *state = this;
	outArchive << state;

	if (!outStream.good())
		throw runtime_error("Error while saving serialized render state: " + fileName);

	flush(outStream);

	const streamoff size = outFile.tellp() - startPosition;
	if (size < 1024) {
		SLG_LOG("Render state saved: " << size << " bytes");
	} else {
		SLG_LOG("Render state saved: " << (size / 1024) << " Kbytes");
	}
}

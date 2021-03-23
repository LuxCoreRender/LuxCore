/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/utils/serializationutils.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// SerializationOuputFile
//------------------------------------------------------------------------------

template <class T>
SerializationOutputFile<T>::SerializationOutputFile(const std::string &fileName) :
		outArchive(NULL) {
	outFile.exceptions(boost::filesystem::ofstream::failbit |
			boost::filesystem::ofstream::badbit |
			boost::filesystem::ofstream::eofbit);
	// Force to use C locale
	outFile.imbue(cLocale);

	// The use of boost::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	outFile.open(boost::filesystem::path(fileName),
			boost::filesystem::ofstream::binary | boost::filesystem::ofstream::trunc);

	// Enable compression
	outStream.push(boost::iostreams::gzip_compressor(4));
	outStream.push(outFile);

	// Use portable archive
	outArchive = new T(outStream);
}

template <class T>
SerializationOutputFile<T>::~SerializationOutputFile() {
	delete outArchive;
}

template <class T>
T &SerializationOutputFile<T>::GetArchive() {
	return *outArchive;
}

template <class T>
bool SerializationOutputFile<T>::IsGood() {
	return outStream.good();
}

template <class T>
void SerializationOutputFile<T>::Flush() {
	outStream.flush();
}

template <class T>
std::streampos SerializationOutputFile<T>::GetPosition() {
	return outFile.tellp();
}

//------------------------------------------------------------------------------
// SerializationInputFile
//------------------------------------------------------------------------------

template <class T>
SerializationInputFile<T>::SerializationInputFile(const std::string &fileName) :
		inArchive(NULL) {
	inFile.exceptions(boost::filesystem::ifstream::failbit |
		boost::filesystem::ifstream::badbit |
		boost::filesystem::ifstream::eofbit);

	// The use of boost::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	inFile.open(boost::filesystem::path(fileName),
			boost::filesystem::ifstream::binary);

	// Enable compression
	inStream.push(boost::iostreams::gzip_decompressor());
	inStream.push(inFile);

	// Use portable archive
	inArchive = new T(inStream);
	
}

template <class T>
SerializationInputFile<T>::~SerializationInputFile() {
	delete inArchive;
}

template <class T>
T &SerializationInputFile<T>::GetArchive() {
	return *inArchive;
}

template <class T>
bool SerializationInputFile<T>::IsGood() {
	return inStream.good();
}

//------------------------------------------------------------------------------
// Explicit instantiations
//------------------------------------------------------------------------------

namespace luxrays {
template class SerializationOutputFile<LuxOutputBinArchive>;
template class SerializationInputFile<LuxInputBinArchive>;
template class SerializationOutputFile<LuxOutputTextArchive>;
template class SerializationInputFile<LuxInputTextArchive>;
}

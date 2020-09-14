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

#ifndef _SLG_SERIALIZATIONUTILS_H
#define	_SLG_SERIALIZATIONUTILS_H

#include <string>
#include <deque>
#include <set>
#include <vector>

#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/archive/basic_archive.hpp>

#include <boost/serialization/access.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/level.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/tracking.hpp>
#include <boost/serialization/split_free.hpp>

#include <boost/serialization/deque.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "luxrays/luxrays.h"

namespace luxrays {

typedef boost::archive::binary_oarchive LuxOutputArchive;
typedef boost::archive::binary_iarchive LuxInputArchive;

class SerializationOutputFile {
public:
	SerializationOutputFile(const std::string &fileName);
	virtual ~SerializationOutputFile();

	LuxOutputArchive &GetArchive();

	bool IsGood();
	std::streampos GetPosition();
	void Flush();

private:
	boost::filesystem::ofstream outFile;
	boost::iostreams::filtering_ostream outStream;
	LuxOutputArchive *outArchive;
};

class SerializationInputFile {
public:
	SerializationInputFile(const std::string &fileName);
	virtual ~SerializationInputFile();

	LuxInputArchive &GetArchive();

	bool IsGood();

private:
	boost::filesystem::ifstream inFile;
	boost::iostreams::filtering_istream inStream;
	LuxInputArchive *inArchive;
};

}

#endif	/* _SLG_SERIALIZATIONUTILS_H */

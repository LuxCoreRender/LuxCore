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

#ifndef _SLG_SERIALIZATIONUTILS_H
#define	_SLG_SERIALIZATIONUTILS_H

#include <string>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "luxrays/luxrays.h"

namespace luxrays {

class SerializationOuputFile {
public:
	SerializationOuputFile(const std::string &fileName);
	virtual ~SerializationOuputFile();

	eos::polymorphic_portable_oarchive &GetArchive();

	bool IsGood();
	std::streampos GetPosition();
	void Flush();

private:
	BOOST_OFSTREAM outFile;
	boost::iostreams::filtering_ostream outStream;
	eos::polymorphic_portable_oarchive *outArchive;
};

class SerializationInputFile {
public:
	SerializationInputFile(const std::string &fileName);
	virtual ~SerializationInputFile();

	eos::polymorphic_portable_iarchive &GetArchive();

	bool IsGood();

private:
	BOOST_IFSTREAM inFile;
	boost::iostreams::filtering_istream inStream;
	eos::polymorphic_portable_iarchive *inArchive;
};

}

#endif	/* _SLG_SERIALIZATIONUTILS_H */

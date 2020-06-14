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

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>

#include "luxrays/utils/safesave.h"
#include "luxrays/utils/fileext.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// SafeSave
//------------------------------------------------------------------------------

SafeSave::SafeSave(const string &name) {
	fileName = name;
	// Keep the file extension, it can be useful for some case (like
	// OpenImageIO, in order to keep the file format)
	// Note: GetFileNamePath() already return a "/" at the end if needed
	fileNameTmp = GetFileNamePath(name) + ToString(boost::uuids::random_generator()()) + GetFileNameExt(name);
}

SafeSave::~SafeSave() {
}

void SafeSave::Process() const {
	const string fileNameCopy = fileName + ".bak";

	// Check if the file already exists
	if (boost::filesystem::exists(fileName)) {
		// Check if there is an old copy
		if (boost::filesystem::exists(fileNameCopy)) {
			// Erase the old copy
			boost::filesystem::remove(fileNameCopy);
		}

		// Rename the new copy
		boost::filesystem::rename(fileName, fileNameCopy);
	}

	// Rename the temporary file to file name
	boost::filesystem::rename(fileNameTmp, fileName);
}

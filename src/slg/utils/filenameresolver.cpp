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

#include <boost/filesystem.hpp>

#include "luxrays/utils/proputils.h"
#include "slg/utils/filenameresolver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FileNameResolver
//------------------------------------------------------------------------------

FileNameResolver::FileNameResolver() {
}

FileNameResolver::~FileNameResolver() {
}

void FileNameResolver::Clear() {
	filePaths.clear();
}

void FileNameResolver::Print() {
	SLG_LOG("File Name Resolver Configuration: ");
	for (auto filePath : filePaths)
		SLG_LOG("  " << filePath);
}

string FileNameResolver::ResolveFile(const std::string &fileName) {
	//SLG_LOG("Try to resolve file name: " << fileName);

	boost::filesystem::path path(fileName);

	//SLG_LOG("  Is absolute: " << path.is_absolute());
	//SLG_LOG("  Exists: " << boost::filesystem::exists(path));
	
	if (!path.is_absolute()) {
		// It is a relative path

		if (!boost::filesystem::exists(path)) {
			// File doesn't exist
			
			// Check if the complete file name exists in all paths
			//SLG_LOG("    Checking complete: ");
			for (auto dir : filePaths) {
				// Check all possible appending paths
				path = boost::filesystem::path(dir) / boost::filesystem::path(fileName);
				//SLG_LOG("    Checking: " << path.generic_string());
				
				if (boost::filesystem::exists(path)) {
					//SLG_LOG("    Found: " << path.generic_string());
					return path.generic_string();
				}
			}

			// Check if the file name alone exists in all paths
			//SLG_LOG("    Checking alone: ");
			for (auto dir : filePaths) {
				// Check all possible appending paths
				path = boost::filesystem::path(dir) / boost::filesystem::path(fileName).filename();
				//SLG_LOG("    Checking: " << path.generic_string());
				
				if (boost::filesystem::exists(path)) {
					//SLG_LOG("    Found: " << path.generic_string());
					return path.generic_string();
				}
			}
		}
	}

	return fileName;
}

void FileNameResolver::AddPath(const std::string &filePath) {
	filePaths.push_back(filePath);
}

void FileNameResolver::AddFileNamePath(const std::string &fileName) {
	boost::filesystem::path path(fileName);
	
	AddPath(path.parent_path().generic_string());
}

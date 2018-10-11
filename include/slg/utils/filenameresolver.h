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

#ifndef _SLG_FILENAMERESOLVER_H
#define	_SLG_FILENAMERESOLVER_H

#include <vector>

#include "luxrays/utils/properties.h"
#include "slg/slg.h"

namespace slg {

class FileNameResolver {
public:
	FileNameResolver();
	virtual ~FileNameResolver();

	void Clear();
	void Print();
	const std::vector<std::string> &GetPaths() const { return filePaths; }

	std::string ResolveFile(const std::string &fileName);

	void AddPath(const std::string &filePath);
	void AddFileNamePath(const std::string &fileName);

private:
	std::vector<std::string> filePaths;
};

}

#endif	/* _SLG_FILENAMERESOLVER_H */

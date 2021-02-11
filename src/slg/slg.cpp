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

#include <openvdb/openvdb.h>

#include "luxrays/luxrays.h"
#include "slg/slg.h"
#include "slg/utils/filenameresolver.h"

#include <OpenImageIO/imageio.h>

using namespace std;
using namespace luxrays;
using namespace slg;

namespace slg {
FileNameResolver SLG_FileNameResolver;
}

void slg::Init() {
	luxrays::Init();

	openvdb::initialize();
	
	// Workaround to a bug: https://github.com/OpenImageIO/oiio/issues/1795
	OIIO::attribute ("threads", 1);
	
	SLG_FileNameResolver.Clear();
}

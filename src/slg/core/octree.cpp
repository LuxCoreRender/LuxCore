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

#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"
#include "slg/core/octree.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Octree
//------------------------------------------------------------------------------

namespace slg {
	
template <> void Octree<Point>::DebugExport(const std::string &fileName,
		const float sphereRadius) const {
	Properties prop;

	prop <<
			Property("scene.materials.octree_material.type")("matte") <<
			Property("scene.materials.octree_material.kd")("0.75 0.75 0.75");

	vector<Point> allData;
	GetAllData(allData);
	
	for (u_int i = 0; i < allData.size(); ++i) {
		prop <<
			Property("scene.objects.octree_data_" + ToString(i) + ".material")("octree_material") <<
			Property("scene.objects.octree_data_" + ToString(i) + ".ply")("scenes/simple/sphere.ply") <<
			Property("scene.objects.octree_data_" + ToString(i) + ".transformation")(Matrix4x4(
				sphereRadius, 0.f, 0.f, allData[i].x,
				0.f, sphereRadius, 0.f, allData[i].y,
				0.f, 0.f, sphereRadius, allData[i].z,
				0.f, 0.f, 0.f, 1.f));
	}

	prop.Save(fileName);
}

}

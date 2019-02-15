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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "luxrays/core/bvh/bvhbuild.h"
#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/engines/caches/photongi/photongicache.h"
#include "slg/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void CompiledScene::CompilePhotonGI() {
	if (!photonGICache) {
		pgicRadiancePhotons.clear();
		pgicRadiancePhotons.shrink_to_fit();

		pgicRadiancePhotonsBVHArrayNode.clear();
		pgicRadiancePhotonsBVHArrayNode.shrink_to_fit();

		return;
	}
	
	wasPhotonGICompiled = true;
	pgicDebugType = (u_int)photonGICache->GetDebugType();
	
	if (photonGICache->IsIndirectEnabled()) {
		SLG_LOG("Compile PhotonGI indirect cache");

		// Compile radiance photons

		const std::vector<RadiancePhoton> &radiancePhotons = photonGICache->GetRadiancePhotons();
		pgicRadiancePhotons.resize(radiancePhotons.size());
		for (u_int i = 0; i < radiancePhotons.size(); ++i) {
			const RadiancePhoton &radiancePhoton = radiancePhotons[i];
			slg::ocl::RadiancePhoton &oclRadiancePhoton = pgicRadiancePhotons[i];
			
			ASSIGN_VECTOR(oclRadiancePhoton.p, radiancePhoton.p);
			ASSIGN_NORMAL(oclRadiancePhoton.n, radiancePhoton.n);
			ASSIGN_SPECTRUM(oclRadiancePhoton.outgoingRadiance, radiancePhoton.outgoingRadiance);
		}

		// Compile radiance photons BVH

		const PGICRadiancePhotonBvh *radiancePhotonsBVH = photonGICache->GetRadiancePhotonsBVH();

		u_int nNodes;
		const slg::ocl::IndexBVHArrayNode *nodes = radiancePhotonsBVH->GetArrayNodes(&nNodes);
		pgicRadiancePhotonsBVHArrayNode.resize(nNodes);
		copy(&nodes[0], &nodes[0] + nNodes, pgicRadiancePhotonsBVHArrayNode.begin());

		pgicIndirectLookUpRadius2 = radiancePhotonsBVH->GetEntryRadius() * radiancePhotonsBVH->GetEntryRadius();
		pgicIndirectLookUpNormalCosAngle = radiancePhotonsBVH->GetEntryNormalCosAngle();
	} else {
		pgicRadiancePhotonsBVHArrayNode.clear();
		pgicRadiancePhotonsBVHArrayNode.shrink_to_fit();		
	}
}

#endif

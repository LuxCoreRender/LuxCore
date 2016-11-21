/***************************************************************************
 * Copyright 1998-2016 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

#include "slg/textures/densitygrid/densitygrid.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void CompiledScene::CompileDensityGrids() {
	SLG_LOG("Compile Densitygrids");
	wasDensityGridsCompiled = true;

	densityGridDescs.resize(0);
	densityGridMemBlocks.resize(0);

	//--------------------------------------------------------------------------
	// Translate density grids
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	vector<const DensityGrid *> dgs;
	scene->densityGridCache.GetDensityGrids(dgs);

	densityGridDescs.resize(dgs.size());
	for (u_int i = 0; i < dgs.size(); ++i) {
		const DensityGrid *dg = dgs[i];
		slg::ocl::DensityGrid *dgd = &densityGridDescs[i];

		const u_int voxelCount = dg->GetNx() * dg->GetNy() * dg->GetNz();
		const size_t memSize = RoundUp(dg->GetStorage()->GetMemorySize(), sizeof(float));

		if (memSize > maxMemPageSize)
			throw runtime_error("An density grid is too big to fit in a single block of memory");

		bool found = false;
		u_int page;
		for (u_int j = 0; j < densityGridMemBlocks.size(); ++j) {
			// Check if it fits in this page
			if (memSize + densityGridMemBlocks[j].size() * sizeof(float) <= maxMemPageSize) {
				found = true;
				page = j;
				break;
			}
		}

		if (!found) {
			// Check if I can add a new page
			if (densityGridMemBlocks.size() > 8)
				throw runtime_error("More than 8 blocks of memory are required for density grids");

			// Add a new page
			densityGridMemBlocks.push_back(vector<float>());
			page = densityGridMemBlocks.size() - 1;
		}

		vector<float> &densityGridMemBlock = densityGridMemBlocks[page];

		dgd->nx = dg->GetNx();
		dgd->ny = dg->GetNy();
		dgd->nz = dg->GetNz();		
		dgd->wrapMode = dg->GetWrapMode();
		dgd->pageIndex = page;
		dgd->voxelsIndex = (u_int)densityGridMemBlock.size();

		//Debug info
		//SLG_LOG("DensityGrid " << i << ":");
		//SLG_LOG("   Dimensions: " << dgd->nx << " x " << dgd->ny << " x " << dgd->nz);
		//SLG_LOG("   PageIndex: " << dgd->pageIndex);
		//SLG_LOG("   Voxelsindex: " << dgd->voxelsIndex);
		//SLG_LOG("   WrapMode: " << dgd->wrapMode);

		// Copy the density grid data
		const size_t start = densityGridMemBlock.size();
		const size_t dataSize = voxelCount * sizeof(float);
		const size_t dataSizeInFloat = RoundUp(dataSize, sizeof(float)) / sizeof(float);
		densityGridMemBlock.resize(start + dataSizeInFloat);

		memcpy(&densityGridMemBlock[start], &(dg->GetStorage()->GetVoxelsData()[0]), dataSize);		
	}

	SLG_LOG("Density grids page(s) count: " << densityGridMemBlocks.size());
	for (u_int i = 0; i < densityGridMemBlocks.size(); ++i)
		SLG_LOG(" Density page " << i << " size: " << densityGridMemBlocks[i].size() * sizeof(float) / 1024 << "Kbytes");

	const double tEnd = WallClockTime();
	SLG_LOG("Density grids compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif

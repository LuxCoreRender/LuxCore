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

#include <vector>

#include "luxrays/core/randomgen.h"
#include "slg/utils/harlequincolors.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// HarlequinColor
//------------------------------------------------------------------------------

namespace slg {

// Must be power of 2
#define HARLEQUIN_TABLE_SIZE 128

static vector<Spectrum> InitHarlequinColorsTable(const u_int size) {
	vector<Spectrum> table(size);
	
	for (u_int i = 0; i < size; i++) {
		Spectrum &c = table[i];
		
		c.c[0] = RadicalInverse(i + 1, 2);
		c.c[1] = RadicalInverse(i + 1, 3);
		c.c[2] = RadicalInverse(i + 1, 5);
	}

	return table;
}

static vector<Spectrum> HarlequinColorsTable = InitHarlequinColorsTable(HARLEQUIN_TABLE_SIZE);

const Spectrum &GetHarlequinColorByIndex(const u_int v) {
	return HarlequinColorsTable[v & (HARLEQUIN_TABLE_SIZE - 1)];
}

const Spectrum &GetHarlequinColorByAddr(const u_longlong v) {
	// I assume the addresd is 8 bytes aligned
	const u_longlong index = (v & ((HARLEQUIN_TABLE_SIZE - 1) << 3)) >> 3;
	
	return HarlequinColorsTable[index];
}

}

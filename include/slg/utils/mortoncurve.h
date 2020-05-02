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

#ifndef _SLG_MORTONCURVE_H
#define	_SLG_MORTONCURVE_H

#include "luxrays/utils/utils.h"

namespace slg {

//------------------------------------------------------------------------------
// Morton related functions
//------------------------------------------------------------------------------

// Morton decode from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// Inverse of Part1By1 - "delete" all odd-indexed bits

inline u_int Compact1By1(u_int x) {
	x &= 0x55555555;					// x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
	x = (x ^ (x >> 1)) & 0x33333333;	// x = --fe --dc --ba --98 --76 --54 --32 --10
	x = (x ^ (x >> 2)) & 0x0f0f0f0f;	// x = ---- fedc ---- ba98 ---- 7654 ---- 3210
	x = (x ^ (x >> 4)) & 0x00ff00ff;	// x = ---- ---- fedc ba98 ---- ---- 7654 3210
	x = (x ^ (x >> 8)) & 0x0000ffff;	// x = ---- ---- ---- ---- fedc ba98 7654 3210
	return x;
}

// Inverse of Part1By2 - "delete" all bits not at positions divisible by 3

inline u_int Compact1By2(u_int x) {
	x &= 0x09249249;					// x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	x = (x ^ (x >> 2)) & 0x030c30c3;	// x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x >> 4)) & 0x0300f00f;	// x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x >> 8)) & 0xff0000ff;	// x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x >> 16)) & 0x000003ff;	// x = ---- ---- ---- ---- ---- --98 7654 3210
	return x;
}

inline u_int DecodeMorton2X(const u_int code) {
	return Compact1By1(code >> 0);
}

inline u_int DecodeMorton2Y(const u_int code) {
	return Compact1By1(code >> 1);
}


}

#endif	/* _SLG_MORTONCURVE_H */

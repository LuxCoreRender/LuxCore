#line 2 "mortoncurve_funcs.cl"

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

//------------------------------------------------------------------------------
// Morton related functions
//------------------------------------------------------------------------------

// Morton decode from https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/

// Inverse of Part1By1 - "delete" all odd-indexed bits

OPENCL_FORCE_INLINE uint Compact1By1(uint x) {
	x &= 0x55555555;					// x = -f-e -d-c -b-a -9-8 -7-6 -5-4 -3-2 -1-0
	x = (x ^ (x >> 1)) & 0x33333333;	// x = --fe --dc --ba --98 --76 --54 --32 --10
	x = (x ^ (x >> 2)) & 0x0f0f0f0f;	// x = ---- fedc ---- ba98 ---- 7654 ---- 3210
	x = (x ^ (x >> 4)) & 0x00ff00ff;	// x = ---- ---- fedc ba98 ---- ---- 7654 3210
	x = (x ^ (x >> 8)) & 0x0000ffff;	// x = ---- ---- ---- ---- fedc ba98 7654 3210
	return x;
}

// Inverse of Part1By2 - "delete" all bits not at positions divisible by 3

OPENCL_FORCE_INLINE uint Compact1By2(uint x) {
	x &= 0x09249249;					// x = ---- 9--8 --7- -6-- 5--4 --3- -2-- 1--0
	x = (x ^ (x >> 2)) & 0x030c30c3;	// x = ---- --98 ---- 76-- --54 ---- 32-- --10
	x = (x ^ (x >> 4)) & 0x0300f00f;	// x = ---- --98 ---- ---- 7654 ---- ---- 3210
	x = (x ^ (x >> 8)) & 0xff0000ff;	// x = ---- --98 ---- ---- ---- ---- 7654 3210
	x = (x ^ (x >> 16)) & 0x000003ff;	// x = ---- ---- ---- ---- ---- --98 7654 3210
	return x;
}

OPENCL_FORCE_INLINE uint DecodeMorton2X(const uint code) {
	return Compact1By1(code >> 0);
}

OPENCL_FORCE_INLINE uint DecodeMorton2Y(const uint code) {
	return Compact1By1(code >> 1);
}

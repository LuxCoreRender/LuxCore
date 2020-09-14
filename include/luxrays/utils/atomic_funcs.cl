#line 2 "atomic_funcs.cl"

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

OPENCL_FORCE_INLINE void AtomicAdd(__global float *val, const float delta) {
#if defined(LUXRAYS_OPENCL_DEVICE)
	union {
		float f;
		uint i;
	} oldVal;
	union {
		float f;
		uint i;
	} newVal;

	do {
		oldVal.f = *val;
		newVal.f = oldVal.f + delta;
	} while (atomic_cmpxchg((__global uint *)val, oldVal.i, newVal.i) != oldVal.i);
#elif defined (LUXRAYS_CUDA_DEVICE)
	atomicAdd(val, delta);
#else
#error "Error in AtomicAdd() definition"
#endif
}

OPENCL_FORCE_INLINE bool AtomicMin(__global float *val, const float val2) {
	union {
		float f;
		uint i;
	} oldVal;
	union {
		float f;
		uint i;
	} newVal;

	do {
		oldVal.f = *val;
		if (val2 >= oldVal.f)
			return false;
		else
			newVal.f = val2;
	} while (atomic_cmpxchg((__global uint *)val, oldVal.i, newVal.i) != oldVal.i);

	return true;
}

OPENCL_FORCE_INLINE uint AtomicAddMod(__global uint *val, const uint delta, const uint mod) {
	uint oldVal, newVal;

	do {
		oldVal = *val;
		newVal = (oldVal + delta) % mod;
	} while (atomic_cmpxchg((__global uint *)val, oldVal, newVal) != oldVal);

	return oldVal;
}

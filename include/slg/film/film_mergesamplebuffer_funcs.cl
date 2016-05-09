#line 2 "film_mergesamplebuffer_funcs.cl"

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

//------------------------------------------------------------------------------
// Film_ClearMergeBuffer 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_ClearMergeBuffer(
		const uint filmWidth, const uint filmHeight,
		__global uint *channel_FRAMEBUFFER_MASK) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	channel_FRAMEBUFFER_MASK[gid] = 0;
}

//------------------------------------------------------------------------------
// Film_MergeRADIANCE_PER_PIXEL_NORMALIZED 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_MergeRADIANCE_PER_PIXEL_NORMALIZED(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global uint *channel_FRAMEBUFFER_MASK,
		__global float *mergeBuffer,
		const float scaleR, const float scaleG, const float scaleB) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	__global const float *mergeBufferPixel = &mergeBuffer[gid * 4];
	float r = mergeBufferPixel[0];
	float g = mergeBufferPixel[1];
	float b = mergeBufferPixel[2];
	const float w = mergeBufferPixel[3];

	if (w > 0.f) {
		const float iw = 1.f / w;
		r *= iw;
		g *= iw;
		b *= iw;

		r *= scaleR;
		g *= scaleG;
		b *= scaleB;

		__global uint *mask = &channel_FRAMEBUFFER_MASK[gid];

		__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
		if (*mask) {
			channelBufferPixel[0] += r;
			channelBufferPixel[1] += g;
			channelBufferPixel[2] += b;
		} else {
			channelBufferPixel[0] = r;
			channelBufferPixel[1] = g;
			channelBufferPixel[2] = b;			
		}

		*mask = 1;
	}
}

//------------------------------------------------------------------------------
// Film_MergeRADIANCE_PER_SCREEN_NORMALIZED 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_MergeRADIANCE_PER_SCREEN_NORMALIZED(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global uint *channel_FRAMEBUFFER_MASK,
		__global float *mergeBuffer,
		const float scaleR, const float scaleG, const float scaleB) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	__global const float *mergeBufferPixel = &mergeBuffer[gid * 3];
	float r = mergeBufferPixel[0];
	float g = mergeBufferPixel[1];
	float b = mergeBufferPixel[2];

	if ((r != 0.f) || (g != 0.f) || (b != 0.f)) {
		r *= scaleR;
		g *= scaleG;
		b *= scaleB;

		__global uint *mask = &channel_FRAMEBUFFER_MASK[gid];

		__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
		if (*mask) {
			channelBufferPixel[0] += r;
			channelBufferPixel[1] += g;
			channelBufferPixel[2] += b;
		} else {
			channelBufferPixel[0] = r;
			channelBufferPixel[1] = g;
			channelBufferPixel[2] = b;			
		}

		*mask = 1;
	}
}

//------------------------------------------------------------------------------
// Film_MergeRADIANCE_PER_SCREEN_NORMALIZED 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_NotOverlappedScreenBufferUpdate(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global uint *channel_FRAMEBUFFER_MASK) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
	__global uint *mask = &channel_FRAMEBUFFER_MASK[gid];
	if (!(*mask)) {
		channelBufferPixel[0] = 0.f;
		channelBufferPixel[1] = 0.f;
		channelBufferPixel[2] = 0.f;			
	}
}

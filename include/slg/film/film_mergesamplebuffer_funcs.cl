#line 2 "film_mergesamplebuffer_funcs.cl"

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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expixelRess or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

//------------------------------------------------------------------------------
// Film_MergeBufferInitialize 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_MergeBufferInitialize(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
	// I use INFINITY to mark pixel with no samples
	channelBufferPixel[0] = INFINITY;
	channelBufferPixel[1] = INFINITY;
	channelBufferPixel[2] = INFINITY;
}

//------------------------------------------------------------------------------
// Film_MergeRADIANCE_PER_PIXEL_NORMALIZED 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_MergeRADIANCE_PER_PIXEL_NORMALIZED(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global float *mergeBuffer,
		const float scaleR, const float scaleG, const float scaleB) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	__global const float *mergeBufferPixel = &mergeBuffer[gid * 4];
	const float w = mergeBufferPixel[3];

	if (w > 0.f) {
		float r = mergeBufferPixel[0];
		float g = mergeBufferPixel[1];
		float b = mergeBufferPixel[2];

		const float iw = 1.f / w;
		r *= iw;
		g *= iw;
		b *= iw;

		r *= scaleR;
		g *= scaleG;
		b *= scaleB;

		__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
		const float pixelR = channelBufferPixel[0];
		const float pixelG = channelBufferPixel[1];
		const float pixelB = channelBufferPixel[2];

		channelBufferPixel[0] = isinf(pixelR) ? r : (pixelR + r);
		channelBufferPixel[1] = isinf(pixelG) ? g : (pixelG + g);
		channelBufferPixel[2] = isinf(pixelB) ? b : (pixelB + b);
	}
}

//------------------------------------------------------------------------------
// Film_MergeRADIANCE_PER_SCREEN_NORMALIZED 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_MergeRADIANCE_PER_SCREEN_NORMALIZED(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
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

		__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
		const float pixelR = channelBufferPixel[0];
		const float pixelG = channelBufferPixel[1];
		const float pixelB = channelBufferPixel[2];

		channelBufferPixel[0] = isinf(pixelR) ? r : (pixelR + r);
		channelBufferPixel[1] = isinf(pixelG) ? g : (pixelG + g);
		channelBufferPixel[2] = isinf(pixelB) ? b : (pixelB + b);
	}
}

//------------------------------------------------------------------------------
// Film_MergeBufferfinalize 
//------------------------------------------------------------------------------

__kernel __attribute__((work_group_size_hint(256, 1, 1))) void Film_MergeBufferFinalize(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE) {
	const size_t gid = get_global_id(0);
	if (gid > filmWidth * filmHeight)
		return;

	__global float *channelBufferPixel = &channel_IMAGEPIPELINE[gid * 3];
	const float pixelR = channelBufferPixel[0];
	const float pixelG = channelBufferPixel[1];
	const float pixelB = channelBufferPixel[2];

	// I use INFINITY to mark pixel with no samples and I need now to replace
	// all INFINITY with 0.0
	if (isinf(pixelR))
		channelBufferPixel[0] = 0.f;
	if (isinf(pixelG))
		channelBufferPixel[1] = 0.f;
	if (isinf(pixelB))
		channelBufferPixel[2] = 0.f;
}

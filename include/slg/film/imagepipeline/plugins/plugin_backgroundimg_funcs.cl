#line 2 "plugin_backgroundimg_funcs.cl"

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
// BackgroundImgPlugin_Apply
//------------------------------------------------------------------------------

__kernel void BackgroundImgPlugin_Apply(
		const uint filmWidth, const uint filmHeight,
		__global float *channel_IMAGEPIPELINE,
		__global float *channel_ALPHA,
		__global const ImageMap *imageMapDesc,
		__global const float *imageMapBuff) {
	const size_t gid = get_global_id(0);
	if (gid >= filmWidth * filmHeight)
		return;

	// Check if the pixel has received any sample
	if (!isinf(channel_IMAGEPIPELINE[gid * 3])) {
		const uint x = gid % filmWidth;
		const uint y = gid / filmWidth;

		const uint s = x;
		// Need to flip the along the Y axis for the image
		const uint t = filmHeight - y - 1;
		const float3 backgroundPixel = ImageMap_GetTexel_Spectrum(imageMapDesc->storageType,
				imageMapBuff, imageMapDesc->width, imageMapDesc->height,
				imageMapDesc->channelCount, WRAP_REPEAT, s, t);

		__global float *alphaPixel = &channel_ALPHA[gid * 2];
		const float alpha = alphaPixel[0] / alphaPixel[1];

		__global float *pixel = &channel_IMAGEPIPELINE[gid * 3];
		pixel[0] = mix(backgroundPixel.s0, pixel[0], alpha);
		pixel[1] = mix(backgroundPixel.s1, pixel[1], alpha);
		pixel[2] = mix(backgroundPixel.s2, pixel[2], alpha);
	}
}

#line 2 "imagemap_funcs.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
// ImageMaps support
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)

__global const void *ImageMap_GetPixelsAddress(__global const float* restrict* restrict imageMapBuff,
		const uint page, const uint offset) {
	return &imageMapBuff[page][offset];
}

float ImageMap_GetTexel_Float(
		const ImageMapStorageType storageType,
		__global const void *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = v * width + u;

	switch (storageType) {
		case BYTE: {
			switch (channelCount) {
				case 1: {
					const uchar a = ((__global const uchar *)pixels)[index];
					return a * (1.f / 255.f);
				}
				case 2: {
					const uchar a = ((__global const uchar *)pixels)[index * 2];
					return a * (1.f / 255.f);
				}
				case 3: {
					__global const uchar *rgb = &((__global const uchar *)pixels)[index * 3];
					return Spectrum_Y((float3)(rgb[0] * (1.f / 255.f), rgb[1] * (1.f / 255.f), rgb[2] * (1.f / 255.f)));
				}
				case 4: {
					__global const uchar *rgba = &((__global const uchar *)pixels)[index * 4];
					return Spectrum_Y((float3)(rgba[0] * (1.f / 255.f), rgba[1] * (1.f / 255.f), rgba[2] * (1.f / 255.f)));
				}
				default:
					return 0.f;
			}
		}
		case HALF: {
			switch (channelCount) {
				case 1: {
					return vload_half(index, (__global const half *)pixels);
				}
				case 2: {
					return vload_half(index * 2, (__global const half *)pixels);
				}				
				case 3: {
					return Spectrum_Y((float3)(
							vload_half(index * 3, (__global const half *)pixels),
							vload_half(index * 3 + 1, (__global const half *)pixels),
							vload_half(index * 3 + 2, (__global const half *)pixels)));
				}
				case 4: {
					return Spectrum_Y((float3)(
							vload_half(index * 4, (__global const half *)pixels),
							vload_half(index * 4 + 1, (__global const half *)pixels),
							vload_half(index * 4 + 2, (__global const half *)pixels)));
				}
				default:
					return 0.f;
			}
		}
		case FLOAT: {
			switch (channelCount) {
				case 1: {
					const float a = ((__global const float *)pixels)[index];
					return a;
				}
				case 2: {
					const float a = ((__global const float *)pixels)[index * 2];
					return a;
				}
				case 3: {
					__global const float *rgb = &((__global const float *)pixels)[index * 3];
					return Spectrum_Y((float3)(rgb[0], rgb[1], rgb[2]));
				}
				case 4: {
					__global const float *rgba = &((__global const float *)pixels)[index * 4];
					return Spectrum_Y((float3)(rgba[0], rgba[1], rgba[2]));
				}
				default:
					return 0.f;
			}
		}
		default:
			return 0.f;
	}
}

float3 ImageMap_GetTexel_Spectrum(
		const ImageMapStorageType storageType,
		__global const void *pixels,
		const uint width, const uint height, const uint channelCount,
		const int s, const int t) {
	const uint u = Mod(s, width);
	const uint v = Mod(t, height);

	const uint index = v * width + u;

	switch (storageType) {
		case BYTE: {
			switch (channelCount) {
				case 1: {
					const uchar a = ((__global const uchar *)pixels)[index];
					return a * (1.f / 255.f);
				}
				case 2: {
					const uchar a = ((__global const uchar *)pixels)[index * 2] * (1.f / 255.f);
					return a;
				}
				case 3: {
					__global const uchar *rgb = &((__global const uchar *)pixels)[index * 3];
					return (float3)(rgb[0] * (1.f / 255.f), rgb[1] * (1.f / 255.f), rgb[2] * (1.f / 255.f));
				}
				case 4: {
					__global const uchar *rgba = &((__global const uchar *)pixels)[index * 4];
					return (float3)(rgba[0] * (1.f / 255.f), rgba[1] * (1.f / 255.f), rgba[2] * (1.f / 255.f));
				}
				default:
					return 0.f;
			}
		}
		case HALF: {
			switch (channelCount) {
				case 1: {
					return vload_half(index, (__global const half *)pixels);
				}
				case 2: {
					return vload_half(index * 2, (__global const half *)pixels);
				}
				case 3: {
					return (float3)(
							vload_half(index * 3, (__global const half *)pixels),
							vload_half(index * 3 + 1, (__global const half *)pixels),
							vload_half(index * 3 + 2, (__global const half *)pixels));
				}
				case 4: {
					return (float3)(
							vload_half(index * 4, (__global const half *)pixels),
							vload_half(index * 4 + 1, (__global const half *)pixels),
							vload_half(index * 4 + 2, (__global const half *)pixels));
				}
				default:
					return 0.f;
			}
		}
		case FLOAT: {
			switch (channelCount) {
				case 1: {
					const float a = ((__global const float *)pixels)[index];
					return a;
				}
				case 2: {
					const float a = ((__global const float *)pixels)[index * 2];
					return a;
				}
				case 3: {
					__global const float *rgb = &((__global const float *)pixels)[index * 3];
					return (float3)(rgb[0], rgb[1], rgb[2]);
				}
				case 4: {
					__global const float *rgba = &((__global const float *)pixels)[index * 4];
					return (float3)(rgba[0], rgba[1], rgba[2]);
				}
				default:
					return 0.f;
			}
		}
		default:
			return 0.f;
	}
}

float ImageMap_GetFloat(__global const ImageMap *imageMap,
		const float u, const float v
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);
	const ImageMapStorageType storageType = imageMap->storageType;
	const uint channelCount = imageMap->channelCount;
	const uint width = imageMap->width;
	const uint height = imageMap->height;

	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float c0 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, s0, t0);
	const float c1 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, s0, t0 + 1);
	const float c2 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, s0 + 1, t0);
	const float c3 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

float3 ImageMap_GetSpectrum(__global const ImageMap *imageMap,
		const float u, const float v
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->pageIndex, imageMap->pixelsIndex);
	const ImageMapStorageType storageType = imageMap->storageType;
	const uint channelCount = imageMap->channelCount;
	const uint width = imageMap->width;
	const uint height = imageMap->height;

	const float s = u * width - 0.5f;
	const float t = v * height - 0.5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float3 c0 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, s0, t0);
	const float3 c1 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, s0, t0 + 1);
	const float3 c2 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, s0 + 1, t0);
	const float3 c3 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

#endif

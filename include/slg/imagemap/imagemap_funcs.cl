#line 2 "imagemap_funcs.cl"

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
// ImageMaps support
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE __global const void *ImageMap_GetPixelsAddress(__global const float* restrict* restrict imageMapBuff,
		const uint page, const uint offset) {
	return &imageMapBuff[page][offset];
}

OPENCL_FORCE_INLINE uint ImageMap_GetTexel_Coords(
		const uint width, const uint height,
		const uint channelCount,
		const ImageWrapType wrapType,
		const int s, const int t, float *fixedColor) {
	uint u, v;
	switch (wrapType) {
		case WRAP_REPEAT:
			u = Mod(s, width);
			v = Mod(t, height);
			break;
		case WRAP_BLACK:
			if ((s < 0) || (s >= width) || (t < 0) || (t >= height)) {
				*fixedColor = 0.f;
				return NULL_INDEX;
			}

			u = s;
			v = t;
			break;
		case WRAP_WHITE:
			if ((s < 0) || (s >= width) || (t < 0) || (t >= height)) {
				*fixedColor = 1.f;
				return NULL_INDEX;
			}

			u = s;
			v = t;
			break;
		case WRAP_CLAMP:
			u = clamp(s, 0, (int)width - 1);
			v = clamp(t, 0, (int)height - 1);
			break;
		default:
			return 0.f;
	}

	return v * width + u;
}

OPENCL_FORCE_INLINE float ImageMap_GetTexel_FloatValue(
		const ImageMapStorageType storageType,
		__global const void *pixels,
		const uint channelCount,
		const uint index) {
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
					return Spectrum_Y(MAKE_FLOAT3(rgb[0] * (1.f / 255.f), rgb[1] * (1.f / 255.f), rgb[2] * (1.f / 255.f)));
				}
				case 4: {
					__global const uchar *rgba = &((__global const uchar *)pixels)[index * 4];
					return Spectrum_Y(MAKE_FLOAT3(rgba[0] * (1.f / 255.f), rgba[1] * (1.f / 255.f), rgba[2] * (1.f / 255.f)));
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
					return Spectrum_Y(MAKE_FLOAT3(
							vload_half(index * 3, (__global const half *)pixels),
							vload_half(index * 3 + 1, (__global const half *)pixels),
							vload_half(index * 3 + 2, (__global const half *)pixels)));
				}
				case 4: {
					return Spectrum_Y(MAKE_FLOAT3(
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
					return Spectrum_Y(MAKE_FLOAT3(rgb[0], rgb[1], rgb[2]));
				}
				case 4: {
					__global const float *rgba = &((__global const float *)pixels)[index * 4];
					return Spectrum_Y(MAKE_FLOAT3(rgba[0], rgba[1], rgba[2]));
				}
				default:
					return 0.f;
			}
		}
		default:
			return 0.f;
	}
}

OPENCL_FORCE_INLINE float3 ImageMap_GetTexel_SpectrumValue(
		const ImageMapStorageType storageType,
		__global const void *pixels,
		const uint channelCount,
		const uint index) {
	switch (storageType) {
		case BYTE: {
			switch (channelCount) {
				case 1: {
					const float a = ((__global const uchar *)pixels)[index] * (1.f / 255.f);
					return TO_FLOAT3(a);
				}
				case 2: {
					const uchar a = ((__global const uchar *)pixels)[index * 2] * (1.f / 255.f);
					return TO_FLOAT3(a);
				}
				case 3: {
					__global const uchar *rgb = &((__global const uchar *)pixels)[index * 3];
					return MAKE_FLOAT3(rgb[0] * (1.f / 255.f), rgb[1] * (1.f / 255.f), rgb[2] * (1.f / 255.f));
				}
				case 4: {
					__global const uchar *rgba = &((__global const uchar *)pixels)[index * 4];
					return MAKE_FLOAT3(rgba[0] * (1.f / 255.f), rgba[1] * (1.f / 255.f), rgba[2] * (1.f / 255.f));
				}
				default:
					return BLACK;
			}
		}
		case HALF: {
			switch (channelCount) {
				case 1: {
					const float a = vload_half(index, (__global const half *)pixels);
					return TO_FLOAT3(a);
				}
				case 2: {
					const float a = vload_half(index * 2, (__global const half *)pixels);
					return TO_FLOAT3(a);
				}
				case 3: {
					return MAKE_FLOAT3(
							vload_half(index * 3, (__global const half *)pixels),
							vload_half(index * 3 + 1, (__global const half *)pixels),
							vload_half(index * 3 + 2, (__global const half *)pixels));
				}
				case 4: {
					return MAKE_FLOAT3(
							vload_half(index * 4, (__global const half *)pixels),
							vload_half(index * 4 + 1, (__global const half *)pixels),
							vload_half(index * 4 + 2, (__global const half *)pixels));
				}
				default:
					return BLACK;
			}
		}
		case FLOAT: {
			switch (channelCount) {
				case 1: {
					const float a = ((__global const float *)pixels)[index];
					return TO_FLOAT3(a);
				}
				case 2: {
					const float a = ((__global const float *)pixels)[index * 2];
					return TO_FLOAT3(a);
				}
				case 3: {
					__global const float *rgb = &((__global const float *)pixels)[index * 3];
					return MAKE_FLOAT3(rgb[0], rgb[1], rgb[2]);
				}
				case 4: {
					__global const float *rgba = &((__global const float *)pixels)[index * 4];
					return MAKE_FLOAT3(rgba[0], rgba[1], rgba[2]);
				}
				default:
					return BLACK;
			}
		}
		default:
			return BLACK;
	}
}

OPENCL_FORCE_INLINE float ImageMap_GetTexel_Float(
		const ImageMapStorageType storageType,
		__global const void *pixels,
		const uint width, const uint height, const uint channelCount,
		const ImageWrapType wrapType,
		const int s, const int t) {
	//--------------------------------------------------------------------------
	// This code was ImageMap_GetTexel_Coords() but has been inlined as
	// optimization
	//--------------------------------------------------------------------------

	uint u, v;
	switch (wrapType) {
		case WRAP_REPEAT:
			u = Mod(s, width);
			v = Mod(t, height);
			break;
		case WRAP_BLACK:
			if ((s < 0) || (s >= width) || (t < 0) || (t >= height))
				return 0.f;

			u = s;
			v = t;
			break;
		case WRAP_WHITE:
			if ((s < 0) || (s >= width) || (t < 0) || (t >= height))
				return 1.f;

			u = s;
			v = t;
			break;
		case WRAP_CLAMP:
			u = clamp(s, 0, (int)width - 1);
			v = clamp(t, 0, (int)height - 1);
			break;
		default:
			return 0.f;
	}

	const uint index = v * width + u;
	
	//--------------------------------------------------------------------------

	return ImageMap_GetTexel_FloatValue(storageType, pixels, channelCount, index);
}

OPENCL_FORCE_INLINE float3 ImageMap_GetTexel_Spectrum(
		const ImageMapStorageType storageType,
		__global const void *pixels,
		const uint width, const uint height, const uint channelCount,
		const ImageWrapType wrapType,
		const int s, const int t) {
	//--------------------------------------------------------------------------
	// This code was ImageMap_GetTexel_Coords() but has been inlined as
	// optimization
	//--------------------------------------------------------------------------

	uint u, v;
	switch (wrapType) {
		case WRAP_REPEAT:
			u = Mod(s, width);
			v = Mod(t, height);
			break;
		case WRAP_BLACK:
			if ((s < 0) || (s >= width) || (t < 0) || (t >= height))
				return BLACK;

			u = s;
			v = t;
			break;
		case WRAP_WHITE:
			if ((s < 0) || (s >= width) || (t < 0) || (t >= height))
				return WHITE;

			u = s;
			v = t;
			break;
		case WRAP_CLAMP:
			u = clamp(s, 0, (int)width - 1);
			v = clamp(t, 0, (int)height - 1);
			break;
		default:
			return BLACK;
	}

	const uint index = v * width + u;
	
	//--------------------------------------------------------------------------

	return ImageMap_GetTexel_SpectrumValue(storageType, pixels, channelCount, index);
}

OPENCL_FORCE_NOT_INLINE float ImageMapStorage_GetFloat(__global const ImageMap *imageMap,
		const uint s0, const uint t0 
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->genericAddr.pageIndex, imageMap->genericAddr.pixelsIndex);
	const ImageMapStorageType storageType = imageMap->desc.storageType;
	const uint channelCount = imageMap->desc.channelCount;
	const uint width = imageMap->desc.width;
	const uint height = imageMap->desc.height;
	const ImageWrapType wrapType = imageMap->desc.wrapType;

	return ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0, t0);	
}

OPENCL_FORCE_NOT_INLINE float3 ImageMapStorage_GetSpectrum(__global const ImageMap *imageMap,
		const uint s0, const uint t0
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->genericAddr.pageIndex, imageMap->genericAddr.pixelsIndex);
	const ImageMapStorageType storageType = imageMap->desc.storageType;
	const uint channelCount = imageMap->desc.channelCount;
	const uint width = imageMap->desc.width;
	const uint height = imageMap->desc.height;
	const ImageWrapType wrapType = imageMap->desc.wrapType;

	return ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, wrapType, s0, t0);	
}

OPENCL_FORCE_NOT_INLINE float ImageMap_GetFloat(__global const ImageMap *imageMap,
		const float u, const float v
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->genericAddr.pageIndex, imageMap->genericAddr.pixelsIndex);
	const ImageMapStorageType storageType = imageMap->desc.storageType;
	const uint channelCount = imageMap->desc.channelCount;
	const uint width = imageMap->desc.width;
	const uint height = imageMap->desc.height;
	const ImageWrapType wrapType = imageMap->desc.wrapType;

	const float s = u * width - .5f;
	const float t = v * height - .5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float c0 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0, t0);
	const float c1 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0, t0 + 1);
	const float c2 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0 + 1, t0);
	const float c3 = ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

OPENCL_FORCE_NOT_INLINE float3 ImageMap_GetSpectrum(__global const ImageMap *imageMap,
		const float u, const float v
		IMAGEMAPS_PARAM_DECL) {
//	const float texVal = tex2D<float>(imageMap->cudaAddr.texObj, u, v);
//	return MAKE_FLOAT3(texVal, texVal, texVal);
	
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->genericAddr.pageIndex, imageMap->genericAddr.pixelsIndex);
	const ImageMapStorageType storageType = imageMap->desc.storageType;
	const uint channelCount = imageMap->desc.channelCount;
	const uint width = imageMap->desc.width;
	const uint height = imageMap->desc.height;
	const ImageWrapType wrapType = imageMap->desc.wrapType;

	const float s = u * width - .5f;
	const float t = v * height - .5f;

	const int s0 = Floor2Int(s);
	const int t0 = Floor2Int(t);

	const float ds = s - s0;
	const float dt = t - t0;

	const float ids = 1.f - ds;
	const float idt = 1.f - dt;

	const float3 c0 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, wrapType, s0, t0);
	const float3 c1 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, wrapType, s0, t0 + 1);
	const float3 c2 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, wrapType, s0 + 1, t0);
	const float3 c3 = ImageMap_GetTexel_Spectrum(storageType, pixels, width, height, channelCount, wrapType, s0 + 1, t0 + 1);

	const float k0 = ids * idt;
	const float k1 = ids * dt;
	const float k2 = ds * idt;
	const float k3 = ds * dt;

	return (k0 * c0 + k1 *c1 + k2 * c2 + k3 * c3);
}

OPENCL_FORCE_NOT_INLINE float2 ImageMap_GetDuv(__global const ImageMap *imageMap,
		const float u, const float v
		IMAGEMAPS_PARAM_DECL) {
	__global const void *pixels = ImageMap_GetPixelsAddress(
		imageMapBuff, imageMap->genericAddr.pageIndex, imageMap->genericAddr.pixelsIndex);
	const ImageMapStorageType storageType = imageMap->desc.storageType;
	const uint channelCount = imageMap->desc.channelCount;
	const uint width = imageMap->desc.width;
	const uint height = imageMap->desc.height;
	const ImageWrapType wrapType = imageMap->desc.wrapType;

	const float s = u * width;
	const float t = v * height;

	const int is = Floor2Int(s);
	const int it = Floor2Int(t);

	const float as = s - is;
	const float at = t - it;

	int s0, s1;
	if (as < .5f) {
		s0 = is - 1;
		s1 = is;
	} else {
		s0 = is;
		s1 = is + 1;
	}
	int t0, t1;
	if (at < .5f) {
		t0 = it - 1;
		t1 = it;
	} else {
		t0 = it;
		t1 = it + 1;
	}

	float2 duv;
	duv.x = mix(ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s1, it) - ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0, it),
		ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s1, it + 1) - ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, s0, it + 1), at) * width;
	duv.y = mix(ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, is, t1) - ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, is, t0),
		ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, is + 1, t1) - ImageMap_GetTexel_Float(storageType, pixels, width, height, channelCount, wrapType, is + 1, t0), as) * height;

	return duv;
}

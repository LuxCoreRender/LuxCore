#line 2 "texture_blender_funcs.cl"

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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef TEXTURE_STACK_SIZE
#define TEXTURE_STACK_SIZE 16
#endif

//------------------------------------------------------------------------------
// Blender blend texture
//------------------------------------------------------------------------------
 
#if defined(PARAM_ENABLE_BLENDER_BLEND)
 
OPENCL_FORCE_INLINE float BlenderBlendTexture_Evaluate(__global const HitPoint *hitPoint,
		const ProgressionType type, const bool direction,
 		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);
 
	float result = 0.f;
	float x, y, t;
     
	if(!direction) {
		//horizontal
		x = P.x;
		y = P.y;
	} else {
		//vertical
		x = P.y;
		y = P.x;
	};
 
    if (type == TEX_LIN) { /* lin */
		result = (1.f + x) / 2.f;
	} else if (type == TEX_QUAD) { /* quad */
		result = (1.f + x) / 2.f;
		if (result < 0.f) result = 0.f;
		else result *= result;
	} else if (type == TEX_EASE) { /* ease */
		result = (1.f + x) / 2.f;
        if (result <= 0.f) result = 0.f;
        else if (result >= 1.f) result = 1.f;
        else {
			t = result * result;
            result = (3.f * t - 2.f * t * result);
        }
    } else if (type == TEX_DIAG) { /* diag */
		result = (2.f + x + y) / 4.f;
    } else if (type == TEX_RAD) { /* radial */
        result = (atan2(y, x) / (2.f * M_PI_F) + 0.5f);
    } else { /* sphere TEX_SPHERE */
        result = 1.f - sqrt(x * x + y * y + P.z * P.z);
        if (result < 0.f) result = 0.f;
	    if (type == TEX_HALO) result *= result; /* halo */
    }

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;

	return result;
}

OPENCL_FORCE_NOT_INLINE float BlenderBlendTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const ProgressionType type, const bool direction,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderBlendTexture_Evaluate(hitPoint, type, direction, contrast, bright, mapping);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderBlendTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const ProgressionType type, const bool direction,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderBlendTexture_Evaluate(hitPoint, type, direction, contrast, bright, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_CLOUDS)

OPENCL_FORCE_INLINE float BlenderCloudsTexture_Evaluate(__global const HitPoint *hitPoint,
		const BlenderNoiseBasis noisebasis, const float noisesize, const int noisedepth,
		const float contrast, const float bright, const bool hard, __global const TextureMapping3D *mapping) {
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);

	float clouds = BLI_gTurbulence(noisesize, P.x, P.y, P.z, noisedepth, hard, noisebasis);

	clouds = (clouds - 0.5f) * contrast + bright - 0.5f;
	clouds = clamp(clouds, 0.f, 1.f);

	return clouds;
}

OPENCL_FORCE_NOT_INLINE float BlenderCloudsTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const BlenderNoiseBasis noisebasis, const float noisesize, const int noisedepth,
		const float contrast, const float bright, const bool hard, __global const TextureMapping3D *mapping) {
	return BlenderCloudsTexture_Evaluate(hitPoint, noisebasis, noisesize, noisedepth,
			contrast, bright, hard, mapping);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderCloudsTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const BlenderNoiseBasis noisebasis, const float noisesize, const int noisedepth,
		const float contrast, const float bright, const bool hard, __global const TextureMapping3D *mapping) {
	return BlenderCloudsTexture_Evaluate(hitPoint, noisebasis, noisesize, noisedepth,
			contrast, bright, hard, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender distorted noise texture
//------------------------------------------------------------------------------
            
#if defined(PARAM_ENABLE_BLENDER_DISTORTED_NOISE)

OPENCL_FORCE_INLINE float BlenderDistortedNoiseTexture_Evaluate(__global const HitPoint *hitPoint,
		const BlenderNoiseBasis noisedistortion, const BlenderNoiseBasis noisebasis, const float distortion, const float noisesize,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	float3 P = TextureMapping3D_Map(mapping, hitPoint);

	float result = 0.f;
	float scale = 1.f;

	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);
	P = scale*P;
	result = mg_VLNoise(P.x, P.y, P.z, distortion, noisebasis, noisedistortion);

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;

	return result;
}

OPENCL_FORCE_NOT_INLINE float BlenderDistortedNoiseTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const BlenderNoiseBasis noisedistortion, const BlenderNoiseBasis noisebasis, const float distortion, const float noisesize,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderDistortedNoiseTexture_Evaluate(hitPoint, noisedistortion, noisebasis, distortion, noisesize,
			contrast, bright, mapping);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderDistortedNoiseTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const BlenderNoiseBasis noisedistortion, const BlenderNoiseBasis noisebasis, const float distortion, const float noisesize,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderDistortedNoiseTexture_Evaluate(hitPoint, noisedistortion, noisebasis, distortion, noisesize,
			contrast, bright, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender magic texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_MAGIC)
OPENCL_FORCE_INLINE float3 BlenderMagicTexture_Evaluate(__global const HitPoint *hitPoint, const int noisedepth, const float turbulence, 
			const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);
	float3 s;

    float x, y, z, turb = 1.f;
    float r, g, b;
    int n;

    n = noisedepth;
    turb = turbulence / 5.f;

    x = sin((P.x + P.y + P.z)*5.f);
    y = cos((-P.x + P.y - P.z)*5.f);
    z = -cos((-P.x - P.y + P.z)*5.f);
    if (n > 0) {
        x *= turb;
        y *= turb;
        z *= turb;
        y = -cos(x - y + z);
        y *= turb;
        if (n > 1) {
            x = cos(x - y - z);
            x *= turb;
            if (n > 2) {
                z = sin(-x - y - z);
                z *= turb;
                if (n > 3) {
                    x = -cos(-x + y - z);
                    x *= turb;
                    if (n > 4) {
                        y = -sin(-x + y + z);
                        y *= turb;
                        if (n > 5) {
                            y = -cos(-x + y + z);
                            y *= turb;
                            if (n > 6) {
                                x = cos(x + y + z);
                                x *= turb;
                                if (n > 7) {
                                    z = sin(x + y - z);
                                    z *= turb;
                                    if (n > 8) {
                                        x = -cos(-x - y + z);
                                        x *= turb;
                                        if (n > 9) {
                                            y = -sin(x - y + z);
                                            y *= turb;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (turb != 0.f) {
        turb *= 2.f;
        x /= turb;
        y /= turb;
        z /= turb;
    }
    r = 0.5f - x;
    g = 0.5f - y;
    b = 0.5f - z;

	r = (r - 0.5f) * contrast + bright - 0.5f;
    if(r < 0.f) r = 0.f; 
	else if(r > 1.f) r = 1.f;

	g = (g - 0.5f) * contrast + bright - 0.5f;
    if(g < 0.f) g = 0.f; 
	else if(g > 1.f) g = 1.f;

	b = (b - 0.5f) * contrast + bright - 0.5f;
    if(b < 0.f) b = 0.f; 
	else if(b > 1.f) b = 1.f;

	s.x = r;
	s.y = g;
	s.z = b;

	return s;
}

OPENCL_FORCE_NOT_INLINE float BlenderMagicTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const int noisedepth, const float turbulence,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	float3 result = BlenderMagicTexture_Evaluate(hitPoint, noisedepth, turbulence, contrast, bright, mapping);
	return (0.212671f * result.x + 0.715160f * result.y + 0.072169f * result.z);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderMagicTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const int noisedepth, const float turbulence,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderMagicTexture_Evaluate(hitPoint, noisedepth, turbulence, contrast, bright, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender marble texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_MARBLE)
OPENCL_FORCE_INLINE float BlenderMarbleTexture_Evaluate(__global const HitPoint *hitPoint, const BlenderMarbleType type, 
		const BlenderNoiseBasis noisebasis, const BlenderNoiseBase noisebasis2, const float noisesize,
		const float turbulence, const int noisedepth, const float contrast, const float bright,
		const bool hard, __global const TextureMapping3D *mapping) {
	
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);

	float result = 0.f;
    float n = 5.f * (P.x + P.y + P.z);
    result = n + turbulence * BLI_gTurbulence(noisesize, P.x, P.y, P.z, noisedepth, hard, noisebasis);
	

	if(noisebasis2 == TEX_SIN) {
		result = tex_sin(result);
	} else if(noisebasis2 == TEX_SAW) {
		result = tex_saw(result);
	} else {
		result = tex_tri(result);
	}
	
	if (type == TEX_SHARP) {
		result = sqrt(result);
	} else if (type == TEX_SHARPER) {
		result = sqrt(sqrt(result));
	}

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;
	
    return result;
}

OPENCL_FORCE_NOT_INLINE float BlenderMarbleTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const BlenderMarbleType type,
		const BlenderNoiseBasis noisebasis, const BlenderNoiseBase noisebasis2, const float noisesize, 
		const float turbulence, const int noisedepth, const float contrast, const float bright,
		const bool hard, __global const TextureMapping3D *mapping) {
	return BlenderMarbleTexture_Evaluate(hitPoint, type, noisebasis, noisebasis2, noisesize, turbulence, noisedepth, contrast, bright, hard, mapping);
}


OPENCL_FORCE_NOT_INLINE float3 BlenderMarbleTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const BlenderMarbleType type, 
		const BlenderNoiseBasis noisebasis, const BlenderNoiseBase noisebasis2, const float noisesize, 
		const float turbulence, const int noisedepth, const float contrast, const float bright, 
		const bool hard, __global const TextureMapping3D *mapping) {
	return BlenderMarbleTexture_Evaluate(hitPoint, type, noisebasis, noisebasis2, noisesize, turbulence, noisedepth, contrast, bright, hard, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender musgrave texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_MUSGRAVE)
OPENCL_FORCE_INLINE float BlenderMusgraveTexture_Evaluate(__global const HitPoint *hitPoint, const BlenderMusgraveType type,
		const BlenderNoiseBasis noisebasis, const float dimension, const float intensity, const float lacunarity,
		const float offset, const float gain, const float octaves, const float noisesize,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
		
	float scale = 1.f;
	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);
	const float3 P = scale*TextureMapping3D_Map(mapping, hitPoint);

	float result = 0.f;

	switch (type) {
		case TEX_MULTIFRACTAL:
			result = mg_MultiFractal(P.x, P.y, P.z, dimension, lacunarity, octaves, noisebasis);
			break;
		case TEX_FBM:
			result = mg_fBm(P.x, P.y, P.z, dimension, lacunarity, octaves, noisebasis);
            break;
		case TEX_RIDGED_MULTIFRACTAL:
			result = mg_RidgedMultiFractal(P.x, P.y, P.z, dimension, lacunarity, octaves, offset, gain, noisebasis);
			break;
        case TEX_HYBRID_MULTIFRACTAL:
			result = mg_HybridMultiFractal(P.x, P.y, P.z, dimension, lacunarity, octaves, offset, gain, noisebasis);
			break;
        case TEX_HETERO_TERRAIN:
			result = mg_HeteroTerrain(P.x, P.y, P.z, dimension, lacunarity, octaves, offset, noisebasis);
			break;
    };

	result *= intensity;
           
	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;
	
	return result;
}

OPENCL_FORCE_NOT_INLINE float BlenderMusgraveTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const BlenderMusgraveType type,
		const BlenderNoiseBasis noisebasis, const float dimension, const float intensity, const float lacunarity,
		const float offset, const float gain, const float octaves, const float noisesize,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderMusgraveTexture_Evaluate(hitPoint, type, noisebasis, dimension, intensity, lacunarity,
			offset, gain, octaves, noisesize, contrast, bright, mapping);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderMusgraveTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const BlenderMusgraveType type,
		const BlenderNoiseBasis noisebasis, const float dimension, const float intensity, const float lacunarity,
		const float offset, const float gain, const float octaves, const float noisesize,
		const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderMusgraveTexture_Evaluate(hitPoint, type, noisebasis, dimension, intensity, lacunarity,
			offset, gain, octaves, noisesize, contrast, bright, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender noise texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_NOISE)

OPENCL_FORCE_INLINE float BlenderNoiseTexture_Evaluate(__global const HitPoint *hitPoint, const int noisedepth,
		const float bright, const float contrast) {
	// The original Blender code was not thread-safe

	float result = 0.f;

	float div = 3.f;

	float ran = hitPoint->passThroughEvent;
	float val = Floor2UInt(ran * 4.f);
	
	int loop = noisedepth;
    while (loop--) {
		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		ran = fabs(ran - .5f) * 2.f;

        val *= Floor2UInt(ran * 4.f);
        div *= 3.f;		
    }

	result = ((float) val) / div;
	
	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

	return result;
}

OPENCL_FORCE_NOT_INLINE float BlenderNoiseTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const int noisedepth,
		const float bright, const float contrast) {
	return BlenderNoiseTexture_Evaluate(hitPoint, noisedepth, bright, contrast);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderNoiseTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const int noisedepth,
		const float bright, const float contrast) {
	return BlenderNoiseTexture_Evaluate(hitPoint, noisedepth, bright, contrast);
}

#endif

//------------------------------------------------------------------------------
// Blender stucci texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_STUCCI)

OPENCL_FORCE_INLINE float BlenderStucciTexture_Evaluate(__global const HitPoint *hitPoint, const BlenderStucciType type,
		const BlenderNoiseBasis noisebasis, const float noisesize, const float turbulence, const float contrast,
		const float bright, const bool hard, __global const TextureMapping3D *mapping) {
		
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);
	
	float result = 0.f;
//	neo2068: only nor[2] is used, so normal float variable is sufficient
//	float nor[3], b2, ofs;
	float b2, ofs;
	
	b2 = BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis);
	ofs = turbulence / 200.f;

	if (type != TEX_PLASTIC) ofs *= (b2 * b2);
//	neo2068: only nor[2] is used
//    nor[0] = BLI_gNoise(noisesize, P.x + ofs, P.y, P.z, hard, noisebasis);
//    nor[1] = BLI_gNoise(noisesize, P.x, P.y + ofs, P.z, hard, noisebasis);
//    nor[2] = BLI_gNoise(noisesize, P.x, P.y, P.z + ofs, hard, noisebasis);
    result = BLI_gNoise(noisesize, P.x, P.y, P.z + ofs, hard, noisebasis);

//    result = nor[2];

    if (type == TEX_WALL_OUT)
        result = 1.f - result;

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;

	return result;
}

OPENCL_FORCE_NOT_INLINE float BlenderStucciTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint, const BlenderStucciType type,
		const BlenderNoiseBasis noisebasis, const float noisesize, const float turbulence, const float contrast,
		const float bright, const bool hard, __global const TextureMapping3D *mapping) {
	return BlenderStucciTexture_Evaluate(hitPoint, type, noisebasis, noisesize, turbulence,
			contrast, bright, hard, mapping);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderStucciTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const BlenderStucciType type,
		const BlenderNoiseBasis noisebasis, const float noisesize, const float turbulence, const float contrast,
		const float bright, const bool hard, __global const TextureMapping3D *mapping) {
	return BlenderStucciTexture_Evaluate(hitPoint, type, noisebasis, noisesize, turbulence,
			contrast, bright, hard, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender wood texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_WOOD)

OPENCL_FORCE_INLINE float BlenderWoodTexture_Evaluate(__global const HitPoint *hitPoint,
		const BlenderWoodType type, const BlenderNoiseBase noisebasis2,
		const BlenderNoiseBasis noisebasis, const float noisesize, const float turbulence,
		const float contrast, const float bright, const bool hard,
		__global const TextureMapping3D *mapping) {
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);

	float wood = 0.f;
	switch(type) {
		default:
		case BANDS:
			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f);
			}
			break;
		case RINGS:
			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			}
			break;
		case BANDNOISE:			
			if(hard)	
				wood = turbulence * fabs(2.f * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis) - 1.f);
			else
				wood = turbulence * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis);

			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f + wood);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f + wood);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f + wood);
			}
			break;
		case RINGNOISE:
			if(hard)	
				wood = turbulence * fabs(2.f * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis) - 1.f);
			else
				wood = turbulence * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis);

			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			}
			break;
	}
	wood = (wood - 0.5f) * contrast + bright - 0.5f;
	wood = clamp(wood, 0.f, 1.f);

	return wood;
}

OPENCL_FORCE_NOT_INLINE float BlenderWoodTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const BlenderWoodType type, const BlenderNoiseBase noisebasis2,
		const BlenderNoiseBasis noisebasis, const float noisesize, const float turbulence,
		const float contrast, const float bright, const bool hard,
		__global const TextureMapping3D *mapping) {
	return BlenderWoodTexture_Evaluate(hitPoint, type, noisebasis2, noisebasis,
		noisesize, turbulence, contrast, bright, hard, mapping);
}

OPENCL_FORCE_NOT_INLINE float3 BlenderWoodTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const BlenderWoodType type, const BlenderNoiseBase noisebasis2, 
		const BlenderNoiseBasis noisebasis, const float noisesize, const float turbulence,
		const float contrast, const float bright, const bool hard,
		__global const TextureMapping3D *mapping) {
    return BlenderWoodTexture_Evaluate(hitPoint, type, noisebasis2, noisebasis,
		noisesize, turbulence, contrast, bright, hard, mapping);
}

#endif

//------------------------------------------------------------------------------
// Blender voronoi texture
//------------------------------------------------------------------------------
 
#if defined(PARAM_ENABLE_BLENDER_VORONOI)
 
OPENCL_FORCE_INLINE float BlenderVoronoiTexture_Evaluate(__global const HitPoint *hitPoint,
		const DistanceMetric distancemetric, const float feature_weight1, const float feature_weight2, 
		const float feature_weight3, const float feature_weight4, const float noisesize, const float intensity,
		const float exponent, const float contrast, const float bright, __global const TextureMapping3D *mapping) {

    float da[4], pa[12]; /* distance and point coordinate arrays of 4 nearest neighbours */
	float scale = 1.f;
	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);
	const float3 P = scale*TextureMapping3D_Map(mapping, hitPoint);
 
	const float aw1 = fabs(feature_weight1);
    const float aw2 = fabs(feature_weight2);
    const float aw3 = fabs(feature_weight3);
    const float aw4 = fabs(feature_weight4);

    float sc = (aw1 + aw2 + aw3 + aw4);

    if (sc > 0.00001f) sc = intensity / sc;

    float result = 1.f;

	voronoi(P.x, P.y, P.z, da, pa, exponent, distancemetric);
    result = sc * fabs(feature_weight1 * da[0] + feature_weight2 * da[1] + feature_weight3 * da[2] + feature_weight4 * da[3]);

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;

    return result;
}
		
OPENCL_FORCE_NOT_INLINE float BlenderVoronoiTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,	const DistanceMetric distancemetric, const float feature_weight1, 
		const float feature_weight2, const float feature_weight3, const float feature_weight4, const float noisesize, const float intensity,
		const float exponent, const float contrast, const float bright, __global const TextureMapping3D *mapping) {	
	return BlenderVoronoiTexture_Evaluate(hitPoint, distancemetric, feature_weight1, feature_weight2, feature_weight3, feature_weight4,
		noisesize, intensity, exponent, contrast, bright, mapping);
}
 
OPENCL_FORCE_NOT_INLINE float3 BlenderVoronoiTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint, const DistanceMetric distancemetric, const float feature_weight1,
		const float feature_weight2, const float feature_weight3, const float feature_weight4, const float noisesize, const float intensity,
		const float exponent, const float contrast, const float bright, __global const TextureMapping3D *mapping) {
	return BlenderVoronoiTexture_Evaluate(hitPoint, distancemetric, feature_weight1, feature_weight2, feature_weight3, feature_weight4,
		noisesize, intensity, exponent, contrast, bright, mapping);
}
 
#endif

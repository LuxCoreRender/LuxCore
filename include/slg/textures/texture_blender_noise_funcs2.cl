#line 2 "texture_blender_noise_funcs2.cl"

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
// Blender Texture utility functions
//------------------------------------------------------------------------------
/******************/
/* VORONOI/WORLEY */
/******************/

/* distance metrics for voronoi, e parameter only used in Minkovsky */
/* Camberra omitted, didn't seem useful */

/* distance squared */
OPENCL_FORCE_INLINE float dist_Squared(float x, float y, float z, float e) { return (x*x + y*y + z*z); }
/* real distance */
OPENCL_FORCE_INLINE float dist_Real(float x, float y, float z, float e) { return sqrt(x*x + y*y + z*z); }
/* manhattan/taxicab/cityblock distance */
OPENCL_FORCE_INLINE float dist_Manhattan(float x, float y, float z, float e) { return (fabs(x) + fabs(y) + fabs(z)); }
/* Chebychev */
OPENCL_FORCE_INLINE float dist_Chebychev(float x, float y, float z, float e) {
	float t;
	x = fabs(x);
	y = fabs(y);
	z = fabs(z);
	t = (x>y)?x:y;
	return ((z>t)?z:t);
}

/* minkovsky preset exponent 0.5 */
OPENCL_FORCE_INLINE float dist_MinkovskyH(float x, float y, float z, float e) {
	float d = sqrt(fabs(x)) + sqrt(fabs(y)) + sqrt(fabs(z));
	return (d*d);
}

/* minkovsky preset exponent 4 */
OPENCL_FORCE_INLINE float dist_Minkovsky4(float x, float y, float z, float e) {
	x *= x;
	y *= y;
	z *= z;
	return sqrt(sqrt(x*x + y*y + z*z));
}

/* Minkovsky, general case, slow, maybe too slow to be useful */
OPENCL_FORCE_INLINE float dist_Minkovsky(float x, float y, float z, float e) {
	return pow(pow(fabs(x), e) + pow(fabs(y), e) + pow(fabs(z), e), 1.f/e);
}

/* Not 'pure' Worley, but the results are virtually the same.
	 Returns distances in da and point coords in pa */
OPENCL_FORCE_NOT_INLINE void voronoi(float x, float y, float z, float* da, float* pa, float me, DistanceMetric dtype) {
	int xx, yy, zz, xi, yi, zi;
	float xd, yd, zd, d, p0, p1, p2;

	xi = (int)(floor(x));
	yi = (int)(floor(y));
	zi = (int)(floor(z));
	da[0] = da[1] = da[2] = da[3] = 1e10f;
	for (xx=xi-1;xx<=xi+1;xx++) {
		for (yy=yi-1;yy<=yi+1;yy++) {
			for (zz=zi-1;zz<=zi+1;zz++) {
			
				p0 = hashpntf[3*hash[ (hash[ (hash[(zz) & 255]+(yy)) & 255]+(xx)) & 255]];
				p1 = hashpntf[3*hash[ (hash[ (hash[(zz) & 255]+(yy)) & 255]+(xx)) & 255]+1];
				p2 = hashpntf[3*hash[ (hash[ (hash[(zz) & 255]+(yy)) & 255]+(xx)) & 255]+2];
				xd = x - (p0 + xx);
				yd = y - (p1 + yy);
				zd = z - (p2 + zz);
				switch (dtype) {
					case DISTANCE_SQUARED:						
						d = dist_Squared(xd, yd, zd, me);
						break;
					case MANHATTAN:						
						d = dist_Manhattan(xd, yd, zd, me);
						break;
					case CHEBYCHEV:						
						d = dist_Chebychev(xd, yd, zd, me);
						break;
					case MINKOWSKI_HALF:
						d = dist_MinkovskyH(xd, yd, zd, me);
						break;
					case MINKOWSKI_FOUR:						
						d = dist_Minkovsky4(xd, yd, zd, me);
						break;
					case MINKOWSKI:						
						d = dist_Minkovsky(xd, yd, zd, me);
						break;
					case ACTUAL_DISTANCE:
					default:						
						d = dist_Real(xd, yd, zd, me);
				}
				
				if (d<da[0]) {
					da[3]=da[2];  da[2]=da[1];  da[1]=da[0];  da[0]=d;
					pa[9]=pa[6];  pa[10]=pa[7];  pa[11]=pa[8];
					pa[6]=pa[3];  pa[7]=pa[4];  pa[8]=pa[5];
					pa[3]=pa[0];  pa[4]=pa[1];  pa[5]=pa[2];
					pa[0]=p0+xx;  pa[1]=p1+yy;  pa[2]=p2+zz;
				}
				else if (d<da[1]) {
					da[3]=da[2];  da[2]=da[1];  da[1]=d;
					pa[9]=pa[6];  pa[10]=pa[7];  pa[11]=pa[8];
					pa[6]=pa[3];  pa[7]=pa[4];  pa[8]=pa[5];
					pa[3]=p0+xx;  pa[4]=p1+yy;  pa[5]=p2+zz;
				}
				else if (d<da[2]) {
					da[3]=da[2];  da[2]=d;
					pa[9]=pa[6];  pa[10]=pa[7];  pa[11]=pa[8];
					pa[6]=p0+xx;  pa[7]=p1+yy;  pa[8]=p2+zz;
				}
				else if (d<da[3]) {
					da[3]=d;
					pa[9]=p0+xx;  pa[10]=p1+yy;  pa[11]=p2+zz;
				}
			}
		}
	}
}

/* returns different feature points for use in BLI_gNoise() */
OPENCL_FORCE_NOT_INLINE float voronoi_F1(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return da[0];
}

OPENCL_FORCE_NOT_INLINE float voronoi_F2(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return da[1];
}

OPENCL_FORCE_NOT_INLINE float voronoi_F3(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return da[2];
}

OPENCL_FORCE_NOT_INLINE float voronoi_F4(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return da[3];
}

OPENCL_FORCE_NOT_INLINE float voronoi_F1F2(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return (da[1]-da[0]);
}

/* Crackle type pattern, just a scale/clamp of F2-F1 */
OPENCL_FORCE_NOT_INLINE float voronoi_Cr(float x, float y, float z) {
	float t = 10.f*voronoi_F1F2(x, y, z);
	if (t>1.f) return 1.f;
	return t;
}

/* Signed version of all 6 of the above, just 2x-1, not really correct though (range is potentially (0, sqrt(6)).
   Used in the musgrave functions */
OPENCL_FORCE_NOT_INLINE float voronoi_F1S(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return (2.f*da[0]-1.f);
}

OPENCL_FORCE_NOT_INLINE float voronoi_F2S(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return (2.f*da[1]-1.f);
}

OPENCL_FORCE_NOT_INLINE float voronoi_F3S(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return (2.f*da[2]-1.f);
}

OPENCL_FORCE_NOT_INLINE float voronoi_F4S(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return (2.f*da[3]-1.f);
}

OPENCL_FORCE_NOT_INLINE float voronoi_F1F2S(float x, float y, float z) {
	float da[4], pa[12];
	voronoi(x, y, z, da, pa, 1.f, ACTUAL_DISTANCE);
	return (2.f*(da[1]-da[0])-1.f);
}

/* Crackle type pattern, just a scale/clamp of F2-F1 */
OPENCL_FORCE_NOT_INLINE float voronoi_CrS(float x, float y, float z) {
	float t = 10.f*voronoi_F1F2(x, y, z);
	if (t>1.f) return 1.f;
	return (2.f*t-1.f);
}

/***************/
/* voronoi end */
/***************/

/*************/
/* CELLNOISE */
/*************/

/* returns unsigned cellnoise */
OPENCL_FORCE_INLINE float cellNoiseU(float x, float y, float z) {
  int xi = (int)(floor(x));
  int yi = (int)(floor(y));
  int zi = (int)(floor(z));
  unsigned int n = xi + yi*1301 + zi*314159;
  n ^= (n<<13);
  return ((float)(n*(n*n*15731 + 789221) + 1376312589) / 4294967296.f);
}

/* idem, signed */
OPENCL_FORCE_INLINE float cellNoise(float x, float y, float z) {
  return (2.f*cellNoiseU(x, y, z)-1.f);
}

/* returns a vector/point/color in ca, using point hasharray directly */
OPENCL_FORCE_INLINE void cellNoiseV(float x, float y, float z, float *ca) {
	int xi = (int)(floor(x));
	int yi = (int)(floor(y));
	int zi = (int)(floor(z));
	
	ca[0] = hashpntf[3*hash[ (hash[ (hash[(zi) & 255]+(yi)) & 255]+(xi)) & 255]];
	ca[1] = hashpntf[3*hash[ (hash[ (hash[(zi) & 255]+(yi)) & 255]+(xi)) & 255]+1];
	ca[2] = hashpntf[3*hash[ (hash[ (hash[(zi) & 255]+(yi)) & 255]+(xi)) & 255]+2];
}

/*****************/
/* end cellnoise */
/*****************/

OPENCL_FORCE_NOT_INLINE float noisefuncS(BlenderNoiseBasis noisebasis, float x, float y, float z) {
	float result;
	switch (noisebasis) {
		case ORIGINAL_PERLIN:
			result = orgPerlinNoise(x, y, z);
			break;
		case IMPROVED_PERLIN:
			result = newPerlin(x, y, z);
			break;
		case VORONOI_F1:
			result = voronoi_F1S(x, y, z);
			break;
		case VORONOI_F2:
			result = voronoi_F2S(x, y, z);
			break;
		case VORONOI_F3:
			result = voronoi_F3S(x, y, z);
			break;
		case VORONOI_F4:
			result = voronoi_F4S(x, y, z);
			break;
		case VORONOI_F2_F1:
			result = voronoi_F1F2S(x, y, z);
			break;
		case VORONOI_CRACKLE:
			result = voronoi_CrS(x, y, z);
			break;
		case CELL_NOISE:
			result = cellNoise(x, y, z);
			break;
		case BLENDER_ORIGINAL:
		default: {
			result = orgBlenderNoiseS(x, y, z);
		}
	}
	return result;
}

/*
 * The following code is based on Ken Musgrave's explanations and sample
 * source code in the book "Texturing and Modelling: A procedural approach"
 */

/*
 * Procedural fBm evaluated at "point"; returns value stored in "value".
 *
 * Parameters:
 *    ``H''  is the fractal increment parameter
 *    ``lacunarity''  is the gap between successive frequencies
 *    ``octaves''  is the number of frequencies in the fBm
 */
OPENCL_FORCE_NOT_INLINE float mg_fBm(float x, float y, float z, float H, float lacunarity, float octaves, BlenderNoiseBasis noisebasis)
{
	float rmd, value=0.f, pwr=1.f, pwHL=pow(lacunarity, -H);
	int	i;

	for (i=0; i<(int)octaves; i++) {
		value +=noisefuncS(noisebasis, x, y, z)*pwr;
		pwr *= pwHL;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}	

	rmd = octaves - floor(octaves);
	if (rmd!=0.f) value += rmd * noisefuncS(noisebasis, x, y, z) * pwr;

	return value;

} /* fBm() */

/*
 * Procedural multifractal evaluated at "point";
 * returns value stored in "value".
 *
 * Parameters:
 *    ``H''  determines the highest fractal dimension
 *    ``lacunarity''  is gap between successive frequencies
 *    ``octaves''  is the number of frequencies in the fBm
 *    ``offset''  is the zero offset, which determines multifractality (NOT USED?!?)
 */
 /* this one is in fact rather confusing,
 	* there seem to be errors in the original source code (in all three versions of proc.text&mod),
	* I modified it to something that made sense to me, so it might be wrong... */

OPENCL_FORCE_NOT_INLINE float mg_MultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, BlenderNoiseBasis noisebasis) {
	float rmd, value=1.f, pwr=1.f, pwHL=pow(lacunarity, -H);
	int i;
	
	for (i=0; i<(int)octaves; i++) {
		value *= (pwr * noisefuncS(noisebasis, x, y, z) + 1.f);
		pwr *= pwHL;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}	

	rmd = octaves - floor(octaves);
	if (rmd!=0.f) value *= (rmd * noisefuncS(noisebasis, x, y, z) * pwr + 1.f);
	
	return value;

} /* multifractal() */

/*
 * Heterogeneous procedural terrain function: stats by altitude method.
 * Evaluated at "point"; returns value stored in "value".
 *
 * Parameters:
 *       ``H''  determines the fractal dimension of the roughest areas
 *       ``lacunarity''  is the gap between successive frequencies
 *       ``octaves''  is the number of frequencies in the fBm
 *       ``offset''  raises the terrain from `sea level'
 */
OPENCL_FORCE_NOT_INLINE float mg_HeteroTerrain(float x, float y, float z, float H, float lacunarity, float octaves, float offset, BlenderNoiseBasis noisebasis)
{
	float	value, increment, rmd;
	int i;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;	/* starts with i=1 instead of 0 */

	/* first unscaled octave of function; later octaves are scaled */
	value = offset + noisefuncS(noisebasis, x, y, z);

	x *= lacunarity;
	y *= lacunarity;
	z *= lacunarity;	

	for (i=1; i<(int)octaves; i++) {
		increment = (noisefuncS(noisebasis, x, y, z) + offset) * pwr * value;
		value += increment;
		pwr *= pwHL;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}	

	rmd = octaves - floor(octaves);

	if (rmd!=0.f) {
		increment = (noisefuncS(noisebasis, x, y, z) + offset) * pwr * value;
		value += rmd * increment;
	}
	return value;
}

/* Hybrid additive/multiplicative multifractal terrain model.
 *
 * Some good parameter values to start with:
 *
 *      H:           0.25
 *      offset:      0.7
 */
OPENCL_FORCE_NOT_INLINE float mg_HybridMultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, float offset, float gain, BlenderNoiseBasis noisebasis)
{
	float result, signal, weight, rmd;
	int i;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;	/* starts with i=1 instead of 0 */
	
	result = noisefuncS(noisebasis, x, y, z) + offset;
	weight = gain * result;
	x *= lacunarity;
	y *= lacunarity;
	z *= lacunarity;

	for (i=1; (weight>0.001f) && (i<(int)octaves); i++) {
		if (weight>1.f)  weight=1.f;
		signal = (noisefuncS(noisebasis, x, y, z) + offset) * pwr;
		pwr *= pwHL;
		result += weight * signal;
		weight *= gain * signal;
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
	}

	rmd = octaves - floor(octaves);
	if (rmd!=0.f) result += rmd * ((noisefuncS(noisebasis, x, y, z) + offset) * pwr);

	return result;

} /* HybridMultifractal() */


/* Ridged multifractal terrain model.
 *
 * Some good parameter values to start with:
 *
 *      H:           1.0
 *      offset:      1.0
 *      gain:        2.0
 */
OPENCL_FORCE_NOT_INLINE float mg_RidgedMultiFractal(float x, float y, float z, float H, float lacunarity, float octaves, float offset, float gain, BlenderNoiseBasis noisebasis)
{
	float result, signal, weight;
	int	i;
	float pwHL = pow(lacunarity, -H);
	float pwr = pwHL;	/* starts with i=1 instead of 0 */

	signal = offset - fabs(noisefuncS(noisebasis, x, y, z));
	signal *= signal;
	result = signal;
	weight = 1.f;

	for( i=1; i<(int)octaves; i++ ) {
		x *= lacunarity;
		y *= lacunarity;
		z *= lacunarity;
		weight = signal * gain;
		if (weight>1.f) weight=1.f; else if (weight<0.f) weight=0.f;

		signal = offset - fabs(noisefuncS(noisebasis, x, y, z));
		signal *= signal;
		signal *= weight;
		result += signal * pwr;
		pwr *= pwHL;
	}

	return result;
} /* RidgedMultifractal() */

/* "Variable Lacunarity Noise"
 * A distorted variety of Perlin noise.
 */
OPENCL_FORCE_NOT_INLINE float mg_VLNoise(float x, float y, float z, float distortion, BlenderNoiseBasis nbas1, BlenderNoiseBasis nbas2)
{
	float3 rv;
	float result;

	/* get a random vector and scale the randomization */		
	rv.x = noisefuncS(nbas1, x+13.5f, y+13.5f, z+13.5f) * distortion;
	rv.y = noisefuncS(nbas1, x, y, z) * distortion;
	rv.z = noisefuncS(nbas1, x-13.5f, y-13.5f, z-13.5f) * distortion;

	result = noisefuncS(nbas2, x+rv.x, y+rv.y, z+rv.z);

	return result;
}

/****************/
/* musgrave end */
/****************/

/* newnoise: generic noise function for use with different noisebases */
OPENCL_FORCE_NOT_INLINE float BLI_gNoise(float noisesize, float x, float y, float z, int hard, BlenderNoiseBasis noisebasis) {
	float result = 0.f;

	if(noisebasis == BLENDER_ORIGINAL) {
		// add one to make return value same as BLI_hnoise 
		x += 1.f;
		y += 1.f;
		z += 1.f;
	}

	if (noisesize!=0.f) {
		noisesize = 1.f/noisesize;
		x *= noisesize;
		y *= noisesize;
		z *= noisesize;
	}

	switch (noisebasis) {
		case ORIGINAL_PERLIN:
			result = orgPerlinNoiseU(x, y, z);
			break;
		case IMPROVED_PERLIN:
			result = newPerlinU(x, y, z);
			break;
		case VORONOI_F1:
			result = voronoi_F1(x, y, z);
			break;
		case VORONOI_F2:
			result = voronoi_F2(x, y, z);
			break;
		case VORONOI_F3:
			result = voronoi_F3(x, y, z);
			break;
		case VORONOI_F4:
			result = voronoi_F4(x, y, z);
			break;
		case VORONOI_F2_F1:
			result = voronoi_F1F2(x, y, z);
			break;
		case VORONOI_CRACKLE:
			result = voronoi_Cr(x, y, z);
			break;
		case CELL_NOISE:
			result = cellNoiseU(x, y, z);
			break;
		case BLENDER_ORIGINAL:
		default: 			
			result = orgBlenderNoise(x, y, z);		
	}

	if (hard) return fabs(2.f*result-1.f);
	return result;
}

/* newnoise: generic turbulence function for use with different noisebasis */
OPENCL_FORCE_NOT_INLINE float BLI_gTurbulence(float noisesize, float x, float y, float z, int oct, int hard, BlenderNoiseBasis noisebasis) {
	float sum=0.f, t, amp=1.f, fscale=1.f;
	int i;

	if(noisebasis == BLENDER_ORIGINAL) {
		// add one to make return value same as BLI_hnoise 
		x += 1.f;
		y += 1.f;
		z += 1.f;
	}

	if (noisesize!=0.f) {
		noisesize = 1.f/noisesize;
		x *= noisesize;
		y *= noisesize;
		z *= noisesize;
	}

	for (i=0;i<=oct;i++, amp*=0.5f, fscale*=2.f) {
		switch (noisebasis) {
			case ORIGINAL_PERLIN:
				t = orgPerlinNoise(fscale*x, fscale*y, fscale*z);
				break;
			case IMPROVED_PERLIN:
				t = newPerlin(fscale*x, fscale*y, fscale*z);
				break;
			case VORONOI_F1:
				t = voronoi_F1(fscale*x, fscale*y, fscale*z);
				break;
			case VORONOI_F2:
				t = voronoi_F2(fscale*x, fscale*y, fscale*z);
				break;
			case VORONOI_F3:
				t = voronoi_F3(fscale*x, fscale*y, fscale*z);
				break;
			case VORONOI_F4:
				t = voronoi_F4(fscale*x, fscale*y, fscale*z);
				break;
			case VORONOI_F2_F1:
				t = voronoi_F1F2(fscale*x, fscale*y, fscale*z);
				break;
			case VORONOI_CRACKLE:
				t = voronoi_Cr(fscale*x, fscale*y, fscale*z);
				break;
			case CELL_NOISE:
				t = cellNoiseU(fscale*x, fscale*y, fscale*z);
				break;
			case BLENDER_ORIGINAL:
			default: 			
				t = orgBlenderNoise(fscale*x, fscale*y, fscale*z);		
		}

		if (hard) t = fabs(2.f*t-1.f);
		sum += t * amp;
	}

	sum *= (float)(1u << oct) / (float)((1u << (oct + 1)) - 1u);

	return sum;

}

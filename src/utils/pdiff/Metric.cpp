/*
Metric
Copyright (C) 2006 Yangli Hector Yee

This program is free software; you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program;
if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

// Adapted for LuxRender/LuxRays by Dade

#include <cstdio>
#include <math.h>
#include "luxrays/utils/pdiff/Metric.h"
#include "luxrays/utils/pdiff/LPyramid.h"

#ifndef M_PI
#define M_PI 3.14159265f
#endif

using namespace luxrays;
using namespace luxrays::utils;

/*
* Given the adaptation luminance, this function returns the
* threshold of visibility in cd per m^2
* TVI means Threshold vs Intensity function
* This version comes from Ward Larson Siggraph 1997
*/ 

static float tvi(float adaptation_luminance)
{
      // returns the threshold luminance given the adaptation luminance
      // units are candelas per meter squared

      float log_a, r, result; 
      log_a = log10f(adaptation_luminance);

      if (log_a < -3.94f) {
            r = -2.86f;
      } else if (log_a < -1.44f) {
            r = powf(0.405f * log_a + 1.6f , 2.18f) - 2.86f;
      } else if (log_a < -0.0184f) {
            r = log_a - 0.395f;
      } else if (log_a < 1.9f) {
            r = powf(0.249f * log_a + 0.65f, 2.7f) - 0.72f;
      } else {
            r = log_a - 1.255f;
      }

      result = powf(10.0f , r); 

      return result;

} 

// computes the contrast sensitivity function (Barten SPIE 1989)
// given the cycles per degree (cpd) and luminance (lum)
static float csf(float cpd, float lum)
{
	float a, b, result; 
	
	a = 440.0f * powf((1.0f + 0.7f / lum), -0.2f);
	b = 0.3f * powf((1.0f + 100.0f / lum), 0.15f);
		
	result = a * cpd * expf(-b * cpd) * sqrtf(1.0f + 0.06f * expf(b * cpd)); 
	
	return result;	
}

/*
* Visual Masking Function
* from Daly 1993
*/
static float mask(float contrast)
{
      float a, b, result;
      a = powf(392.498f * contrast,  0.7f);
      b = powf(0.0153f * a, 4.0f);
      result = powf(1.0f + b, 0.25f); 

      return result;
} 

// convert Adobe RGB (1998) with reference white D65 to XYZ
static void AdobeRGBToXYZ(float r, float g, float b, float &x, float &y, float &z)
{
	// matrix is from http://www.brucelindbloom.com/
	x = r * 0.576700f + g * 0.185556f + b * 0.188212f;
	y = r * 0.297361f + g * 0.627355f + b * 0.0752847f;
	z = r * 0.0270328f + g * 0.0706879f + b * 0.991248f;
}

static void XYZToLAB(float x, float y, float z, float &L, float &A, float &B)
{
	static float xw = -1;
	static float yw;
	static float zw;
	// reference white
	if (xw < 0) {
		AdobeRGBToXYZ(1, 1, 1, xw, yw, zw);
	}
	const float epsilon  = 216.0f / 24389.0f;
	const float kappa = 24389.0f / 27.0f;
	float f[3];
	float r[3];
	r[0] = x / xw;
	r[1] = y / yw;
	r[2] = z / zw;
	for (int i = 0; i < 3; i++) {
		if (r[i] > epsilon) {
			f[i] = powf(r[i], 1.0f / 3.0f);
		} else {
			f[i] = (kappa * r[i] + 16.0f) / 116.0f;
		}
	}
	L = 116.0f * f[1] - 16.0f;
	A = 500.0f * (f[0] - f[1]);
	B = 200.0f * (f[1] - f[2]);
}

unsigned int luxrays::utils::Yee_Compare(
		const float *rgbA,
		const float *rgbB,
		std::vector<bool> *diff,
		float *tviBuffer,
		const unsigned int width,
		const unsigned int height,
		const bool LuminanceOnly,
		const float FieldOfView,
		const float Gamma,
		const float Luminance,
		const float ColorFactor,
		const unsigned int DownSample)
{
	unsigned int i, dim;
	dim = width * height;
	bool identical = true;
	for (i = 0; i < 3 * dim; i++) {
		if (rgbA[i] != rgbB[i]) {
		  identical = false;
		  break;
		}
	}
	if (identical) {
		// Images are binary identical
		return true;
	}
	
	// assuming colorspaces are in Adobe RGB (1998) convert to XYZ
	float *aX = new float[dim];
	float *aY = new float[dim];
	float *aZ = new float[dim];
	float *bX = new float[dim];
	float *bY = new float[dim];
	float *bZ = new float[dim];
	float *aLum = new float[dim];
	float *bLum = new float[dim];
	
	float *aA = new float[dim];
	float *bA = new float[dim];
	float *aB = new float[dim];
	float *bB = new float[dim];
	
	unsigned int x, y;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			float r, g, b, l;
			i = x + y * width;
			r = powf(rgbA[3 * i], Gamma);
			g = powf(rgbA[3 * i + 1], Gamma);
			b = powf(rgbA[3 * i + 2], Gamma);						
			AdobeRGBToXYZ(r,g,b,aX[i],aY[i],aZ[i]);			
			XYZToLAB(aX[i], aY[i], aZ[i], l, aA[i], aB[i]);
			r = powf(rgbB[3 * i], Gamma);
			g = powf(rgbB[3 * i + 1], Gamma);
			b = powf(rgbB[3 * i + 2], Gamma);						
			AdobeRGBToXYZ(r,g,b,bX[i],bY[i],bZ[i]);
			XYZToLAB(bX[i], bY[i], bZ[i], l, bA[i], bB[i]);
			aLum[i] = aY[i] * Luminance;
			bLum[i] = bY[i] * Luminance;
		}
	}

	// Constructing Laplacian Pyramids
	
	LPyramid *la = new LPyramid(aLum, width, height);
	LPyramid *lb = new LPyramid(bLum, width, height);
	
	float num_one_degree_pixels = (float) (2 * tan(FieldOfView * 0.5 * M_PI / 180) * 180 / M_PI);
	float pixels_per_degree = width / num_one_degree_pixels;
	
	// Performing test
	
	float num_pixels = 1;
	unsigned int adaptation_level = 0;
	for (i = 0; i < MAX_PYR_LEVELS; i++) {
		adaptation_level = i;
		if (num_pixels > num_one_degree_pixels) break;
		num_pixels *= 2;
	}
	
	float cpd[MAX_PYR_LEVELS];
	cpd[0] = 0.5f * pixels_per_degree;
	for (i = 1; i < MAX_PYR_LEVELS; i++) cpd[i] = 0.5f * cpd[i - 1];
	float csf_max = csf(3.248f, 100.0f);
	
	float F_freq[MAX_PYR_LEVELS - 2];
	for (i = 0; i < MAX_PYR_LEVELS - 2; i++) F_freq[i] = csf_max / csf( cpd[i], 100.0f);
	
	unsigned int pixels_failed = 0;
	for (y = 0; y < height; y++) {
	  for (x = 0; x < width; x++) {
		int index = x + y * width;
		float contrast[MAX_PYR_LEVELS - 2];
		float sum_contrast = 0;
		for (i = 0; i < MAX_PYR_LEVELS - 2; i++) {
			float n1 = fabsf(la->Get_Value(x,y,i) - la->Get_Value(x,y,i + 1));
			float n2 = fabsf(lb->Get_Value(x,y,i) - lb->Get_Value(x,y,i + 1));
			float numerator = (n1 > n2) ? n1 : n2;
			float d1 = fabsf(la->Get_Value(x,y,i+2));
			float d2 = fabsf(lb->Get_Value(x,y,i+2));
			float denominator = (d1 > d2) ? d1 : d2;
			if (denominator < 1e-5f) denominator = 1e-5f;
			contrast[i] = numerator / denominator;
			sum_contrast += contrast[i];
		}
		if (sum_contrast < 1e-5) sum_contrast = 1e-5f;
		float F_mask[MAX_PYR_LEVELS - 2];
		float adapt = la->Get_Value(x,y,adaptation_level) + lb->Get_Value(x,y,adaptation_level);
		adapt *= 0.5f;
		if (adapt < 1e-5) adapt = 1e-5f;
		for (i = 0; i < MAX_PYR_LEVELS - 2; i++) {
			F_mask[i] = mask(contrast[i] * csf(cpd[i], adapt)); 
		}
		float factor = 0;
		for (i = 0; i < MAX_PYR_LEVELS - 2; i++) {
			factor += contrast[i] * F_freq[i] * F_mask[i] / sum_contrast;
		}
		if (factor < 1) factor = 1;
		if (factor > 10) factor = 10;
		float delta = fabsf(la->Get_Value(x,y,0) - lb->Get_Value(x,y,0));
		bool pass = true;
		// pure luminance test
		const float tviValue = tvi(adapt);
		if (tviBuffer)
			tviBuffer[x + y * width] = tviValue;
		if (delta > factor * tviValue) {
			pass = false;
		} else if (!LuminanceOnly) {
			// CIE delta E test with modifications
			float color_scale = ColorFactor;
			// ramp down the color test in scotopic regions
			if (adapt < 10.0f) {
				// Don't do color test at all.
				color_scale = 0.0;
			}
			float da = aA[index] - bA[index];
			float db = aB[index] - bB[index];
			da = da * da;
			db = db * db;
			float delta_e = (da + db) * color_scale;
			if (delta_e > factor) {
				pass = false;
			}
		}
		if (!pass) {
			pixels_failed++;

			if (diff)
				(*diff)[index] = false;
		} else {
			if (diff)
				(*diff)[index] = true;
		}
	  }
	}
	
	if (aX) delete[] aX;
	if (aY) delete[] aY;
	if (aZ) delete[] aZ;
	if (bX) delete[] bX;
	if (bY) delete[] bY;
	if (bZ) delete[] bZ;
	if (aLum) delete[] aLum;
	if (bLum) delete[] bLum;
	if (la) delete la;
	if (lb) delete lb;
	if (aA) delete aA;
	if (bA) delete bA;
	if (aB) delete aB;
	if (bB) delete bB;
	
	return pixels_failed;
}

//------------------------------------------------------------------------------
// ConvergenceTest class
//------------------------------------------------------------------------------

ConvergenceTest::ConvergenceTest(const unsigned int w, const unsigned int h) :
				width(w), height(h), reference(NULL) {
}

ConvergenceTest::~ConvergenceTest() {
	delete[] reference;
}

void ConvergenceTest::Reset() {
	delete[] reference;
	reference = NULL;
	
}

void ConvergenceTest::Reset(const unsigned int w, const unsigned int h) {
	width = w;
	height = h;
	delete[] reference;
	reference = NULL;
	
}

unsigned int ConvergenceTest::Test(const float *image) {
	const unsigned int pixelCount = width * height;

	if (reference == NULL) {
		reference = new float[pixelCount * 3];
		std::copy(image, image + pixelCount * 3, reference);
		return pixelCount;
	} else {
		const unsigned int count = Yee_Compare(reference, image, NULL, NULL, width, height);
		std::copy(image, image + pixelCount * 3, reference);
		return count;
	}
}

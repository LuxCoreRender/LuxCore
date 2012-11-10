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

#ifndef _PDIFF_METRIC_H
#define _PDIFF_METRIC_H

#include <vector>

namespace luxrays { namespace utils {

// Image comparison metric using Yee's method
// References: A Perceptual Metric for Production Testing, Hector Yee, Journal of Graphics Tools 2004
extern unsigned int Yee_Compare(
		const float *rgbA,
		const float *rgbB,
		std::vector<bool> *diff,
		float *tviBuffer,
		const unsigned int width,
		const unsigned int height,
		const bool LuminanceOnly = false,
		const float FieldOfView = 45.f,
		const float Gamma = 2.2f,
		const float Luminance = 100.f,
		const float ColorFactor = 1.f,
		const unsigned int DownSample = 0);

} }

#endif


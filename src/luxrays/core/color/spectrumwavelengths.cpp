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

#include "luxrays/core/color/spectrumwavelengths.h"
#include "luxrays/core/color/spds/data/rgbE_32.h"
#include "luxrays/core/color/spds/data/xyzbasis.h"

using namespace luxrays;

const RegularSPD SpectrumWavelengths::spd_w(refrgb2spect_white,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_c(refrgb2spect_cyan,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_m(refrgb2spect_magenta,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_y(refrgb2spect_yellow,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_r(refrgb2spect_red,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_g(refrgb2spect_green,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_b(refrgb2spect_blue,
	refrgb2spect_start, refrgb2spect_end, refrgb2spect_bins,
	refrgb2spect_scale);

const RegularSPD SpectrumWavelengths::spd_ciex(CIE_X, CIEstart, CIEend, nCIE,
	683.f * float(WAVELENGTH_END - WAVELENGTH_START) / WAVELENGTH_SAMPLES);

const RegularSPD SpectrumWavelengths::spd_ciey(CIE_Y, CIEstart, CIEend, nCIE,
	683.f * float(WAVELENGTH_END - WAVELENGTH_START) / WAVELENGTH_SAMPLES);

const RegularSPD SpectrumWavelengths::spd_ciez(CIE_Z, CIEstart, CIEend, nCIE,
	683.f * float(WAVELENGTH_END - WAVELENGTH_START) / WAVELENGTH_SAMPLES);

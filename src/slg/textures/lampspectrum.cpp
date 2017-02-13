/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "slg/textures/lampspectrum.h"
#include "slg/textures/blackbody.h"
#include "slg/textures/irregulardata.h"
#include "slg/lights/data/lampspect.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Lamp spectrum texture
//------------------------------------------------------------------------------

Texture *slg::AllocLampSpectrumTex(const Properties &props, const string &propName) {
	const string name = props.Get(Property(propName + ".name")("Incandescent2")).Get<string>();
	const int wlCount = 250;

	float *wl;
	float *data;

	if (name == "Alcohol") {
		wl = lampspectrum_Alcohol_WL;
		data = lampspectrum_Alcohol_AP;
	} else if (name == "AntiInsect") {
		wl = lampspectrum_AntiInsect_WL;
		data = lampspectrum_AntiInsect_AP;
	} else if (name == "Ar") {
		wl = lampspectrum_Ar_WL;
		data = lampspectrum_Ar_AP;
	} else if (name == "BLAU") {
		wl = lampspectrum_BLAU_WL;
		data = lampspectrum_BLAU_AP;
	} else if (name == "BLNG") {
		wl = lampspectrum_BLNG_WL;
		data = lampspectrum_BLNG_AP;
	} else if (name == "BLP") {
		wl = lampspectrum_BLP_WL;
		data = lampspectrum_BLP_AP;
	} else if (name == "Butane") {
		wl = lampspectrum_Butane_WL;
		data = lampspectrum_Butane_AP;
	} else if (name == "Candle") {
		//wl = lampspectrum_Candle_WL;
		//data = lampspectrum_Candle_AP;
		// Override with blackbody for incandescent sources due to poor calibration
		return new BlackBodyTexture(1020.f);
	} else if (name == "CarbonArc") {
		wl = lampspectrum_CarbonArc_WL;
		data = lampspectrum_CarbonArc_AP;
	} else if (name == "Cd") {
		wl = lampspectrum_Cd_WL;
		data = lampspectrum_Cd_AP;
	} else if (name == "CFL27K") {
		wl = lampspectrum_CFL27K_WL;
		data = lampspectrum_CFL27K_AP;
	} else if (name == "CFL4K") {
		wl = lampspectrum_CFL4K_WL;
		data = lampspectrum_CFL4K_AP;
	} else if (name == "CFL6K") {
		wl = lampspectrum_CFL6K_WL;
		data = lampspectrum_CFL6K_AP;
	} else if (name == "CL42053") {
		wl = lampspectrum_CL42053_WL;
		data = lampspectrum_CL42053_AP;
	} else if (name == "CobaltGlass") {
		wl = lampspectrum_CobaltGlass_WL;
		data = lampspectrum_CobaltGlass_AP;
	} else if (name == "Daylight") {
		wl = lampspectrum_Daylight_WL;
		data = lampspectrum_Daylight_AP;
	} else if (name == "FeCo") {
		wl = lampspectrum_FeCo_WL;
		data = lampspectrum_FeCo_AP;
	} else if (name == "FL37K") {
		wl = lampspectrum_FL37K_WL;
		data = lampspectrum_FL37K_AP;
	} else if (name == "FLAV17K") {
		wl = lampspectrum_FLAV17K_WL;
		data = lampspectrum_FLAV17K_AP;
	} else if (name == "FLAV8K") {
		wl = lampspectrum_FLAV8K_WL;
		data = lampspectrum_FLAV8K_AP;
	} else if (name == "FLBL") {
		wl = lampspectrum_FLBL_WL;
		data = lampspectrum_FLBL_AP;
	} else if (name == "FLBLB") {
		wl = lampspectrum_FLBLB_WL;
		data = lampspectrum_FLBLB_AP;
	} else if (name == "FLD2") {
		wl = lampspectrum_FLD2_WL;
		data = lampspectrum_FLD2_AP;
	} else if (name == "GaPb") {
		wl = lampspectrum_GaPb_WL;
		data = lampspectrum_GaPb_AP;
	} else if (name == "GreenLaser") {
		wl = lampspectrum_GreenLaser_WL;
		data = lampspectrum_GreenLaser_AP;
	} else if (name == "GroLux") {
		wl = lampspectrum_GroLux_WL;
		data = lampspectrum_GroLux_AP;
	} else if (name == "GRUN") {
		wl = lampspectrum_GRUN_WL;
		data = lampspectrum_GRUN_AP;
	} else if (name == "HPM2") {
		wl = lampspectrum_HPM2_WL;
		data = lampspectrum_HPM2_AP;
	} else if (name == "HPMFL1") {
		wl = lampspectrum_HPMFL1_WL;
		data = lampspectrum_HPMFL1_AP;
	} else if (name == "HPMFL2") {
		wl = lampspectrum_HPMFL2_WL;
		data = lampspectrum_HPMFL2_AP;
	} else if (name == "HPMFL2Glow") {
		wl = lampspectrum_HPMFL2Glow_WL;
		data = lampspectrum_HPMFL2Glow_AP;
	} else if (name == "HPMFLCL42053") {
		wl = lampspectrum_HPMFLCL42053_WL;
		data = lampspectrum_HPMFLCL42053_AP;
	} else if (name == "HPMFLCobaltGlass") {
		wl = lampspectrum_HPMFLCobaltGlass_WL;
		data = lampspectrum_HPMFLCobaltGlass_AP;
	} else if (name == "HPMFLRedGlass") {
		wl = lampspectrum_HPMFLRedGlass_WL;
		data = lampspectrum_HPMFLRedGlass_AP;
	} else if (name == "HPMSB") {
		wl = lampspectrum_HPMSB_WL;
		data = lampspectrum_HPMSB_AP;
	} else if (name == "HPMSBFL") {
		wl = lampspectrum_HPMSBFL_WL;
		data = lampspectrum_HPMSBFL_AP;
	} else if (name == "HPS") {
		wl = lampspectrum_HPS_WL;
		data = lampspectrum_HPS_AP;
	} else if (name == "HPX") {
		wl = lampspectrum_HPX_WL;
		data = lampspectrum_HPX_AP;
	} else if (name == "Incandescent1") {
		//wl = lampspectrum_Incandescent1_WL;
		//data = lampspectrum_Incandescent1_AP;
		// Override with blackbody for incandescent sources due to poor calibration
		return new BlackBodyTexture(2750.f);
	} else if (name == "LCDS") {
		wl = lampspectrum_LCDS_WL;
		data = lampspectrum_LCDS_AP;
	} else if (name == "LEDB") {
		wl = lampspectrum_LEDB_WL;
		data = lampspectrum_LEDB_AP;
	} else if (name == "LPM2") {
		wl = lampspectrum_LPM2_WL;
		data = lampspectrum_LPM2_AP;
	} else if (name == "LPS") {
		wl = lampspectrum_LPS_WL;
		data = lampspectrum_LPS_AP;
	} else if (name == "MHD") {
		wl = lampspectrum_MHD_WL;
		data = lampspectrum_MHD_AP;
	} else if (name == "MHN") {
		wl = lampspectrum_MHN_WL;
		data = lampspectrum_MHN_AP;
	} else if (name == "MHSc") {
		wl = lampspectrum_MHSc_WL;
		data = lampspectrum_MHSc_AP;
	} else if (name == "MHWWD") {
		wl = lampspectrum_MHWWD_WL;
		data = lampspectrum_MHWWD_AP;
	} else if (name == "MPS") {
		wl = lampspectrum_MPS_WL;
		data = lampspectrum_MPS_AP;
	} else if (name == "Ne") {
		wl = lampspectrum_Ne_WL;
		data = lampspectrum_Ne_AP;
	} else if (name == "NeKrFL") {
		wl = lampspectrum_NeKrFL_WL;
		data = lampspectrum_NeKrFL_AP;
	} else if (name == "NeXeFL1") {
		wl = lampspectrum_NeXeFL1_WL;
		data = lampspectrum_NeXeFL1_AP;
	} else if (name == "NeXeFL2") {
		wl = lampspectrum_NeXeFL2_WL;
		data = lampspectrum_NeXeFL2_AP;
	} else if (name == "OliveOil") {
		wl = lampspectrum_OliveOil_WL;
		data = lampspectrum_OliveOil_AP;
	} else if (name == "PLANTA") {
		wl = lampspectrum_PLANTA_WL;
		data = lampspectrum_PLANTA_AP;
	} else if (name == "Rb") {
		wl = lampspectrum_Rb_WL;
		data = lampspectrum_Rb_AP;
	} else if (name == "RedGlass") {
		wl = lampspectrum_RedGlass_WL;
		data = lampspectrum_RedGlass_AP;
	} else if (name == "RedLaser") {
		wl = lampspectrum_RedLaser_WL;
		data = lampspectrum_RedLaser_AP;
	} else if (name == "SHPS") {
		wl = lampspectrum_SHPS_WL;
		data = lampspectrum_SHPS_AP;
	} else if (name == "SS1") {
		wl = lampspectrum_SS1_WL;
		data = lampspectrum_SS1_AP;
	} else if (name == "SS2") {
		wl = lampspectrum_SS2_WL;
		data = lampspectrum_SS2_AP;
	} else if (name == "TV") {
		wl = lampspectrum_TV_WL;
		data = lampspectrum_TV_AP;
	} else if (name == "UVA") {
		wl = lampspectrum_UVA_WL;
		data = lampspectrum_UVA_AP;
	} else if (name == "Welsbach") {
		wl = lampspectrum_Welsbach_WL;
		data = lampspectrum_Welsbach_AP;
	} else if (name == "Xe") {
		wl = lampspectrum_Xe_WL;
		data = lampspectrum_Xe_AP;
	} else if (name == "XeI") {
		wl = lampspectrum_XeI_WL;
		data = lampspectrum_XeI_AP;
	} else if (name == "Zn") {
		wl = lampspectrum_Zn_WL;
		data = lampspectrum_Zn_AP;
	} else {
		// if(name == "Incandescent2") {
		// NOTE - lordcrc - use Incandecent2 as default to match luxblend default
		//wl = lampspectrum_Incandescent2_WL;
		//data = lampspectrum_Incandescent2_AP;
		// Override with blackbody for incandescent sources due to poor calibration
		return new BlackBodyTexture(2900.f);
	}

	return new IrregularDataTexture(wlCount, wl, data, .1f, true);
}

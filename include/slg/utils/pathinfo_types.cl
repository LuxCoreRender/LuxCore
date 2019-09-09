#line 2 "pathinfo_types.cl"

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

// This is defined only under OpenCL because of variable size structures
#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	PathDepthInfo depth;
#if defined(PARAM_HAS_VOLUMES)
	PathVolumeInfo volume;
#endif

	int isPassThroughPath;

	// Last path vertex information
	BSDFEvent lastBSDFEvent;
	float lastBSDFPdfW;
	float lastGlossiness;
	Normal lastShadeN;
#if defined(PARAM_HAS_VOLUMES)
	bool lastFromVolume;
#endif

	int isNearlyCaustic;
	// Specular, Specular+ Diffuse and Specular+ Diffuse Specular+ paths
	int isNearlyS, isNearlySD, isNearlySDS;
} EyePathInfo;

#endif

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

typedef struct {
	PathDepthInfo maxPathDepth;

	// Russian roulette
	unsigned int rrDepth;
	float rrImportanceCap;

	// Hybrid backward/forward path tracing settings
	struct {
		int enabled;
		float glossinessThreshold;
	} hybridBackForward;

	// PhotonGI cache settings
	struct {
		float glossinessUsageThreshold;
		float indirectLookUpRadius;
		float indirectLookUpNormalCosAngle;
		float indirectUsageThresholdScale;
		unsigned int causticPhotonTracedCount;
		float causticLookUpRadius;
		float causticLookUpNormalCosAngle;

		int indirectEnabled, causticEnabled;

		PhotonGIDebugType debugType;
	} pgic;

	int forceBlackBackground;
} PathTracer;

#line 2 "materialdefs_funcs_mix.cl"

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

#if defined(PARAM_DISABLE_MAT_DYNAMIC_EVALUATION)

//------------------------------------------------------------------------------
// Mix material
//
// This requires a quite complex implementation because OpenCL doesn't support
// recursion.
//
// This code is used only when dynamic code generation is disabled
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MIX)

#define MIX_STACK_SIZE 16

BSDFEvent MixMaterial_IsDelta(__global Material *material
		MATERIALS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	materialStack[0] = material;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			materialStack[++stackIndex] = &mats[m->mix.matBIndex];
		} else {
			// Normal GetEventTypes() evaluation
			if (!Material_IsDeltaNoMix(m))
				return false;
		}
	}

	return true;
}

BSDFEvent MixMaterial_GetEventTypes(__global Material *material
		MATERIALS_PARAM_DECL) {
	BSDFEvent event = NONE;
	__global Material *materialStack[MIX_STACK_SIZE];
	materialStack[0] = material;
	int stackIndex = 0;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			materialStack[++stackIndex] = &mats[m->mix.matBIndex];
		} else {
			// Normal GetEventTypes() evaluation
			event |= Material_GetEventTypesNoMix(m);
		}
	}

	return event;
}

float3 MixMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	__global Material *materialStack[MIX_STACK_SIZE];
	float totalWeightStack[MIX_STACK_SIZE];

	// Push the root Mix material
	materialStack[0] = material;
	totalWeightStack[0] = 1.f;
	int stackIndex = 0;

	// Setup the results
	float3 result = BLACK;
	*event = NONE;
	if (directPdfW)
		*directPdfW = 0.f;

	while (stackIndex >= 0) {
		// Extract a material from the stack
		__global Material *m = materialStack[stackIndex];
		float totalWeight = totalWeightStack[stackIndex--];

		// Check if it is a Mix material too
		if (m->type == MIX) {
			// Add both material to the stack
			const float factor = Texture_GetFloatValue(m->mix.mixFactorTexIndex, hitPoint
					TEXTURES_PARAM);
			const float weight2 = clamp(factor, 0.f, 1.f);
			const float weight1 = 1.f - weight2;

			materialStack[++stackIndex] = &mats[m->mix.matAIndex];
			totalWeightStack[stackIndex] = totalWeight * weight1;

			materialStack[++stackIndex] = &mats[m->mix.matBIndex];			
			totalWeightStack[stackIndex] = totalWeight * weight2;
		} else {
			// Normal Evaluate() evaluation
			if (totalWeight > 0.f) {
				BSDFEvent eventMat;
				float directPdfWMat;
				const float3 resultMat = Material_EvaluateNoMix(m, hitPoint, lightDir, eyeDir, &eventMat, &directPdfWMat
						TEXTURES_PARAM);

				if (!Spectrum_IsBlack(resultMat)) {
					*event |= eventMat;
					result += totalWeight * resultMat;

					if (directPdfW)
						*directPdfW += totalWeight * directPdfWMat;
				}
			}
		}
	}

	return result;
}

float3 MixMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, const float passEvent,
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		MATERIALS_PARAM_DECL) {
	__global Material *evaluationMatList[MIX_STACK_SIZE];
	float parentWeightList[MIX_STACK_SIZE];
	int evaluationListSize = 0;

	// Setup the results
	float3 result = BLACK;
	*pdfW = 0.f;

	// Look for a no Mix material to sample
	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	float parentWeight = 1.f;
	for (;;) {
		const float factor = Texture_GetFloatValue(currentMixMat->mix.mixFactorTexIndex, hitPoint
			TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);

		const float weightFirst = sampleMatA ? weight1 : weight2;
		const float weightSecond = sampleMatA ? weight2 : weight1;

		const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

		const uint matIndexFirst = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		const uint matIndexSecond = sampleMatA ? currentMixMat->mix.matBIndex : currentMixMat->mix.matAIndex;

		// Sample the first material, evaluate the second
		__global Material *matFirst = &mats[matIndexFirst];
		__global Material *matSecond = &mats[matIndexSecond];

		//----------------------------------------------------------------------
		// Add the second material to the evaluation list
		//----------------------------------------------------------------------

		if (weightSecond > 0.f) {
			evaluationMatList[evaluationListSize] = matSecond;
			parentWeightList[evaluationListSize++] = parentWeight * weightSecond;
		}

		//----------------------------------------------------------------------
		// Sample the first material
		//----------------------------------------------------------------------

		// Check if it is a Mix material too
		if (matFirst->type == MIX) {
			// Make the first material the current
			currentMixMat = matFirst;
			passThroughEvent = passThroughEventFirst;
			parentWeight *= weightFirst;
		} else {
			// Sample the first material
			float pdfWMatFirst;
			const float3 sampleResult = Material_SampleNoMix(matFirst, hitPoint,
					fixedDir, sampledDir,
					u0, u1, passThroughEventFirst,
					&pdfWMatFirst, cosSampledDir, event,
					requestedEvent
					TEXTURES_PARAM);

			if (all(isequal(sampleResult, BLACK)))
				return BLACK;

			const float weight = parentWeight * weightFirst;
			*pdfW += weight * pdfWMatFirst;
			result += weight * sampleResult * pdfWMatFirst;

			// I can stop now
			break;
		}
	}

	while (evaluationListSize > 0) {
		// Extract the material to evaluate
		__global Material *evalMat = evaluationMatList[--evaluationListSize];
		const float evalWeight = parentWeightList[evaluationListSize];

		// Evaluate the material

		// Check if it is a Mix material too
		BSDFEvent eventMat;
		float pdfWMat;
		float3 eval;
		if (evalMat->type == MIX) {
			eval = MixMaterial_Evaluate(evalMat, hitPoint, *sampledDir, fixedDir,
					&eventMat, &pdfWMat
					MATERIALS_PARAM);
		} else {
			eval = Material_EvaluateNoMix(evalMat, hitPoint, *sampledDir, fixedDir,
					&eventMat, &pdfWMat
					TEXTURES_PARAM);
		}
		if (!Spectrum_IsBlack(eval)) {
			result += evalWeight * eval;
			*pdfW += evalWeight * pdfWMat;
		}
	}

	return result / *pdfW;
}

float3 MixMaterial_GetEmittedRadiance(__global Material *material, __global HitPoint *hitPoint
		MATERIALS_PARAM_DECL) {
	if (material->emitTexIndex != NULL_INDEX) {
        // Use this mix node emission
        return Material_GetEmittedRadianceNoMix(material, hitPoint
                TEXTURES_PARAM);
    } else {
		__global Material *materialStack[MIX_STACK_SIZE];
		float totalWeightStack[MIX_STACK_SIZE];

		// Push the root Mix material
		materialStack[0] = material;
		totalWeightStack[0] = 1.f;
		int stackIndex = 0;

		// Setup the results
		float3 result = BLACK;

		while (stackIndex >= 0) {
			// Extract a material from the stack
			__global Material *m = materialStack[stackIndex];
			float totalWeight = totalWeightStack[stackIndex--];

			if (m->type == MIX) {
				const float factor = Texture_GetFloatValue(m->mix.mixFactorTexIndex, hitPoint
						TEXTURES_PARAM);
				const float weight2 = clamp(factor, 0.f, 1.f);
				const float weight1 = 1.f - weight2;

				if (weight1 > 0.f) {
					materialStack[++stackIndex] = &mats[m->mix.matAIndex];
					totalWeightStack[stackIndex] = totalWeight * weight1;
				}

				if (weight2 > 0.f) {
					materialStack[++stackIndex] = &mats[m->mix.matBIndex];
					totalWeightStack[stackIndex] = totalWeight * weight2;
				}
			} else {
				const float3 emitRad = Material_GetEmittedRadianceNoMix(m, hitPoint
					TEXTURES_PARAM);
				if (!Spectrum_IsBlack(emitRad))
					result += totalWeight * emitRad;
			}
		}

		return result;
	}
}

#if defined(PARAM_HAS_BUMPMAPS)
void MixMaterial_Bump(__global Material *material, __global HitPoint *hitPoint,
        const float3 dpdu, const float3 dpdv,
        const float3 dndu, const float3 dndv, const float weight
        MATERIALS_PARAM_DECL) {
    if (weight == 0.f)
        return;

    if (material->bumpTexIndex != NULL_INDEX) {
        // Use this mix node bump mapping
        Material_BumpNoMix(material, hitPoint,
                dpdu, dpdv, dndu, dndv, weight
                TEXTURES_PARAM);
    } else {
        // Mix the child bump mapping
        __global Material *materialStack[MIX_STACK_SIZE];
        float totalWeightStack[MIX_STACK_SIZE];

        // Push the root Mix material
        materialStack[0] = material;
        totalWeightStack[0] = weight;
        int stackIndex = 0;

        while (stackIndex >= 0) {
            // Extract a material from the stack
            __global Material *m = materialStack[stackIndex];
            float totalWeight = totalWeightStack[stackIndex--];

            if (m->type == MIX) {
                const float factor = Texture_GetFloatValue(m->mix.mixFactorTexIndex, hitPoint
                        TEXTURES_PARAM);
                const float weight2 = clamp(factor, 0.f, 1.f);
                const float weight1 = 1.f - weight2;

                if (weight1 > 0.f) {
                    materialStack[++stackIndex] = &mats[m->mix.matAIndex];
                    totalWeightStack[stackIndex] = totalWeight * weight1;
                }

                if (weight2 > 0.f) {
                    materialStack[++stackIndex] = &mats[m->mix.matBIndex];
                    totalWeightStack[stackIndex] = totalWeight * weight2;
                }
            } else {
                Material_BumpNoMix(m, hitPoint,
                        dpdu, dpdv, dndu, dndv, totalWeight
                        TEXTURES_PARAM);
            }
        }
    }
}
#endif

float3 MixMaterial_GetPassThroughTransparency(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, const float passEvent
		MATERIALS_PARAM_DECL) {
	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	for (;;) {
		const float factor = Texture_GetFloatValue(currentMixMat->mix.mixFactorTexIndex, hitPoint
				TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);
		passThroughEvent = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;
		const uint matIndex = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		__global Material *mat = &mats[matIndex];

		if (mat->type == MIX) {
			currentMixMat = mat;
		} else
			return Material_GetPassThroughTransparencyNoMix(mat, hitPoint, fixedDir, passThroughEvent
					TEXTURES_PARAM);
	}
}

#if defined(PARAM_HAS_VOLUMES)
uint MixMaterial_GetInteriorVolume(__global Material *material, 
		__global HitPoint *hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float passEvent
#endif
		MATERIALS_PARAM_DECL) {
	const uint intVolIndex = Material_GetInteriorVolumeNoMix(material);
	if (intVolIndex != NULL_INDEX)
		return intVolIndex;

	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	for (;;) {
		const float factor = Texture_GetFloatValue(currentMixMat->mix.mixFactorTexIndex, hitPoint
				TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);
		passThroughEvent = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;
		const uint matIndex = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		__global Material *mat = &mats[matIndex];

		const uint intVolIndex = Material_GetInteriorVolumeNoMix(mat);
		if (mat->type == MIX) {
			if (intVolIndex != NULL_INDEX)
				return intVolIndex;
			else
				currentMixMat = mat;
		} else
			return intVolIndex;
	}
}
#endif

#if defined(PARAM_HAS_VOLUMES)
uint MixMaterial_GetExteriorVolume(__global Material *material, 
		__global HitPoint *hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float passEvent
#endif
		MATERIALS_PARAM_DECL) {
	const uint extVolIndex = Material_GetExteriorVolumeNoMix(material);
	if (extVolIndex != NULL_INDEX)
		return extVolIndex;

	__global Material *currentMixMat = material;
	float passThroughEvent = passEvent;
	for (;;) {
		const float factor = Texture_GetFloatValue(currentMixMat->mix.mixFactorTexIndex, hitPoint
				TEXTURES_PARAM);
		const float weight2 = clamp(factor, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		const bool sampleMatA = (passThroughEvent < weight1);
		passThroughEvent = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;
		const uint matIndex = sampleMatA ? currentMixMat->mix.matAIndex : currentMixMat->mix.matBIndex;
		__global Material *mat = &mats[matIndex];

		const uint extVolIndex = Material_GetExteriorVolumeNoMix(material);
		if (mat->type == MIX) {
			if (extVolIndex != NULL_INDEX)
				return extVolIndex;
			else
				currentMixMat = mat;
		} else
			return extVolIndex;
	}
}
#endif

#endif

//------------------------------------------------------------------------------
// Generic material functions with Mix support
//------------------------------------------------------------------------------

BSDFEvent Material_GetEventTypes(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetEventTypes(material
				MATERIALS_PARAM);
	else
#endif
		return Material_GetEventTypesNoMix(material);
}

bool Material_IsDelta(const uint matIndex
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_IsDelta(material
				MATERIALS_PARAM);
	else
#endif
		return Material_IsDeltaNoMix(material);
}

float3 Material_Evaluate(const uint matIndex,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_Evaluate(material, hitPoint, lightDir, eyeDir,
				event, directPdfW
				MATERIALS_PARAM);
	else
#endif
		return Material_EvaluateNoMix(material, hitPoint, lightDir, eyeDir,
				event, directPdfW
				TEXTURES_PARAM);
}

float3 Material_Sample(const uint matIndex,	__global HitPoint *hitPoint,
		const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_Sample(material, hitPoint,
				fixedDir, sampledDir,
				u0, u1,
				passThroughEvent,
				pdfW, cosSampledDir, event, requestedEvent
				MATERIALS_PARAM);
	else
#endif
		return Material_SampleNoMix(material, hitPoint,
				fixedDir, sampledDir,
				u0, u1,
#if defined(PARAM_HAS_PASSTHROUGH)
				passThroughEvent,
#endif
				pdfW, cosSampledDir, event, requestedEvent
				TEXTURES_PARAM);
}

float3 Material_GetEmittedRadiance(const uint matIndex,
		__global HitPoint *hitPoint, const float oneOverPrimitiveArea
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

	float3 result;
#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		result = MixMaterial_GetEmittedRadiance(material, hitPoint
				MATERIALS_PARAM);
	else
#endif
		result = Material_GetEmittedRadianceNoMix(material, hitPoint
				TEXTURES_PARAM);

	return 	VLOAD3F(material->emittedFactor.c) * (material->usePrimitiveArea ? oneOverPrimitiveArea : 1.f) * result;
}

#if defined(PARAM_HAS_BUMPMAPS)
void Material_Bump(const uint matIndex, __global HitPoint *hitPoint,
        const float3 dpdu, const float3 dpdv,
        const float3 dndu, const float3 dndv, const float weight
        MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		MixMaterial_Bump(material, hitPoint,
                dpdu, dpdv, dndu, dndv, weight
                MATERIALS_PARAM);
	else
#endif
		Material_BumpNoMix(material, hitPoint,
                dpdu, dpdv, dndu, dndv, weight
                TEXTURES_PARAM);
}
#endif

#if defined(PARAM_HAS_PASSTHROUGH)
float3 Material_GetPassThroughTransparency(const uint matIndex,
		__global HitPoint *hitPoint, const float3 fixedDir, const float passThroughEvent
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetPassThroughTransparency(material,
				hitPoint, fixedDir, passThroughEvent
				MATERIALS_PARAM);
	else
#endif
		return Material_GetPassThroughTransparencyNoMix(material,
				hitPoint, fixedDir, passThroughEvent
				TEXTURES_PARAM);
}
#endif

#if defined(PARAM_HAS_VOLUMES)
uint Material_GetInteriorVolume(const uint matIndex,
		__global HitPoint *hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float passThroughEvent
#endif
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetInteriorVolume(material, hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, passThroughEvent
#endif
			MATERIALS_PARAM);
	else
#endif
		return Material_GetInteriorVolumeNoMix(material);
}

uint Material_GetExteriorVolume(const uint matIndex,
		__global HitPoint *hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
		, const float passThroughEvent
#endif
		MATERIALS_PARAM_DECL) {
	__global Material *material = &mats[matIndex];

#if defined (PARAM_ENABLE_MAT_MIX)
	if (material->type == MIX)
		return MixMaterial_GetExteriorVolume(material, hitPoint
#if defined(PARAM_HAS_PASSTHROUGH)
			, passThroughEvent
#endif
			MATERIALS_PARAM);
	else
#endif
		return Material_GetExteriorVolumeNoMix(material);
}
#endif

#endif

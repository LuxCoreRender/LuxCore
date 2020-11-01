#line 2 "texture_bombing_funcs.cl"

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
// Bombing texture
//------------------------------------------------------------------------------

//----------------------------------------------------------------------
// For the very special case of Bombing texture evaluation
//
// EVAL_TRIPLANAR_STEP_1, EVAL_TRIPLANAR_STEP_2, EVAL_TRIPLANAR_STEP_3
// and EVAL_TRIPLANAR
//----------------------------------------------------------------------

OPENCL_FORCE_INLINE bool BombingTexture_WriteCellUV(__global const Texture* restrict texture,
		int i, int j, const float2 uv, const float resultPriority,
		float *currentTexturePriority, __global HitPoint *hitPointTmp
		TEXTURES_PARAM_DECL) {
	const float2 cell = MAKE_FLOAT2(floor(uv.x), floor(uv.y));
	const float2 cellOffset = MAKE_FLOAT2(uv.x - cell.x, uv.y - cell.y);

	const float2 currentCell = cell + MAKE_FLOAT2(i, j);
	const float2 currentCellOffset = cellOffset - MAKE_FLOAT2(i, j);

	// Pick cell random values
	__global const ImageMap *randomImageMap = &imageMapDescs[texture->bombingTex.randomImageMapIndex];
	const uint randomImageMapWidth = randomImageMap->width;
	const uint randomImageMapHeight = randomImageMap->height;

	const uint randomImageMapStorageIndex = (((int)currentCell.x) % (randomImageMapWidth / 2)) * 2 +
			(((int)currentCell.y) % randomImageMapHeight) * randomImageMapWidth;

	const float3 noise0 = ImageMapStorage_GetSpectrum(randomImageMap,
			randomImageMapStorageIndex % randomImageMapWidth, randomImageMapStorageIndex / randomImageMapHeight
			IMAGEMAPS_PARAM);

	const float currentCellRandomOffsetX = noise0.x;
	const float currentCellRandomOffsetY = noise0.y;
	*currentTexturePriority = noise0.z;

	// Check the priority of this cell
	if (*currentTexturePriority <= resultPriority)
		return false;

	// Find The cell UV coordinates
	float2 bulletUV = MAKE_FLOAT2(currentCellOffset.x - currentCellRandomOffsetX, currentCellOffset.y - currentCellRandomOffsetY);

	// Pick some more cell random values
	const float3 noise1 = ImageMapStorage_GetSpectrum(randomImageMap,
			(randomImageMapStorageIndex + 1) % randomImageMapWidth, (randomImageMapStorageIndex + 1) / randomImageMapHeight
			IMAGEMAPS_PARAM);

	const float currentCellRandomRotate = noise1.x;
	const float currentCellRandomScale = noise1.y;
	const float currentCellRandomBullet = noise1.z;

	// Translate to the cell center
	const float2 translatedUV = MAKE_FLOAT2(bulletUV.x - .5f, bulletUV.y - .5f);

	// Apply random scale
	const float scaleFactor = Lerp(currentCellRandomScale, 1.f, 1.f + texture->bombingTex.randomScaleFactor);
	const float2 scaledUV = MAKE_FLOAT2(translatedUV.x * scaleFactor, translatedUV.y * scaleFactor);

	// Apply random rotation
	const float angle = texture->bombingTex.useRandomRotation ? currentCellRandomRotate * (2.f * M_PI_F) : 0.f;
	const float sinAngle = sin(angle);
	const float cosAngle = cos(angle);
	const float2 rotatedUV = MAKE_FLOAT2(
			scaledUV.x * cosAngle - scaledUV.y * sinAngle,
			scaledUV.x * sinAngle + scaledUV.y * cosAngle);

	// Translate back the cell center
	bulletUV.x = rotatedUV.x + .5f;
	bulletUV.y = rotatedUV.y + .5f;

	// Check if I'm inside the cell
	//
	// The check is done ouside this function on GPUs
	if ((bulletUV.x < 0.f) || (bulletUV.x >= 1.f) ||
			(bulletUV.y < 0.f) || (bulletUV.y >= 1.f))
		return false;

	// Pick a bullet out if multiple available shapes (if multiBulletCount > 1)
	const uint multiBulletCount = texture->bombingTex.multiBulletCount;
	const uint currentCellRandomBulletIndex = Floor2UInt(multiBulletCount * currentCellRandomBullet);
	bulletUV.x = (currentCellRandomBulletIndex + bulletUV.x) / multiBulletCount;

	VSTORE2F(bulletUV, &hitPointTmp->defaultUV.u);

	return true;
}

OPENCL_FORCE_NOT_INLINE void BombingTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_BOMBING_SETUP_11: {
			// Read background texture evaluation
			float3 backgroundTexValue;
			EvalStack_PopFloat3(backgroundTexValue);

			// Save original UV
			const float2 uv = VLOAD2F(&hitPoint->defaultUV.u);
			EvalStack_PushFloat2(uv);

			// Save mapped UV
			const float2 mapUV = TextureMapping2D_Map(&texture->bombingTex.mapping, hitPoint TEXTURES_PARAM);
			EvalStack_PushFloat2(mapUV);

			// Save background texture value
			EvalStack_PushFloat3(backgroundTexValue);

			// Save current result
			EvalStack_PushFloat3(backgroundTexValue);

			// Save result priority
			float resultPriority = -1.f;
			EvalStack_PushFloat(resultPriority);

			// Update HitPoint
			float currentTexturePriority;
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			const bool usableResult = BombingTexture_WriteCellUV(texture,
					-1, -1, mapUV, resultPriority,
					&currentTexturePriority, hitPointTmp
					TEXTURES_PARAM);

			// Save current result priority
			EvalStack_PushFloat(currentTexturePriority);

			// Save usable result
			EvalStack_PushInt(usableResult);
			break;
		}
		case EVAL_BOMBING_SETUP_10: {
			// Read bullet mask texture evaluation
			float bulletMaskTexValue;
			EvalStack_PopFloat(bulletMaskTexValue);
			// Read bullet texture evaluation
			float3 bulletTexValue;
			EvalStack_PopFloat3(bulletTexValue);

			// Read usable result
			bool usableResult;
			EvalStack_PopInt(usableResult);

			// Read current result priority
			float currentTexturePriority;
			EvalStack_PopFloat(currentTexturePriority);

			// Read result priority
			float resultPriority;
			EvalStack_PopFloat(resultPriority);

			// Read current result
			float3 currentResult;
			EvalStack_PopFloat3(currentResult);

			// Read background texture value
			float3 backgroundTexValue;
			EvalStack_PopFloat3(backgroundTexValue);

			// Read mapped UV
			float2 mapUV;
			EvalStack_PopFloat2(mapUV);

			// Save mapped UV
			EvalStack_PushFloat2(mapUV);

			// Save background texture value
			EvalStack_PushFloat3(backgroundTexValue);
			
			// Save current result
			if (usableResult && (bulletMaskTexValue > 0.f)) {
				resultPriority = currentTexturePriority;
				currentResult = Lerp3(bulletMaskTexValue, backgroundTexValue, bulletTexValue);
			}
			EvalStack_PushFloat3(currentResult);

			// Save current result priority
			EvalStack_PushFloat(resultPriority);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			usableResult = BombingTexture_WriteCellUV(texture,
					-1, 0, mapUV, resultPriority,
					&currentTexturePriority, hitPointTmp
					TEXTURES_PARAM);

			// Save current result priority
			EvalStack_PushFloat(currentTexturePriority);

			// Save usable result
			EvalStack_PushInt(usableResult);
			break;
		}
		case EVAL_BOMBING_SETUP_01: {
			// Read bullet mask texture evaluation
			float bulletMaskTexValue;
			EvalStack_PopFloat(bulletMaskTexValue);
			// Read bullet texture evaluation
			float3 bulletTexValue;
			EvalStack_PopFloat3(bulletTexValue);

			// Read usable result
			bool usableResult;
			EvalStack_PopInt(usableResult);

			// Read current result priority
			float currentTexturePriority;
			EvalStack_PopFloat(currentTexturePriority);

			// Read result priority
			float resultPriority;
			EvalStack_PopFloat(resultPriority);

			// Read current result
			float3 currentResult;
			EvalStack_PopFloat3(currentResult);

			// Read background texture value
			float3 backgroundTexValue;
			EvalStack_PopFloat3(backgroundTexValue);

			// Read mapped UV
			float2 mapUV;
			EvalStack_PopFloat2(mapUV);

			// Save mapped UV
			EvalStack_PushFloat2(mapUV);

			// Save background texture value
			EvalStack_PushFloat3(backgroundTexValue);
			
			// Save current result
			if (usableResult && (bulletMaskTexValue > 0.f)) {
				resultPriority = currentTexturePriority;
				currentResult = Lerp3(bulletMaskTexValue, backgroundTexValue, bulletTexValue);
			}
			EvalStack_PushFloat3(currentResult);

			// Save current result priority
			EvalStack_PushFloat(resultPriority);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			usableResult = BombingTexture_WriteCellUV(texture,
					0, -1, mapUV, resultPriority,
					&currentTexturePriority, hitPointTmp
					TEXTURES_PARAM);

			// Save current result priority
			EvalStack_PushFloat(currentTexturePriority);

			// Save usable result
			EvalStack_PushInt(usableResult);
			break;
		}
		case EVAL_BOMBING_SETUP_00: {
			// Read bullet mask texture evaluation
			float bulletMaskTexValue;
			EvalStack_PopFloat(bulletMaskTexValue);
			// Read bullet texture evaluation
			float3 bulletTexValue;
			EvalStack_PopFloat3(bulletTexValue);

			// Read usable result
			bool usableResult;
			EvalStack_PopInt(usableResult);

			// Read current result priority
			float currentTexturePriority;
			EvalStack_PopFloat(currentTexturePriority);

			// Read result priority
			float resultPriority;
			EvalStack_PopFloat(resultPriority);

			// Read current result
			float3 currentResult;
			EvalStack_PopFloat3(currentResult);

			// Read background texture value
			float3 backgroundTexValue;
			EvalStack_PopFloat3(backgroundTexValue);

			// Read mapped UV
			float2 mapUV;
			EvalStack_PopFloat2(mapUV);

			// Save mapped UV
			EvalStack_PushFloat2(mapUV);

			// Save background texture value
			EvalStack_PushFloat3(backgroundTexValue);
			
			// Save current result
			if (usableResult && (bulletMaskTexValue > 0.f)) {
				resultPriority = currentTexturePriority;
				currentResult = Lerp3(bulletMaskTexValue, backgroundTexValue, bulletTexValue);
			}
			EvalStack_PushFloat3(currentResult);

			// Save current result priority
			EvalStack_PushFloat(resultPriority);

			// Update HitPoint
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			usableResult = BombingTexture_WriteCellUV(texture,
					0, 0, mapUV, resultPriority,
					&currentTexturePriority, hitPointTmp
					TEXTURES_PARAM);

			// Save current result priority
			EvalStack_PushFloat(currentTexturePriority);

			// Save usable result
			EvalStack_PushInt(usableResult);
			break;
		}
		case EVAL_FLOAT:
		case EVAL_SPECTRUM: {
			// Read bullet mask texture evaluation
			float bulletMaskTexValue;
			EvalStack_PopFloat(bulletMaskTexValue);
			// Read bullet texture evaluation
			float3 bulletTexValue;
			EvalStack_PopFloat3(bulletTexValue);

			// Read usable result
			bool usableResult;
			EvalStack_PopInt(usableResult);

			// Read current result priority
			float currentTexturePriority;
			EvalStack_PopFloat(currentTexturePriority);

			// Read result priority
			float resultPriority;
			EvalStack_PopFloat(resultPriority);

			// Read current result
			float3 currentResult;
			EvalStack_PopFloat3(currentResult);

			// Read background texture value
			float3 backgroundTexValue;
			EvalStack_PopFloat3(backgroundTexValue);

			// Read mapped UV
			float2 mapUV;
			EvalStack_PopFloat2(mapUV);

			// Read original UV
			float2 uv;
			EvalStack_PopFloat2(uv);
			
			// Save current result
			if (usableResult && (bulletMaskTexValue > 0.f)) {
				resultPriority = currentTexturePriority;
				currentResult = Lerp3(bulletMaskTexValue, backgroundTexValue, bulletTexValue);
			}

			// Restore original HitPoint UV
			__global HitPoint *hitPointTmp = (__global HitPoint *)hitPoint;
			VSTORE2F(uv, &hitPointTmp->defaultUV.u);

			// Save the texture evaluation result
			if (evalType == EVAL_FLOAT) {
				EvalStack_PushFloat(Spectrum_Y(currentResult));
			} else {
				EvalStack_PushFloat3(currentResult);
			}
			break;
		}
		case EVAL_BUMP_GENERIC_OFFSET_U:
			Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
					hitPoint, sampleDistance);
			break;
		case EVAL_BUMP_GENERIC_OFFSET_V:
			Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
					hitPoint, sampleDistance);
			break;
		case EVAL_BUMP:
			Texture_EvalOpGenericBump(evalStack, evalStackOffset,
					hitPoint, sampleDistance);
			break;
		default:
			// Something wrong here
			break;
	}
}

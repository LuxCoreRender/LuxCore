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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"

#include "slg/materials/archglass.h"
#include "slg/materials/carpaint.h"
#include "slg/materials/cloth.h"
#include "slg/materials/glass.h"
#include "slg/materials/glossy2.h"
#include "slg/materials/glossycoating.h"
#include "slg/materials/glossytranslucent.h"
#include "slg/materials/matte.h"
#include "slg/materials/mattetranslucent.h"
#include "slg/materials/metal2.h"
#include "slg/materials/mirror.h"
#include "slg/materials/mix.h"
#include "slg/materials/null.h"
#include "slg/materials/roughglass.h"
#include "slg/materials/roughmatte.h"
#include "slg/materials/roughmattetranslucent.h"
#include "slg/materials/twosided.h"
#include "slg/materials/velvet.h"

#include "slg/volumes/clear.h"
#include "slg/volumes/heterogenous.h"
#include "slg/volumes/homogenous.h"
#include "slg/materials/disney.h"

using namespace std;
using namespace luxrays;
using namespace slg;

u_int CompiledScene::CompileMaterialConditionalOps(const u_int matIndex,
		const vector<slg::ocl::MaterialEvalOp> &evalOpsA, const u_int evalOpStackSizeA,
		const vector<slg::ocl::MaterialEvalOp> &evalOpsB, const u_int evalOpStackSizeB,
		vector<slg::ocl::MaterialEvalOp> &evalOps) const {
	u_int evalOpStackSize = 0;
	
	// Add the conditional goto
	slg::ocl::MaterialEvalOp opGotoA;
	opGotoA.matIndex = matIndex;
	opGotoA.evalType = slg::ocl::EVAL_CONDITIONAL_GOTO;
	// The +1 is for the unconditional goto
	opGotoA.opData.opsCount = evalOpsA.size() + 1;
	evalOps.push_back(opGotoA);

	// Add compiled Ops of material A
	evalOps.insert(evalOps.end(), evalOpsA.begin(), evalOpsA.end());
	evalOpStackSize += evalOpStackSizeA;

	// Add the unconditional goto
	slg::ocl::MaterialEvalOp opGotoB;
	opGotoB.matIndex = matIndex;
	opGotoB.evalType = slg::ocl::EVAL_UNCONDITIONAL_GOTO;
	opGotoB.opData.opsCount = evalOpsB.size();
	evalOps.push_back(opGotoB);

	// Add compiled Ops of material B
	evalOps.insert(evalOps.end(), evalOpsB.begin(), evalOpsB.end());
	evalOpStackSize += evalOpStackSizeB;

	return evalOpStackSize;
}

u_int CompiledScene::CompileMaterialConditionalOps(const u_int matIndex,
		const u_int matAIndex, const slg::ocl::MaterialEvalOpType opTypeA,
		const u_int matBIndex, const slg::ocl::MaterialEvalOpType opTypeB,
		vector<slg::ocl::MaterialEvalOp> &evalOps) const {
	// Compile opType of material A
	vector<slg::ocl::MaterialEvalOp> evalOpsA;
	const u_int evalOpStackSizeA = CompileMaterialOps(matAIndex, opTypeA,
			evalOpsA);

	// Compile opType of material B
	vector<slg::ocl::MaterialEvalOp> evalOpsB;
	const u_int evalOpStackSizeB = CompileMaterialOps(matBIndex, opTypeB,
			evalOpsB);

	return CompileMaterialConditionalOps(matIndex,
			evalOpsA, evalOpStackSizeA,
			evalOpsB, evalOpStackSizeB,
			evalOps);
}

u_int CompiledScene::CompileMaterialOps(const u_int matIndex,
		const slg::ocl::MaterialEvalOpType opType,
		vector<slg::ocl::MaterialEvalOp> &evalOps) const {
	// Translate materials to material evaluate ops

	const slg::ocl::Material *mat = &mats[matIndex];
	u_int evalOpStackSize = 0;
	bool addDefaultOp = true;

	switch (mat->type) {
		//----------------------------------------------------------------------
		// Materials without sub-nodes
		//----------------------------------------------------------------------
		case MATTE:
		case MIRROR:
		case GLASS:
		case ARCHGLASS:
		case NULLMAT:
		case MATTETRANSLUCENT:
		case GLOSSY2:
		case METAL2:
		case ROUGHGLASS:
		case VELVET:
		case CLOTH:
		case CARPAINT:
		case ROUGHMATTE:
		case ROUGHMATTETRANSLUCENT:
		case GLOSSYTRANSLUCENT:
		case DISNEY:
		case HOMOGENEOUS_VOL:
		case CLEAR_VOL:
		case HETEROGENEOUS_VOL:
			switch (opType) {
				case slg::ocl::EVAL_ALBEDO:
					// 1 x parameter and 3 x results
					evalOpStackSize += 3;
					break;
				case slg::ocl::EVAL_GET_INTERIOR_VOLUME:
					// 1 x parameter and 1 x result
					evalOpStackSize += 1;
					break;
				case slg::ocl::EVAL_GET_EXTERIOR_VOLUME:
					// 1 x parameter and 1 x result
					evalOpStackSize += 1;
					break;
				case slg::ocl::EVAL_GET_EMITTED_RADIANCE:
					// 1 x parameter and 3 x results
					evalOpStackSize += 3;
					break;
				case slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					// 5 x parameters and 3 x results
					evalOpStackSize += 5;
					break;
				case slg::ocl::EVAL_EVALUATE:
					// 6 x parameters and 5 x results
					evalOpStackSize += 6;
					break;
				case slg::ocl::EVAL_SAMPLE:
					// 6 x parameters and 8 x results
					evalOpStackSize += 8;
					break;
				default:
					throw runtime_error("Unknown eval. type in CompiledScene::CompileMaterialOps(" + ToString(mat->type) + "): " + ToString(opType));
			}
			break;
		//----------------------------------------------------------------------
		// Materials with sub-nodes
		//----------------------------------------------------------------------
		case MIX:
			switch (opType) {
				case slg::ocl::EVAL_ALBEDO:
					evalOpStackSize += CompileMaterialOps(mat->mix.matAIndex, slg::ocl::EVAL_ALBEDO, evalOps);
					evalOpStackSize += CompileMaterialOps(mat->mix.matBIndex, slg::ocl::EVAL_ALBEDO, evalOps);
					break;
				case slg::ocl::EVAL_GET_VOLUME_MIX_SETUP1:
					evalOpStackSize += 3;
					break;
				case slg::ocl::EVAL_GET_VOLUME_MIX_SETUP2:
					evalOpStackSize += 4;
					break;
				case slg::ocl::EVAL_GET_INTERIOR_VOLUME:
					if (mat->interiorVolumeIndex == NULL_INDEX) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_VOLUME_MIX_SETUP1, evalOps);
						evalOpStackSize += CompileMaterialOps(mat->mix.matAIndex, slg::ocl::EVAL_GET_INTERIOR_VOLUME, evalOps);
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_VOLUME_MIX_SETUP2, evalOps);
						evalOpStackSize += CompileMaterialOps(mat->mix.matBIndex, slg::ocl::EVAL_GET_INTERIOR_VOLUME, evalOps);
					}

					// 1 x parameter and 1 x result
					evalOpStackSize += 1;	
					break;
				case slg::ocl::EVAL_GET_EXTERIOR_VOLUME:
					if (mat->exteriorVolumeIndex == NULL_INDEX) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_VOLUME_MIX_SETUP1, evalOps);
						evalOpStackSize += CompileMaterialOps(mat->mix.matAIndex, slg::ocl::EVAL_GET_EXTERIOR_VOLUME, evalOps);
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_VOLUME_MIX_SETUP2, evalOps);
						evalOpStackSize += CompileMaterialOps(mat->mix.matBIndex, slg::ocl::EVAL_GET_EXTERIOR_VOLUME, evalOps);
					}

					// 1 x parameter and 1 x result
					evalOpStackSize += 1;	
					break;
				case slg::ocl::EVAL_GET_EMITTED_RADIANCE_MIX_SETUP1:
					// 1 x parameter and 2 x results
					evalOpStackSize += 2;
					break;
				case slg::ocl::EVAL_GET_EMITTED_RADIANCE_MIX_SETUP2:
					// 1 x parameter and 5 x results
					evalOpStackSize += 5;
					break;
				case slg::ocl::EVAL_GET_EMITTED_RADIANCE:
					if (mat->emitTexIndex == NULL_INDEX) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_EMITTED_RADIANCE_MIX_SETUP1, evalOps);
						evalOpStackSize += CompileMaterialOps(mat->mix.matAIndex, slg::ocl::EVAL_GET_EMITTED_RADIANCE, evalOps);
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_EMITTED_RADIANCE_MIX_SETUP2, evalOps);
						evalOpStackSize += CompileMaterialOps(mat->mix.matBIndex, slg::ocl::EVAL_GET_EMITTED_RADIANCE, evalOps);
					}

					// 1 x parameter and 3 x result
					evalOpStackSize += 3;
					break;
				case slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP1:
					// 5 x parameters and 3 x results
					evalOpStackSize += 5;
					break;
				case slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP2:
					// 5 x parameters and 3 x results
					evalOpStackSize += 5;
					break;
				case slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP1, evalOps);
					evalOpStackSize += CompileMaterialOps(mat->mix.matAIndex, slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY, evalOps);
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY_MIX_SETUP2, evalOps);
					evalOpStackSize += CompileMaterialOps(mat->mix.matBIndex, slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY, evalOps);

					// 5 x parameter and 3 x result
					evalOpStackSize += 5;
					break;
				case slg::ocl::EVAL_EVALUATE_MIX_SETUP1:
					// 6 x parameters and 21 x results
					evalOpStackSize += 21;
					break;
				case slg::ocl::EVAL_EVALUATE_MIX_SETUP2:
					// 12 x parameters and 26 x results
					evalOpStackSize += 26;
					break;
				case slg::ocl::EVAL_EVALUATE:
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_EVALUATE_MIX_SETUP1, evalOps);
					evalOpStackSize += CompileMaterialOps(mat->mix.matAIndex, slg::ocl::EVAL_EVALUATE, evalOps);
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_EVALUATE_MIX_SETUP2, evalOps);
					evalOpStackSize += CompileMaterialOps(mat->mix.matBIndex, slg::ocl::EVAL_EVALUATE, evalOps);
					break;
				case slg::ocl::EVAL_SAMPLE_MIX_SETUP1:
					// 6 x parameters and 25 x results
					evalOpStackSize += 25;
					break;
				case slg::ocl::EVAL_SAMPLE_MIX_SETUP2:
					// 16 x parameters and 33 x results
					evalOpStackSize += 33;
					break;
				case slg::ocl::EVAL_SAMPLE: {
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_SAMPLE_MIX_SETUP1, evalOps);
					evalOpStackSize += CompileMaterialConditionalOps(matIndex,
							mat->mix.matAIndex, slg::ocl::EVAL_SAMPLE,
							mat->mix.matBIndex, slg::ocl::EVAL_SAMPLE,
							evalOps);
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_SAMPLE_MIX_SETUP2, evalOps);
					evalOpStackSize += CompileMaterialConditionalOps(matIndex,
							mat->mix.matAIndex, slg::ocl::EVAL_EVALUATE,
							mat->mix.matBIndex, slg::ocl::EVAL_EVALUATE,
							evalOps);
					break;
				}
				default:
					throw runtime_error("Unknown eval. type in CompiledScene::CompileMaterialOps(" + ToString(mat->type) + "): " + ToString(opType));
			}
			break;
		case GLOSSYCOATING:
			switch (opType) {
				case slg::ocl::EVAL_ALBEDO:
					evalOpStackSize += CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_ALBEDO, evalOps);
					break;
				case slg::ocl::EVAL_GET_INTERIOR_VOLUME:
					if (mat->interiorVolumeIndex == NULL_INDEX)
						evalOpStackSize += CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_GET_INTERIOR_VOLUME, evalOps);

					// 1 x parameter and 1 x result
					evalOpStackSize += 1;					
					break;
				case slg::ocl::EVAL_GET_EXTERIOR_VOLUME:
					if (mat->exteriorVolumeIndex == NULL_INDEX)
						evalOpStackSize += CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_GET_EXTERIOR_VOLUME, evalOps);

					// 1 x parameter and 1 x result
					evalOpStackSize += 1;
					break;
				case slg::ocl::EVAL_GET_EMITTED_RADIANCE:
					if (mat->emitTexIndex == NULL_INDEX)
						evalOpStackSize += CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_GET_EMITTED_RADIANCE, evalOps);

					// 1 x parameter and 3 x results
					evalOpStackSize += 3;
					break;
				case slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					evalOpStackSize += CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY, evalOps);

					// 5 x parameters and 3 x results
					evalOpStackSize += 5;
					break;
				case slg::ocl::EVAL_EVALUATE_GLOSSYCOATING_SETUP:
					// 6 x parameters and 21 x results
					evalOpStackSize += 21;
					break;
				case slg::ocl::EVAL_EVALUATE:
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_EVALUATE_GLOSSYCOATING_SETUP, evalOps);
					evalOpStackSize += CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_EVALUATE, evalOps);
					break;
				case slg::ocl::EVAL_SAMPLE_GLOSSYCOATING_SETUP:
					// 6 x parameters and 35 x results
					evalOpStackSize += 35;
					break;
				case slg::ocl::EVAL_SAMPLE_GLOSSYCOATING_CLOSE_SAMPLE_BASE:
					// 26 x parameters and 8 x results
					evalOpStackSize += 8;
					break;
				case slg::ocl::EVAL_SAMPLE_GLOSSYCOATING_CLOSE_EVALUATE_BASE:
					// 26 x parameters and 8 x results
					evalOpStackSize += 8;
					break;
				case slg::ocl::EVAL_SAMPLE: {
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_SAMPLE_GLOSSYCOATING_SETUP, evalOps);
					
					vector<slg::ocl::MaterialEvalOp> evalOpsSampleBase;
					u_int evalOpStackSizeSampleBase = CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_SAMPLE, evalOpsSampleBase);
					evalOpStackSizeSampleBase += CompileMaterialOps(matIndex, slg::ocl::EVAL_SAMPLE_GLOSSYCOATING_CLOSE_SAMPLE_BASE, evalOpsSampleBase);

					vector<slg::ocl::MaterialEvalOp> evalOpsEvaluateBase;
					u_int evalOpStackSizeEvaluateBase = CompileMaterialOps(mat->glossycoating.matBaseIndex, slg::ocl::EVAL_EVALUATE, evalOpsEvaluateBase);
					evalOpStackSizeEvaluateBase += CompileMaterialOps(matIndex, slg::ocl::EVAL_SAMPLE_GLOSSYCOATING_CLOSE_EVALUATE_BASE, evalOpsEvaluateBase);
							
					evalOpStackSize += CompileMaterialConditionalOps(matIndex,
							evalOpsSampleBase, evalOpStackSizeSampleBase,
							evalOpsEvaluateBase, evalOpStackSizeEvaluateBase,
							evalOps);
					
					addDefaultOp = false;
					break;
				}
				default:
					throw runtime_error("Unknown eval. type in CompiledScene::CompileMaterialOps(" + ToString(mat->type) + "): " + ToString(opType));
			}
			break;
		case TWOSIDED:
			switch (opType) {
				case slg::ocl::EVAL_ALBEDO:
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
					evalOpStackSize += CompileMaterialConditionalOps(matIndex,
							mat->twosided.frontMatIndex, opType,
							mat->twosided.backMatIndex, opType,
							evalOps);

					addDefaultOp = false;
					break;
				case slg::ocl::EVAL_GET_INTERIOR_VOLUME:
					if (mat->interiorVolumeIndex == NULL_INDEX) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
						evalOpStackSize += CompileMaterialConditionalOps(matIndex,
								mat->twosided.frontMatIndex, opType,
								mat->twosided.backMatIndex, opType,
								evalOps);

						addDefaultOp = false;
					} else {
						// 1 x parameter and 1 x result
						evalOpStackSize += 1;
					}
					break;
				case slg::ocl::EVAL_GET_EXTERIOR_VOLUME:
					if (mat->exteriorVolumeIndex == NULL_INDEX) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
						evalOpStackSize += CompileMaterialConditionalOps(matIndex,
								mat->twosided.frontMatIndex, opType,
								mat->twosided.backMatIndex, opType,
								evalOps);

						addDefaultOp = false;
					} else {
						// 1 x parameter and 1 x result
						evalOpStackSize += 1;
					}
					break;
				case slg::ocl::EVAL_GET_EMITTED_RADIANCE:
					if (mat->emitTexIndex == NULL_INDEX) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
						evalOpStackSize += CompileMaterialConditionalOps(matIndex,
								mat->twosided.frontMatIndex, opType,
								mat->twosided.backMatIndex, opType,
								evalOps);

						addDefaultOp = false;
					} else {
						// 1 x parameter and 3 x results
						evalOpStackSize += 3;
					}
					break;
				case slg::ocl::EVAL_GET_PASS_TROUGH_TRANSPARENCY:
					if ((mat->frontTranspTexIndex == NULL_INDEX) && (mat->backTranspTexIndex == NULL_INDEX)) {
						evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
						evalOpStackSize += CompileMaterialConditionalOps(matIndex,
								mat->twosided.frontMatIndex, opType,
								mat->twosided.backMatIndex, opType,
								evalOps);

						addDefaultOp = false;
					} else {
						// 5 x parameters and 3 x results
						evalOpStackSize += 5;
					}
					break;
				case slg::ocl::EVAL_EVALUATE:
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
					evalOpStackSize += CompileMaterialConditionalOps(matIndex,
							mat->twosided.frontMatIndex, opType,
							mat->twosided.backMatIndex, opType,
							evalOps);

					addDefaultOp = false;
					break;
				case slg::ocl::EVAL_SAMPLE:
					evalOpStackSize += CompileMaterialOps(matIndex, slg::ocl::EVAL_TWOSIDED_SETUP, evalOps);
					evalOpStackSize += CompileMaterialConditionalOps(matIndex,
							mat->twosided.frontMatIndex, opType,
							mat->twosided.backMatIndex, opType,
							evalOps);

					addDefaultOp = false;
					break;
				case slg::ocl::EVAL_TWOSIDED_SETUP:
					// 1 x parameter and 1 x results
					evalOpStackSize += 1;
					break;
				default:
					throw runtime_error("Unknown eval. type in CompiledScene::CompileMaterialOps(" + ToString(mat->type) + "): " + ToString(opType));
			}
			break;
		default:
			throw runtime_error("Unknown material in CompiledScene::CompileMaterialOps(" + ToString(opType) + "): " + ToString(mat->type));
	}

	if (addDefaultOp) {
		slg::ocl::MaterialEvalOp op;

		op.matIndex = matIndex;
		op.evalType = opType;

		evalOps.push_back(op);
	}

	return evalOpStackSize;
}

void CompiledScene::CompileMaterialOps() {
	// Translate materials to material evaluate ops

	matEvalOps.clear();
	maxMaterialEvalStackSize = 0;

	for (u_int i = 0; i < mats.size(); ++i) {
		slg::ocl::Material *mat = &mats[i];

		//----------------------------------------------------------------------
		// EVAL_ALBEDO
		//----------------------------------------------------------------------
		
		mat->evalAlbedoOpStartIndex = matEvalOps.size();
		const u_int evalAlbedoOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_ALBEDO, matEvalOps);
		mat->evalAlbedoOpLength = matEvalOps.size() - mat->evalAlbedoOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalAlbedoOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_GET_INTERIOR_VOLUME
		//----------------------------------------------------------------------
		
		mat->evalGetInteriorVolumeOpStartIndex = matEvalOps.size();
		const u_int evalGetInteriorVolumeOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_GET_INTERIOR_VOLUME, matEvalOps);
		mat->evalGetInteriorVolumeOpLength = matEvalOps.size() - mat->evalGetInteriorVolumeOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalGetInteriorVolumeOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_GET_EXTERIOR_VOLUME
		//----------------------------------------------------------------------
		
		mat->evalGetExteriorVolumeOpStartIndex = matEvalOps.size();
		const u_int evalGetExteriorVolumeOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_GET_EXTERIOR_VOLUME, matEvalOps);
		mat->evalGetExteriorVolumeOpLength = matEvalOps.size() - mat->evalGetExteriorVolumeOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalGetExteriorVolumeOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_GET_EMITTED_RADIANCE
		//----------------------------------------------------------------------
		
		mat->evalGetEmittedRadianceOpStartIndex = matEvalOps.size();
		const u_int evalGetEmittedRadianceOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_GET_EMITTED_RADIANCE, matEvalOps);
		mat->evalGetEmittedRadianceOpLength = matEvalOps.size() - mat->evalGetEmittedRadianceOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalGetEmittedRadianceOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_GET_PASS_TROUGH_TRANSPARENCY
		//----------------------------------------------------------------------
		
		mat->evalGetPassThroughTransparencyOpStartIndex = matEvalOps.size();
		const u_int evalGetPassThroughTransparencyOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_GET_PASS_TROUGH_TRANSPARENCY, matEvalOps);
		mat->evalGetPassThroughTransparencyOpLength = matEvalOps.size() - mat->evalGetPassThroughTransparencyOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalGetPassThroughTransparencyOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_EVALUATE
		//----------------------------------------------------------------------
		
		mat->evalEvaluateOpStartIndex = matEvalOps.size();
		const u_int evalEvaluateOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_EVALUATE, matEvalOps);
		mat->evalEvaluateOpLength = matEvalOps.size() - mat->evalEvaluateOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalEvaluateOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_SAMPLE
		//----------------------------------------------------------------------
		
		mat->evalSampleOpStartIndex = matEvalOps.size();
		const u_int evalSampleOpsStackSizeFloat = CompileMaterialOps(i,
				slg::ocl::MaterialEvalOpType::EVAL_SAMPLE, matEvalOps);
		mat->evalSampleOpLength = matEvalOps.size() - mat->evalSampleOpStartIndex;

		maxMaterialEvalStackSize = Max(maxMaterialEvalStackSize, evalSampleOpsStackSizeFloat);
	}

	SLG_LOG("Material evaluation ops count: " << matEvalOps.size());
	SLG_LOG("Material evaluation max. stack size: " << maxMaterialEvalStackSize);
}

void CompiledScene::CompileMaterials() {
	wasMaterialsCompiled = true;

	CompileTextures();

	const u_int materialsCount = scene.GetMaterials().GetSize();
	SLG_LOG("Compile " << materialsCount << " Materials");

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	mats.resize(materialsCount);

	for (u_int i = 0; i < materialsCount; ++i) {
		auto& m = scene.GetMaterials().GetMaterial(i);
		slg::ocl::Material *mat = &mats[i];
		//SLG_LOG(" Type: " << m.GetType());

		mat->matID = m.GetID();
		mat->lightID = m.GetLightID();
        mat->bumpSampleDistance = m.GetBumpSampleDistance();

		// Material transparency
		auto frontTranspTex = m.GetFrontTransparencyTexture();
		if (frontTranspTex) {
			mat->frontTranspTexIndex = scene.GetTextures().GetTextureIndex(frontTranspTex);
		} else
			mat->frontTranspTexIndex = NULL_INDEX;

		auto backTranspTex = m.GetBackTransparencyTexture();
		if (backTranspTex) {
			mat->backTranspTexIndex = scene.GetTextures().GetTextureIndex(backTranspTex);
		} else
			mat->backTranspTexIndex = NULL_INDEX;

		ASSIGN_SPECTRUM(mat->passThroughShadowTransparency, m.GetPassThroughShadowTransparency());
		mat->passThroughShadowTransparencyOverride = m.GetPassThroughShadowTransparencyOverride();

		// Material emission
		auto emitTex = m.GetEmitTexture();
		if (emitTex)
			mat->emitTexIndex = scene.GetTextures().GetTextureIndex(emitTex);
		else
			mat->emitTexIndex = NULL_INDEX;
		ASSIGN_SPECTRUM(mat->emittedFactor, m.GetEmittedFactor());
		mat->emittedCosThetaMax = m.GetEmittedCosThetaMax();
		mat->usePrimitiveArea = m.IsUsingPrimitiveArea();

		// Material bump mapping
		auto bumpTex = m.GetBumpTexture();
		if (bumpTex)
			mat->bumpTexIndex = scene.GetTextures().GetTextureIndex(bumpTex);
		else
			mat->bumpTexIndex = NULL_INDEX;

		mat->visibility =
				(m.IsVisibleIndirectDiffuse() ? DIFFUSE : NONE) |
				(m.IsVisibleIndirectGlossy() ? GLOSSY : NONE) |
				(m.IsVisibleIndirectSpecular() ? SPECULAR : NONE);

		// Material volumes
		auto interiorVol = m.GetInteriorVolume();
		mat->interiorVolumeIndex = interiorVol ? scene.GetMaterials().GetMaterialIndex(interiorVol) : NULL_INDEX;
		auto exteriorVol = m.GetExteriorVolume();
		mat->exteriorVolumeIndex = exteriorVol ? scene.GetMaterials().GetMaterialIndex(exteriorVol) : NULL_INDEX;
		mat->glossiness = m.GetGlossiness();
		mat->avgPassThroughTransparency = m.GetAvgPassThroughTransparency();
		mat->isShadowCatcher = m.IsShadowCatcher();
		mat->isShadowCatcherOnlyInfiniteLights = m.IsShadowCatcherOnlyInfiniteLights();
		mat->isPhotonGIEnabled = m.IsPhotonGIEnabled();
		mat->isHoldout = m.IsHoldout();

		// Bake Material::GetEventTypes() and Material::IsDelta()
		mat->eventTypes = m.GetEventTypes();
		mat->isDelta = m.IsDelta();

		// Material specific parameters
		switch (m.GetType()) {
			case MATTE: {
				auto& mm = dynamic_cast<const MatteMaterial &>(m);

				mat->type = slg::ocl::MATTE;
				mat->matte.kdTexIndex = scene.GetTextures().GetTextureIndex(mm.GetKd());
				break;
			}
			case ROUGHMATTE: {
				auto& mm = dynamic_cast<const RoughMatteMaterial &>(m);

				mat->type = slg::ocl::ROUGHMATTE;
				mat->roughmatte.kdTexIndex = scene.GetTextures().GetTextureIndex(mm.GetKd());
				mat->roughmatte.sigmaTexIndex = scene.GetTextures().GetTextureIndex(mm.GetSigma());
				break;
			}
			case MIRROR: {
				auto& mm = dynamic_cast<const MirrorMaterial &>(m);

				mat->type = slg::ocl::MIRROR;
				mat->mirror.krTexIndex = scene.GetTextures().GetTextureIndex(mm.GetKr());
				break;
			}
			case GLASS: {
				auto& gm = dynamic_cast<const GlassMaterial &>(m);

				mat->type = slg::ocl::GLASS;
				mat->glass.krTexIndex = scene.GetTextures().GetTextureIndex(gm.GetKr());
				mat->glass.ktTexIndex = scene.GetTextures().GetTextureIndex(gm.GetKt());
				if (gm.GetExteriorIOR())
					mat->glass.exteriorIorTexIndex = scene.GetTextures().GetTextureIndex(gm.GetExteriorIOR());
				else
					mat->glass.exteriorIorTexIndex = NULL_INDEX;
				if (gm.GetInteriorIOR())
					mat->glass.interiorIorTexIndex = scene.GetTextures().GetTextureIndex(gm.GetInteriorIOR());
				else
					mat->glass.interiorIorTexIndex = NULL_INDEX;
				if (gm.GetCauchyB())
					mat->glass.cauchyBTex = scene.GetTextures().GetTextureIndex(gm.GetCauchyB());
				else
					mat->glass.cauchyBTex = NULL_INDEX;
				if (gm.GetFilmThickness())
					mat->glass.filmThicknessTexIndex = scene.GetTextures().GetTextureIndex(gm.GetFilmThickness());
				else
					mat->glass.filmThicknessTexIndex = NULL_INDEX;
				if (gm.GetFilmIOR())
					mat->glass.filmIorTexIndex = scene.GetTextures().GetTextureIndex(gm.GetFilmIOR());
				else
					mat->glass.filmIorTexIndex = NULL_INDEX;
				break;
			}
			case ARCHGLASS: {
				auto& am = dynamic_cast<const ArchGlassMaterial &>(m);

				mat->type = slg::ocl::ARCHGLASS;
				mat->archglass.krTexIndex = scene.GetTextures().GetTextureIndex(am.GetKr());
				mat->archglass.ktTexIndex = scene.GetTextures().GetTextureIndex(am.GetKt());
				if (am.GetExteriorIOR())
					mat->archglass.exteriorIorTexIndex = scene.GetTextures().GetTextureIndex(am.GetExteriorIOR());
				else
					mat->archglass.exteriorIorTexIndex = NULL_INDEX;
				if (am.GetInteriorIOR())
					mat->archglass.interiorIorTexIndex = scene.GetTextures().GetTextureIndex(am.GetInteriorIOR());
				else
					mat->archglass.interiorIorTexIndex = NULL_INDEX;
				if (am.GetFilmThickness())
					mat->archglass.filmThicknessTexIndex = scene.GetTextures().GetTextureIndex(am.GetFilmThickness());
				else
					mat->archglass.filmThicknessTexIndex = NULL_INDEX;
				if (am.GetFilmIOR())
					mat->archglass.filmIorTexIndex = scene.GetTextures().GetTextureIndex(am.GetFilmIOR());
				else
					mat->archglass.filmIorTexIndex = NULL_INDEX;
				break;
			}
			case MIX: {
				auto& mm = dynamic_cast<const MixMaterial &>(m);

				mat->type = slg::ocl::MIX;
				mat->mix.matAIndex = scene.GetMaterials().GetMaterialIndex(mm.GetMaterialA());
				mat->mix.matBIndex = scene.GetMaterials().GetMaterialIndex(mm.GetMaterialB());
				mat->mix.mixFactorTexIndex = scene.GetTextures().GetTextureIndex(mm.GetMixFactor());
				break;
			}
			case NULLMAT: {
				mat->type = slg::ocl::NULLMAT;
				break;
			}
			case MATTETRANSLUCENT: {
				auto& mt = dynamic_cast<const MatteTranslucentMaterial &>(m);

				mat->type = slg::ocl::MATTETRANSLUCENT;
				mat->matteTranslucent.krTexIndex = scene.GetTextures().GetTextureIndex(mt.GetKr());
				mat->matteTranslucent.ktTexIndex = scene.GetTextures().GetTextureIndex(mt.GetKt());
				break;
			}
			case ROUGHMATTETRANSLUCENT: {
				auto& rmt = dynamic_cast<const RoughMatteTranslucentMaterial &>(m);

				mat->type = slg::ocl::ROUGHMATTETRANSLUCENT;
				mat->roughmatteTranslucent.krTexIndex = scene.GetTextures().GetTextureIndex(rmt.GetKr());
				mat->roughmatteTranslucent.ktTexIndex = scene.GetTextures().GetTextureIndex(rmt.GetKt());
				mat->roughmatteTranslucent.sigmaTexIndex = scene.GetTextures().GetTextureIndex(rmt.GetSigma());
				break;
			}
			case GLOSSY2: {
				auto& g2m = dynamic_cast<const Glossy2Material &>(m);

				mat->type = slg::ocl::GLOSSY2;
				mat->glossy2.kdTexIndex = scene.GetTextures().GetTextureIndex(g2m.GetKd());
				mat->glossy2.ksTexIndex = scene.GetTextures().GetTextureIndex(g2m.GetKs());

				auto nuTex = g2m.GetNu();
				auto nvTex = g2m.GetNv();
				mat->glossy2.nuTexIndex = scene.GetTextures().GetTextureIndex(nuTex);
				mat->glossy2.nvTexIndex = scene.GetTextures().GetTextureIndex(nvTex);

				auto depthTex = g2m.GetDepth();
				mat->glossy2.kaTexIndex = scene.GetTextures().GetTextureIndex(g2m.GetKa());
				mat->glossy2.depthTexIndex = scene.GetTextures().GetTextureIndex(depthTex);

				auto indexTex = g2m.GetIndex();
				mat->glossy2.indexTexIndex = scene.GetTextures().GetTextureIndex(indexTex);
				mat->glossy2.multibounce = g2m.IsMultibounce () ? 1 : 0;
				mat->glossy2.doublesided = g2m.IsDoubleSided () ? 1 : 0;
				break;
			}
			case METAL2: {
				auto& m2m = dynamic_cast<const Metal2Material &>(m);

				mat->type = slg::ocl::METAL2;
				if (m2m.GetFresnel())
					mat->metal2.fresnelTexIndex = scene.GetTextures().GetTextureIndex(m2m.GetFresnel());
				else
					mat->metal2.fresnelTexIndex = NULL_INDEX;
				if (m2m.GetN())
					mat->metal2.nTexIndex = scene.GetTextures().GetTextureIndex(m2m.GetN());
				else
					mat->metal2.nTexIndex = NULL_INDEX;
				if (m2m.GetK())
					mat->metal2.kTexIndex = scene.GetTextures().GetTextureIndex(m2m.GetK());
				else
					mat->metal2.kTexIndex = NULL_INDEX;

				auto nuTex = m2m.GetNu();
				auto nvTex = m2m.GetNv();
				mat->metal2.nuTexIndex = scene.GetTextures().GetTextureIndex(nuTex);
				mat->metal2.nvTexIndex = scene.GetTextures().GetTextureIndex(nvTex);
				break;
			}
			case ROUGHGLASS: {
				auto& rgm = dynamic_cast<const RoughGlassMaterial &>(m);

				mat->type = slg::ocl::ROUGHGLASS;
				mat->roughglass.krTexIndex = scene.GetTextures().GetTextureIndex(rgm.GetKr());
				mat->roughglass.ktTexIndex = scene.GetTextures().GetTextureIndex(rgm.GetKt());
				if (rgm.GetExteriorIOR())
					mat->roughglass.exteriorIorTexIndex = scene.GetTextures().GetTextureIndex(rgm.GetExteriorIOR());
				else
					mat->roughglass.exteriorIorTexIndex = NULL_INDEX;
				if (rgm.GetInteriorIOR())
					mat->roughglass.interiorIorTexIndex = scene.GetTextures().GetTextureIndex(rgm.GetInteriorIOR());
				else
					mat->roughglass.interiorIorTexIndex = NULL_INDEX;

				auto nuTex = rgm.GetNu();
				auto nvTex = rgm.GetNv();
				mat->roughglass.nuTexIndex = scene.GetTextures().GetTextureIndex(nuTex);
				mat->roughglass.nvTexIndex = scene.GetTextures().GetTextureIndex(nvTex);
				
				if (rgm.GetFilmThickness())
					mat->roughglass.filmThicknessTexIndex = scene.GetTextures().GetTextureIndex(rgm.GetFilmThickness());
				else
					mat->roughglass.filmThicknessTexIndex = NULL_INDEX;
				if (rgm.GetFilmIOR())
					mat->roughglass.filmIorTexIndex = scene.GetTextures().GetTextureIndex(rgm.GetFilmIOR());
				else
					mat->roughglass.filmIorTexIndex = NULL_INDEX;
				break;
			}
			case VELVET: {
				auto& vm = dynamic_cast<const VelvetMaterial &>(m);

				mat->type = slg::ocl::VELVET;
				mat->velvet.kdTexIndex = scene.GetTextures().GetTextureIndex(vm.GetKd());
				mat->velvet.p1TexIndex = scene.GetTextures().GetTextureIndex(vm.GetP1());
				mat->velvet.p2TexIndex = scene.GetTextures().GetTextureIndex(vm.GetP2());
				mat->velvet.p3TexIndex = scene.GetTextures().GetTextureIndex(vm.GetP3());
				mat->velvet.thicknessTexIndex = scene.GetTextures().GetTextureIndex(vm.GetThickness());
				break;
			}
			case CLOTH: {
				auto& cm = dynamic_cast<const ClothMaterial &>(m);

				mat->type = slg::ocl::CLOTH;
				mat->cloth.Preset = cm.GetPreset();
				mat->cloth.Weft_KdIndex = scene.GetTextures().GetTextureIndex(cm.GetWeftKd());
				mat->cloth.Weft_KsIndex = scene.GetTextures().GetTextureIndex(cm.GetWeftKs());
				mat->cloth.Warp_KdIndex = scene.GetTextures().GetTextureIndex(cm.GetWarpKd());
				mat->cloth.Warp_KsIndex = scene.GetTextures().GetTextureIndex(cm.GetWarpKs());
				mat->cloth.Repeat_U = cm.GetRepeatU();
				mat->cloth.Repeat_V = cm.GetRepeatV();
				mat->cloth.specularNormalization = cm.GetSpecularNormalization();
				break;
			}
			case CARPAINT: {
				auto& cm = dynamic_cast<const CarPaintMaterial &>(m);
				mat->type = slg::ocl::CARPAINT;
				mat->carpaint.KdTexIndex = scene.GetTextures().GetTextureIndex(cm.Kd);
				mat->carpaint.Ks1TexIndex = scene.GetTextures().GetTextureIndex(cm.Ks1);
				mat->carpaint.Ks2TexIndex = scene.GetTextures().GetTextureIndex(cm.Ks2);
				mat->carpaint.Ks3TexIndex = scene.GetTextures().GetTextureIndex(cm.Ks3);
				mat->carpaint.M1TexIndex = scene.GetTextures().GetTextureIndex(cm.M1);
				mat->carpaint.M2TexIndex = scene.GetTextures().GetTextureIndex(cm.M2);
				mat->carpaint.M3TexIndex = scene.GetTextures().GetTextureIndex(cm.M3);
				mat->carpaint.R1TexIndex = scene.GetTextures().GetTextureIndex(cm.R1);
				mat->carpaint.R2TexIndex = scene.GetTextures().GetTextureIndex(cm.R2);
				mat->carpaint.R3TexIndex = scene.GetTextures().GetTextureIndex(cm.R3);
				mat->carpaint.KaTexIndex = scene.GetTextures().GetTextureIndex(cm.Ka);
				mat->carpaint.depthTexIndex = scene.GetTextures().GetTextureIndex(cm.depth);
				break;
			}
			case GLOSSYTRANSLUCENT: {
				auto& gtm = dynamic_cast<const GlossyTranslucentMaterial &>(m);

				mat->type = slg::ocl::GLOSSYTRANSLUCENT;
				mat->glossytranslucent.kdTexIndex = scene.GetTextures().GetTextureIndex(gtm.GetKd());
				mat->glossytranslucent.ktTexIndex = scene.GetTextures().GetTextureIndex(gtm.GetKt());
				mat->glossytranslucent.ksTexIndex = scene.GetTextures().GetTextureIndex(gtm.GetKs());
				mat->glossytranslucent.ksbfTexIndex = scene.GetTextures().GetTextureIndex(gtm.GetKs_bf());

				auto nuTex = gtm.GetNu();
				auto nvTex = gtm.GetNv();
				mat->glossytranslucent.nuTexIndex = scene.GetTextures().GetTextureIndex(nuTex);
				mat->glossytranslucent.nvTexIndex = scene.GetTextures().GetTextureIndex(nvTex);
				auto nubfTex = gtm.GetNu_bf();
				auto nvbfTex = gtm.GetNv_bf();
				mat->glossytranslucent.nubfTexIndex = scene.GetTextures().GetTextureIndex(nubfTex);
				mat->glossytranslucent.nvbfTexIndex = scene.GetTextures().GetTextureIndex(nvbfTex);

				auto depthTex = gtm.GetDepth();
				mat->glossytranslucent.kaTexIndex = scene.GetTextures().GetTextureIndex(gtm.GetKa());
				mat->glossytranslucent.depthTexIndex = scene.GetTextures().GetTextureIndex(depthTex);
				mat->glossytranslucent.kabfTexIndex = scene.GetTextures().GetTextureIndex(gtm.GetKa_bf());
				mat->glossytranslucent.depthbfTexIndex = scene.GetTextures().GetTextureIndex(depthTex);

				auto indexTex = gtm.GetIndex();
				mat->glossytranslucent.indexTexIndex = scene.GetTextures().GetTextureIndex(indexTex);
				mat->glossytranslucent.indexbfTexIndex = scene.GetTextures().GetTextureIndex(indexTex);

				mat->glossytranslucent.multibounce = gtm.IsMultibounce() ? 1 : 0;
				mat->glossytranslucent.multibouncebf = gtm.IsMultibounce_bf() ? 1 : 0;
				break;
			}
			case GLOSSYCOATING: {
				auto& gcm = dynamic_cast<const GlossyCoatingMaterial &>(m);

				mat->type = slg::ocl::GLOSSYCOATING;
				mat->glossycoating.matBaseIndex = scene.GetMaterials().GetMaterialIndex(gcm.GetMaterialBase());
				mat->glossycoating.ksTexIndex = scene.GetTextures().GetTextureIndex(gcm.GetKs());

				auto nuTex = gcm.GetNu();
				auto nvTex = gcm.GetNv();
				mat->glossycoating.nuTexIndex = scene.GetTextures().GetTextureIndex(nuTex);
				mat->glossycoating.nvTexIndex = scene.GetTextures().GetTextureIndex(nvTex);

				auto depthTex = gcm.GetDepth();
				mat->glossycoating.kaTexIndex = scene.GetTextures().GetTextureIndex(gcm.GetKa());
				mat->glossycoating.depthTexIndex = scene.GetTextures().GetTextureIndex(depthTex);

				auto indexTex = gcm.GetIndex();
				mat->glossycoating.indexTexIndex = scene.GetTextures().GetTextureIndex(indexTex);
				mat->glossycoating.multibounce = gcm.IsMultibounce() ? 1 : 0;
				break;
			}
			case DISNEY: {
				auto& dm = dynamic_cast<const DisneyMaterial &>(m);

				mat->type = slg::ocl::DISNEY;
				mat->disney.baseColorTexIndex = scene.GetTextures().GetTextureIndex(dm.GetBaseColor());
				mat->disney.subsurfaceTexIndex = scene.GetTextures().GetTextureIndex(dm.GetSubsurface());
				mat->disney.roughnessTexIndex = scene.GetTextures().GetTextureIndex(dm.GetRoughness());
				mat->disney.metallicTexIndex = scene.GetTextures().GetTextureIndex(dm.GetMetallic());
				mat->disney.specularTexIndex = scene.GetTextures().GetTextureIndex(dm.GetSpecular());
				mat->disney.specularTintTexIndex = scene.GetTextures().GetTextureIndex(dm.GetSpecularTint());
				mat->disney.clearcoatTexIndex = scene.GetTextures().GetTextureIndex(dm.GetClearcoat());
				mat->disney.clearcoatGlossTexIndex = scene.GetTextures().GetTextureIndex(dm.GetClearcoatGloss());
				mat->disney.anisotropicTexIndex = scene.GetTextures().GetTextureIndex(dm.GetAnisotropic());
				mat->disney.sheenTexIndex = scene.GetTextures().GetTextureIndex(dm.GetSheen());
				mat->disney.sheenTintTexIndex = scene.GetTextures().GetTextureIndex(dm.GetSheenTint());
				if (dm.GetFilmAmount())
					mat->disney.filmAmountTexIndex = scene.GetTextures().GetTextureIndex(dm.GetFilmAmount());
				else
					mat->disney.filmAmountTexIndex = NULL_INDEX;
				if (dm.GetFilmThickness())
					mat->disney.filmThicknessTexIndex = scene.GetTextures().GetTextureIndex(dm.GetFilmThickness());
				else
					mat->disney.filmThicknessTexIndex = NULL_INDEX;
				if (dm.GetFilmIOR())
					mat->disney.filmIorTexIndex = scene.GetTextures().GetTextureIndex(dm.GetFilmIOR());
				else
					mat->disney.filmIorTexIndex = NULL_INDEX;
				break;
			}
			case TWOSIDED: {
				auto& tsm = dynamic_cast<const TwoSidedMaterial &>(m);

				mat->type = slg::ocl::TWOSIDED;
				mat->twosided.frontMatIndex = scene.GetMaterials().GetMaterialIndex(tsm.GetFrontMaterial());
				mat->twosided.backMatIndex = scene.GetMaterials().GetMaterialIndex(tsm.GetBackMaterial());
				break;
			}
			//------------------------------------------------------------------
			// Volumes
			//------------------------------------------------------------------
			case CLEAR_VOL:
			case HOMOGENEOUS_VOL:
			case HETEROGENEOUS_VOL: {
				auto& v = dynamic_cast<const Volume &>(m);
				mat->volume.iorTexIndex = v.HasIORTexture() ?
					scene.GetTextures().GetTextureIndex(v.GetIORTexture()) :
					NULL_INDEX;
				mat->volume.volumeEmissionTexIndex = v.HasVolumeEmissionTexture() ?
					scene.GetTextures().GetTextureIndex(v.GetVolumeEmissionTexture()) :
					NULL_INDEX;
				mat->volume.volumeLightID = v.GetVolumeLightID();
				mat->volume.priority = v.GetPriority();

				switch (m.GetType()) {
					case CLEAR_VOL: {
						auto& cv = dynamic_cast<const ClearVolume &>(m);
						mat->type = slg::ocl::CLEAR_VOL;
						mat->volume.clear.sigmaATexIndex = scene.GetTextures().GetTextureIndex(cv.GetSigmaA());
						break;
					}
					case HOMOGENEOUS_VOL: {
						auto& hv = dynamic_cast<const HomogeneousVolume &>(m);
						mat->type = slg::ocl::HOMOGENEOUS_VOL;
						mat->volume.homogenous.sigmaATexIndex = scene.GetTextures().GetTextureIndex(hv.GetSigmaA());
						mat->volume.homogenous.sigmaSTexIndex = scene.GetTextures().GetTextureIndex(hv.GetSigmaS());
						mat->volume.homogenous.gTexIndex = scene.GetTextures().GetTextureIndex(hv.GetG());
						mat->volume.homogenous.multiScattering = hv.IsMultiScattering();
						break;
					}
					case HETEROGENEOUS_VOL: {
						auto& hv = dynamic_cast<const HeterogeneousVolume &>(m);
						mat->type = slg::ocl::HETEROGENEOUS_VOL;
						mat->volume.heterogenous.sigmaATexIndex = scene.GetTextures().GetTextureIndex(hv.GetSigmaA());
						mat->volume.heterogenous.sigmaSTexIndex = scene.GetTextures().GetTextureIndex(hv.GetSigmaS());
						mat->volume.heterogenous.gTexIndex = scene.GetTextures().GetTextureIndex(hv.GetG());
						mat->volume.heterogenous.stepSize = hv.GetStepSize();
						mat->volume.heterogenous.maxStepsCount = hv.GetMaxStepsCount();
						mat->volume.heterogenous.multiScattering = hv.IsMultiScattering();
						break;
					}
					default:
						throw runtime_error("Unknown volume in CompiledScene::CompileMaterials(): " + ToString(m.GetType()));
				}
				break;
			}
			default:
				throw runtime_error("Unknown material in CompiledScene::CompileMaterials(): " + ToString(m.GetType()));
		}
	}

	//--------------------------------------------------------------------------
	// Material evaluation ops
	//--------------------------------------------------------------------------

	CompileMaterialOps();

	//--------------------------------------------------------------------------
	// Default scene volume
	//--------------------------------------------------------------------------

	defaultWorldVolumeIndex = scene.HasDefaultWorldVolume() ?
		scene.GetMaterials().GetMaterialIndex(scene.GetDefaultWorldVolume()) : NULL_INDEX;

	const double tEnd = WallClockTime();
	SLG_LOG("Material compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

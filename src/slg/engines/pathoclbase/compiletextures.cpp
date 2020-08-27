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

#include "slg/textures/band.h"
#include "slg/textures/bilerp.h"
#include "slg/textures/blackbody.h"
#include "slg/textures/blender_texture.h"
#include "slg/textures/brick.h"
#include "slg/textures/brightcontrast.h"
#include "slg/textures/checkerboard.h"
#include "slg/textures/colordepth.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"
#include "slg/textures/cloud.h"
#include "slg/textures/densitygrid.h"
#include "slg/textures/distort.h"
#include "slg/textures/dots.h"
#include "slg/textures/fbm.h"
#include "slg/textures/fresnelapprox.h"
#include "slg/textures/fresnel/fresnelcolor.h"
#include "slg/textures/fresnel/fresnelconst.h"
#include "slg/textures/fresnel/fresnelluxpop.h"
#include "slg/textures/fresnel/fresnelpreset.h"
#include "slg/textures/fresnel/fresnelsopra.h"
#include "slg/textures/fresnel/fresneltexture.h"
#include "slg/textures/hitpoint/hitpointaov.h"
#include "slg/textures/hitpoint/hitpointcolor.h"
#include "slg/textures/hitpoint/position.h"
#include "slg/textures/hitpoint/shadingnormal.h"
#include "slg/textures/hsv.h"
#include "slg/textures/imagemaptex.h"
#include "slg/textures/irregulardata.h"
#include "slg/textures/lampspectrum.h"
#include "slg/textures/marble.h"
#include "slg/textures/math/abs.h"
#include "slg/textures/math/add.h"
#include "slg/textures/math/clamp.h"
#include "slg/textures/math/divide.h"
#include "slg/textures/math/greaterthan.h"
#include "slg/textures/math/lessthan.h"
#include "slg/textures/math/mix.h"
#include "slg/textures/math/modulo.h"
#include "slg/textures/math/power.h"
#include "slg/textures/math/random.h"
#include "slg/textures/math/remap.h"
#include "slg/textures/math/rounding.h"
#include "slg/textures/math/scale.h"
#include "slg/textures/math/subtract.h"
#include "slg/textures/normalmap.h"
#include "slg/textures/object_id.h"
#include "slg/textures/triplanar.h"
#include "slg/textures/uv.h"
#include "slg/textures/vectormath/dotproduct.h"
#include "slg/textures/vectormath/makefloat3.h"
#include "slg/textures/vectormath/splitfloat3.h"
#include "slg/textures/windy.h"
#include "slg/textures/wireframe.h"
#include "slg/textures/wrinkled.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace slg::blender;

void CompiledScene::CompileTextureMapping2D(slg::ocl::TextureMapping2D *mapping, const TextureMapping2D *m) {
	switch (m->GetType()) {
		case UVMAPPING2D: {
			mapping->type = slg::ocl::UVMAPPING2D;

			const UVMapping2D *uvm = static_cast<const UVMapping2D *>(m);
			mapping->dataIndex = uvm->GetDataIndex();

			mapping->uvMapping2D.sinTheta = uvm->sinTheta;
			mapping->uvMapping2D.cosTheta = uvm->cosTheta;
			mapping->uvMapping2D.uScale = uvm->uScale;
			mapping->uvMapping2D.vScale = uvm->vScale;
			mapping->uvMapping2D.uDelta = uvm->uDelta;
			mapping->uvMapping2D.vDelta = uvm->vDelta;
			break;
		}
		default:
			throw runtime_error("Unknown 2D texture mapping in CompiledScene::CompileTextureMapping2D: " + ToString(m->GetType()));
	}
}

void CompiledScene::CompileTextureMapping3D(slg::ocl::TextureMapping3D *mapping, const TextureMapping3D *m) {
	switch (m->GetType()) {
		case UVMAPPING3D: {
			mapping->type = slg::ocl::UVMAPPING3D;

			const UVMapping3D *uvm = static_cast<const UVMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &uvm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &uvm->worldToLocal.mInv, sizeof(float[4][4]));

			mapping->uvMapping3D.dataIndex = uvm->GetDataIndex();
			break;
		}
		case GLOBALMAPPING3D: {
			mapping->type = slg::ocl::GLOBALMAPPING3D;

			const GlobalMapping3D *gm = static_cast<const GlobalMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &gm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &gm->worldToLocal.mInv, sizeof(float[4][4]));
			break;
		}
		case LOCALMAPPING3D: {
			mapping->type = slg::ocl::LOCALMAPPING3D;

			const LocalMapping3D *gm = static_cast<const LocalMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &gm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &gm->worldToLocal.mInv, sizeof(float[4][4]));
			break;
		}
		default:
			throw runtime_error("Unknown 3D texture mapping in CompiledScene::CompileTextureMapping3D: " + ToString(m->GetType()));
	}
}

float *CompiledScene::CompileDistribution1D(const Distribution1D *dist, u_int *size) {
	const u_int count = dist->GetCount();

	// Here, I assume sizeof(u_int) = sizeof(float)
	*size = sizeof(u_int) + count * sizeof(float) + (count + 1) * sizeof(float);
	// Size is expressed in bytes while I'm working with float
	float *compDist = new float[*size / sizeof(float)];

	*((u_int *)&compDist[0]) = count;
	copy(dist->GetFuncs(), dist->GetFuncs() + count,
			compDist + 1);
	copy(dist->GetCDFs(), dist->GetCDFs() + count + 1,
			compDist + 1 + count);

	return compDist;
}

float *CompiledScene::CompileDistribution2D(const Distribution2D *dist, u_int *size) {
	u_int marginalSize;
	float *marginalDist = CompileDistribution1D(dist->GetMarginalDistribution(),
			&marginalSize);

	u_int condSize;
	vector<float *> condDists;
	for (u_int i = 0; i < dist->GetHeight(); ++i) {
		condDists.push_back(CompileDistribution1D(dist->GetConditionalDistribution(i),
			&condSize));
	}

	// Here, I assume sizeof(u_int) = sizeof(float)
	*size = 2 * sizeof(u_int) + marginalSize + condDists.size() * condSize;
	// Size is expressed in bytes while I'm working with float
	float *compDist = new float[*size / sizeof(float)];

	*((u_int *)&compDist[0]) = dist->GetWidth();
	*((u_int *)&compDist[1]) = dist->GetHeight();

	float *ptr = &compDist[2];
	const u_int marginalSize4 = marginalSize / sizeof(float);
	copy(marginalDist, marginalDist + marginalSize4, ptr);
	ptr += marginalSize4;
	delete[] marginalDist;

	const u_int condSize4 = condSize / sizeof(float);
	for (u_int i = 0; i < dist->GetHeight(); ++i) {
		copy(condDists[i], condDists[i] + condSize4, ptr);
		ptr += condSize4;
		delete[] condDists[i];
	}

	return compDist;
}

u_int CompiledScene::CompileTextureOpsGenericBumpMap(const u_int texIndex) {
	u_int evalOpStackSize = 0;

	// Eval texture at hit point
	evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);

	// EVAL_BUMP_GENERIC_OFFSET_U
	slg::ocl::TextureEvalOp opOffsetU;
	opOffsetU.texIndex = texIndex;
	opOffsetU.evalType = slg::ocl::TextureEvalOpType::EVAL_BUMP_GENERIC_OFFSET_U;
	texEvalOps.push_back(opOffsetU);
	// Save original original P, shadeN and UV
	evalOpStackSize += 3 + 3 + 2;

	// Eval texture at hit point + offset U
	evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);

	// EVAL_BUMP_GENERIC_OFFSET_V
	slg::ocl::TextureEvalOp opOffsetV;
	opOffsetV.texIndex = texIndex;
	opOffsetV.evalType = slg::ocl::TextureEvalOpType::EVAL_BUMP_GENERIC_OFFSET_V;
	texEvalOps.push_back(opOffsetV);

	// Eval texture at hit point + offset V
	evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
	
	return evalOpStackSize;
}
					
u_int CompiledScene::CompileTextureOps(const u_int texIndex,
		const slg::ocl::TextureEvalOpType opType) {
	// Translate textures to texture evaluate ops

	slg::ocl::Texture *tex = &texs[texIndex];
	u_int evalOpStackSize = 0;

	switch (tex->type) {
		//----------------------------------------------------------------------
		// Textures without sub-nodes and with fixed bump map evaluation
		//----------------------------------------------------------------------
		case slg::ocl::CONST_FLOAT:
		case slg::ocl::CONST_FLOAT3:
		case slg::ocl::IMAGEMAP:
		case slg::ocl::BLACKBODY_TEX:
		case slg::ocl::IRREGULARDATA_TEX:
		case slg::ocl::OBJECTID_TEX:
		case slg::ocl::OBJECTID_COLOR_TEX:
		case slg::ocl::OBJECTID_NORMALIZED_TEX:
		case slg::ocl::FRESNELCOLOR_TEX:
		case slg::ocl::FRESNELCONST_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
					evalOpStackSize += 1;
					break;
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM:
					evalOpStackSize += 3;
					break;
				case slg::ocl::TextureEvalOpType::EVAL_BUMP:
					evalOpStackSize += 3;
					break;
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		//----------------------------------------------------------------------
		// Not constant textures without sub-nodes
		//----------------------------------------------------------------------
		case slg::ocl::HITPOINTCOLOR:
		case slg::ocl::HITPOINTALPHA:
		case slg::ocl::HITPOINTGREY:
		case slg::ocl::HITPOINTVERTEXAOV:
		case slg::ocl::HITPOINTTRIANGLEAOV:
		case slg::ocl::CLOUD_TEX:
		case slg::ocl::FBM_TEX:
		case slg::ocl::MARBLE:
		case slg::ocl::WINDY:
		case slg::ocl::WRINKLED:
		case slg::ocl::UV_TEX:
		case slg::ocl::DENSITYGRID_TEX:
		case slg::ocl::SHADING_NORMAL_TEX:
		case slg::ocl::POSITION_TEX:
		case slg::ocl::BLENDER_BLEND:
		case slg::ocl::BLENDER_CLOUDS:
		case slg::ocl::BLENDER_DISTORTED_NOISE:
		case slg::ocl::BLENDER_MAGIC:
		case slg::ocl::BLENDER_MARBLE:
		case slg::ocl::BLENDER_MUSGRAVE:
		case slg::ocl::BLENDER_NOISE:
		case slg::ocl::BLENDER_STUCCI:
		case slg::ocl::BLENDER_WOOD:
		case slg::ocl::BLENDER_VORONOI: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
					evalOpStackSize += 1;
					break;
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM:
					evalOpStackSize += 3;
					break;
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		//----------------------------------------------------------------------
		// Not constant textures with sub-nodes
		//----------------------------------------------------------------------
		case slg::ocl::SCALE_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->scaleTex.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->scaleTex.tex2Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOps(tex->scaleTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);
					evalOpStackSize += CompileTextureOps(tex->scaleTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);

					evalOpStackSize += CompileTextureOps(tex->scaleTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->scaleTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::FRESNEL_APPROX_N: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->fresnelApproxN.texIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::FRESNEL_APPROX_K: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->fresnelApproxK.texIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::MIX_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->mixTex.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->mixTex.tex2Index, opType);
					evalOpStackSize += CompileTextureOps(tex->mixTex.amountTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOps(tex->mixTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);
					evalOpStackSize += CompileTextureOps(tex->mixTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);
					evalOpStackSize += CompileTextureOps(tex->mixTex.amountTexIndex, slg::ocl::TextureEvalOpType::EVAL_BUMP);

					evalOpStackSize += CompileTextureOps(tex->mixTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->mixTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->mixTex.amountTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::ADD_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->addTex.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->addTex.tex2Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOps(tex->addTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);
					evalOpStackSize += CompileTextureOps(tex->addTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);

					evalOpStackSize += CompileTextureOps(tex->addTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->addTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::SUBTRACT_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->subtractTex.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->subtractTex.tex2Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOps(tex->subtractTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);
					evalOpStackSize += CompileTextureOps(tex->subtractTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_BUMP);

					evalOpStackSize += CompileTextureOps(tex->subtractTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->subtractTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::ABS_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->absTex.texIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::CLAMP_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->clampTex.texIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::BILERP_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->bilerpTex.t00Index, opType);
					evalOpStackSize += CompileTextureOps(tex->bilerpTex.t01Index, opType);
					evalOpStackSize += CompileTextureOps(tex->bilerpTex.t10Index, opType);
					evalOpStackSize += CompileTextureOps(tex->bilerpTex.t11Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::COLORDEPTH_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->colorDepthTex.ktIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::HSV_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->hsvTex.texIndex, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					evalOpStackSize += CompileTextureOps(tex->hsvTex.hueTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->hsvTex.satTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->hsvTex.valTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::DIVIDE_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->divideTex.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->divideTex.tex2Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::REMAP_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->remapTex.valueTexIndex, opType);
					evalOpStackSize += CompileTextureOps(tex->remapTex.sourceMinTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->remapTex.sourceMaxTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->remapTex.targetMinTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->remapTex.targetMaxTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::DOT_PRODUCT_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->dotProductTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					evalOpStackSize += CompileTextureOps(tex->dotProductTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::POWER_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->powerTex.baseTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->powerTex.exponentTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::LESS_THAN_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->lessThanTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->lessThanTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::GREATER_THAN_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->greaterThanTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->greaterThanTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::ROUNDING_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->roundingTex.textureIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->roundingTex.incrementIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::MODULO_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->moduloTex.textureIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->moduloTex.moduloIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::SPLIT_FLOAT3: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->splitFloat3Tex.texIndex, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::MAKE_FLOAT3: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->makeFloat3Tex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->makeFloat3Tex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->makeFloat3Tex.tex3Index, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::BRIGHT_CONTRAST_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->brightContrastTex.texIndex, opType);
					evalOpStackSize += CompileTextureOps(tex->brightContrastTex.brightnessTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					evalOpStackSize += CompileTextureOps(tex->brightContrastTex.contrastTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::NORMALMAP_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
					evalOpStackSize += 1;
					break;
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM:
					evalOpStackSize += 3;
					break;
				case slg::ocl::TextureEvalOpType::EVAL_BUMP:
						evalOpStackSize += CompileTextureOps(tex->normalMap.texIndex, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					break;
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::TRIPLANAR_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					// EVAL_TRIPLANAR_STEP_1
					slg::ocl::TextureEvalOp opStep1;
					opStep1.texIndex = texIndex;
					opStep1.evalType = slg::ocl::TextureEvalOpType::EVAL_TRIPLANAR_STEP_1;
					texEvalOps.push_back(opStep1);
					// Save original UV + 3 weights + localPoint
					evalOpStackSize += 2 + 3 + 3;

					// Eval first texture 
					evalOpStackSize += CompileTextureOps(tex->triplanarTex.tex1Index, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);

					// EVAL_TRIPLANAR_STEP_2
					slg::ocl::TextureEvalOp opStep2;
					opStep2.texIndex = texIndex;
					opStep2.evalType = slg::ocl::TextureEvalOpType::EVAL_TRIPLANAR_STEP_2;
					texEvalOps.push_back(opStep2);

					// Eval second texture 
					evalOpStackSize += CompileTextureOps(tex->triplanarTex.tex2Index, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);

					// EVAL_TRIPLANAR_STEP_3
					slg::ocl::TextureEvalOp opStep3;
					opStep3.texIndex = texIndex;
					opStep3.evalType = slg::ocl::TextureEvalOpType::EVAL_TRIPLANAR_STEP_3;
					texEvalOps.push_back(opStep3);

					// Eval last texture 
					evalOpStackSize += CompileTextureOps(tex->triplanarTex.tex3Index, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP:
					if (tex->triplanarTex.enableUVlessBumpMap) {
						// Eval original texture 
						evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);

						// EVAL_BUMP_TRIPLANAR_STEP_1
						slg::ocl::TextureEvalOp opStep1;
						opStep1.texIndex = texIndex;
						opStep1.evalType = slg::ocl::TextureEvalOpType::EVAL_BUMP_TRIPLANAR_STEP_1;
						texEvalOps.push_back(opStep1);
						// Save original localPoint
						evalOpStackSize += 3;

						// Eval first texture 
						evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					
						// EVAL_BUMP_TRIPLANAR_STEP_2
						slg::ocl::TextureEvalOp opStep2;
						opStep2.texIndex = texIndex;
						opStep2.evalType = slg::ocl::TextureEvalOpType::EVAL_BUMP_TRIPLANAR_STEP_2;
						texEvalOps.push_back(opStep2);

						// Eval second texture 
						evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);

						// EVAL_BUMP_TRIPLANAR_STEP_3
						slg::ocl::TextureEvalOp opStep3;
						opStep3.texIndex = texIndex;
						opStep3.evalType = slg::ocl::TextureEvalOpType::EVAL_BUMP_TRIPLANAR_STEP_3;
						texEvalOps.push_back(opStep3);

						// Eval last texture 
						evalOpStackSize += CompileTextureOps(texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					} else {
						// Use generic bump map evaluation path
						evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					}
					break;
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
		}
		case slg::ocl::DISTORT_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					// Eval offset texture
					evalOpStackSize += CompileTextureOps(tex->distortTex.offsetTexIndex, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);

					// EVAL_DISTORT_SETUP
					slg::ocl::TextureEvalOp opSetUp;
					opSetUp.texIndex = texIndex;
					opSetUp.evalType = slg::ocl::TextureEvalOpType::EVAL_DISTORT_SETUP;
					texEvalOps.push_back(opSetUp);
					// Save original P and UV
					evalOpStackSize += 3 + 2;

					// Eval second texture
					evalOpStackSize += CompileTextureOps(tex->distortTex.texIndex, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP:
					// Use generic bump map evaluation path
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::CHECKERBOARD2D: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->checkerBoard2D.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->checkerBoard2D.tex2Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::CHECKERBOARD3D: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->checkerBoard3D.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->checkerBoard3D.tex2Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::DOTS: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->dots.insideIndex, opType);
					evalOpStackSize += CompileTextureOps(tex->dots.outsideIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::BRICK: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->brick.tex1Index, opType);
					evalOpStackSize += CompileTextureOps(tex->brick.tex2Index, opType);
					evalOpStackSize += CompileTextureOps(tex->brick.tex3Index, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::BAND_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->band.amountTexIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::RANDOM_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->randomTex.texIndex, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		case slg::ocl::WIREFRAME_TEX: {
			switch (opType) {
				case slg::ocl::TextureEvalOpType::EVAL_FLOAT:
				case slg::ocl::TextureEvalOpType::EVAL_SPECTRUM: {
					evalOpStackSize += CompileTextureOps(tex->wireFrameTex.borderTexIndex, opType);
					evalOpStackSize += CompileTextureOps(tex->wireFrameTex.insideTexIndex, opType);
					break;
				}
				case slg::ocl::TextureEvalOpType::EVAL_BUMP: {
					evalOpStackSize += CompileTextureOpsGenericBumpMap(texIndex);
					break;
				}
				default:
					throw runtime_error("Unknown op. type in CompiledScene::CompileTextureOps(" + ToString(tex->type) + "): " + ToString(opType));
			}
			break;
		}
		default:
			throw runtime_error("Unknown texture in CompiledScene::CompileTextureOps(" + ToString(opType) + "): " + ToString(tex->type));
	}

	slg::ocl::TextureEvalOp op;

	op.texIndex = texIndex;
	op.evalType = opType;

	texEvalOps.push_back(op);

	return evalOpStackSize;
}

void CompiledScene::CompileTextureOps() {
	// Translate textures to texture evaluate ops

	texEvalOps.clear();
	maxTextureEvalStackSize = 0;

	for (u_int i = 0; i < texs.size(); ++i) {
		slg::ocl::Texture *tex = &texs[i];

		//----------------------------------------------------------------------
		// EVAL_FLOAT
		//----------------------------------------------------------------------
		
		tex->evalFloatOpStartIndex = texEvalOps.size();
		const u_int evalOpsStackSizeFloat = CompileTextureOps(i, slg::ocl::TextureEvalOpType::EVAL_FLOAT);
		tex->evalFloatOpLength = texEvalOps.size() - tex->evalFloatOpStartIndex;

		maxTextureEvalStackSize = Max(maxTextureEvalStackSize, evalOpsStackSizeFloat);

		//----------------------------------------------------------------------
		// EVAL_SPECTRUM
		//----------------------------------------------------------------------

		tex->evalSpectrumOpStartIndex = texEvalOps.size();
		const u_int evalOpsStackSizeSpectrum = CompileTextureOps(i, slg::ocl::TextureEvalOpType::EVAL_SPECTRUM);
		tex->evalSpectrumOpLength = texEvalOps.size() - tex->evalSpectrumOpStartIndex;

		maxTextureEvalStackSize = Max(maxTextureEvalStackSize, evalOpsStackSizeSpectrum);

		//----------------------------------------------------------------------
		// EVAL_BUMP
		//----------------------------------------------------------------------

		tex->evalBumpOpStartIndex = texEvalOps.size();
		const u_int evalOpsStackSizeBump = CompileTextureOps(i, slg::ocl::TextureEvalOpType::EVAL_BUMP);
		tex->evalBumpOpLength = texEvalOps.size() - tex->evalBumpOpStartIndex;

		maxTextureEvalStackSize = Max(maxTextureEvalStackSize, evalOpsStackSizeBump);
	}

	SLG_LOG("Texture evaluation ops count: " << texEvalOps.size());
	SLG_LOG("Texture evaluation max. stack size: " << maxTextureEvalStackSize);
}

void CompiledScene::CompileTextures() {
	wasMaterialsCompiled = true;

	const u_int texturesCount = scene->texDefs.GetSize();
	SLG_LOG("Compile " << texturesCount << " Textures");
	//SLG_LOG("  Texture size: " << sizeof(slg::ocl::Texture));

	//--------------------------------------------------------------------------
	// Translate textures
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	texs.resize(texturesCount);

	for (u_int i = 0; i < texturesCount; ++i) {
		const Texture *t = scene->texDefs.GetTexture(i);
		slg::ocl::Texture *tex = &texs[i];

		switch (t->GetType()) {
			case CONST_FLOAT: {
				const ConstFloatTexture *cft = static_cast<const ConstFloatTexture *>(t);

				tex->type = slg::ocl::CONST_FLOAT;
				tex->constFloat.value = cft->GetValue();
				break;
			}
			case CONST_FLOAT3: {
				const ConstFloat3Texture *cft = static_cast<const ConstFloat3Texture *>(t);

				tex->type = slg::ocl::CONST_FLOAT3;
				ASSIGN_SPECTRUM(tex->constFloat3.color, cft->GetColor());
				break;
			}
			case IMAGEMAP: {
				const ImageMapTexture *imt = static_cast<const ImageMapTexture *>(t);

				tex->type = slg::ocl::IMAGEMAP;
				const ImageMap *im = imt->GetImageMap();
				tex->imageMapTex.gain = imt->GetGain();
				CompileTextureMapping2D(&tex->imageMapTex.mapping, imt->GetTextureMapping());
				tex->imageMapTex.imageMapIndex = scene->imgMapCache.GetImageMapIndex(im);
				break;
			}
			case SCALE_TEX: {
				const ScaleTexture *st = static_cast<const ScaleTexture *>(t);

				tex->type = slg::ocl::SCALE_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->scaleTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->scaleTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case FRESNEL_APPROX_N: {
				const FresnelApproxNTexture *ft = static_cast<const FresnelApproxNTexture *>(t);

				tex->type = slg::ocl::FRESNEL_APPROX_N;
				const Texture *tx = ft->GetTexture();
				tex->fresnelApproxN.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case FRESNEL_APPROX_K: {
				const FresnelApproxKTexture *ft = static_cast<const FresnelApproxKTexture *>(t);

				tex->type = slg::ocl::FRESNEL_APPROX_K;
				const Texture *tx = ft->GetTexture();
				tex->fresnelApproxK.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case CHECKERBOARD2D: {
				const CheckerBoard2DTexture *cb = static_cast<const CheckerBoard2DTexture *>(t);

				tex->type = slg::ocl::CHECKERBOARD2D;
				CompileTextureMapping2D(&tex->checkerBoard2D.mapping, cb->GetTextureMapping());
				const Texture *tex1 = cb->GetTexture1();
				tex->checkerBoard2D.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = cb->GetTexture2();
				tex->checkerBoard2D.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case CHECKERBOARD3D: {
				const CheckerBoard3DTexture *cb = static_cast<const CheckerBoard3DTexture *>(t);

				tex->type = slg::ocl::CHECKERBOARD3D;
				CompileTextureMapping3D(&tex->checkerBoard3D.mapping, cb->GetTextureMapping());
				const Texture *tex1 = cb->GetTexture1();
				tex->checkerBoard3D.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = cb->GetTexture2();
				tex->checkerBoard3D.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case MIX_TEX: {
				const MixTexture *mt = static_cast<const MixTexture *>(t);

				tex->type = slg::ocl::MIX_TEX;
				const Texture *amount = mt->GetAmountTexture();
				tex->mixTex.amountTexIndex = scene->texDefs.GetTextureIndex(amount);

				const Texture *tex1 = mt->GetTexture1();
				tex->mixTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);
				const Texture *tex2 = mt->GetTexture2();
				tex->mixTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case CLOUD_TEX: {
				const CloudTexture *ft = static_cast<const CloudTexture *>(t);

				tex->type = slg::ocl::CLOUD_TEX;
				CompileTextureMapping3D(&tex->cloud.mapping, ft->GetTextureMapping());

				tex->cloud.radius = ft->GetRadius();
				tex->cloud.numspheres = ft->GetNumSpheres();
				tex->cloud.spheresize = ft->GetSphereSize();
				tex->cloud.sharpness = ft->GetSharpness();
				tex->cloud.basefadedistance = ft->GetBaseFadeDistance();
				tex->cloud.baseflatness = ft->GetBaseFlatness();
				tex->cloud.variability = ft->GetVariability();
				tex->cloud.omega = ft->GetOmega();
				tex->cloud.noisescale = ft->GetNoiseScale();
				tex->cloud.noiseoffset = ft->GetNoiseOffset();
				tex->cloud.turbulence = ft->GetTurbulenceAmount();
				tex->cloud.octaves = ft->GetNumOctaves();
				break;
			}
			case FBM_TEX: {
				const FBMTexture *ft = static_cast<const FBMTexture *>(t);

				tex->type = slg::ocl::FBM_TEX;
				CompileTextureMapping3D(&tex->fbm.mapping, ft->GetTextureMapping());
				tex->fbm.octaves = ft->GetOctaves();
				tex->fbm.omega = ft->GetOmega();
				break;
			}
			case MARBLE: {
				const MarbleTexture *mt = static_cast<const MarbleTexture *>(t);

				tex->type = slg::ocl::MARBLE;
				CompileTextureMapping3D(&tex->marble.mapping, mt->GetTextureMapping());
				tex->marble.octaves = mt->GetOctaves();
				tex->marble.omega = mt->GetOmega();
				tex->marble.scale = mt->GetScale();
				tex->marble.variation = mt->GetVariation();
				break;
			}
			case DOTS: {
				const DotsTexture *dt = static_cast<const DotsTexture *>(t);

				tex->type = slg::ocl::DOTS;
				CompileTextureMapping2D(&tex->dots.mapping, dt->GetTextureMapping());
				const Texture *insideTex = dt->GetInsideTex();
				tex->dots.insideIndex = scene->texDefs.GetTextureIndex(insideTex);

				const Texture *outsideTex = dt->GetOutsideTex();
				tex->dots.outsideIndex = scene->texDefs.GetTextureIndex(outsideTex);
				break;
			}
			case BRICK: {
				const BrickTexture *bt = static_cast<const BrickTexture *>(t);

				tex->type = slg::ocl::BRICK;
				CompileTextureMapping3D(&tex->brick.mapping, bt->GetTextureMapping());
				const Texture *tex1 = bt->GetTexture1();
				tex->brick.tex1Index = scene->texDefs.GetTextureIndex(tex1);
				const Texture *tex2 = bt->GetTexture2();
				tex->brick.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				const Texture *tex3 = bt->GetTexture3();
				tex->brick.tex3Index = scene->texDefs.GetTextureIndex(tex3);

				switch (bt->GetBond()) {
					case FLEMISH:
						tex->brick.bond = slg::ocl::FLEMISH;
						break;
					default:
					case RUNNING:
						tex->brick.bond = slg::ocl::RUNNING;
						break;
					case ENGLISH:
						tex->brick.bond = slg::ocl::ENGLISH;
						break;
					case HERRINGBONE:
						tex->brick.bond = slg::ocl::HERRINGBONE;
						break;
					case BASKET:
						tex->brick.bond = slg::ocl::BASKET;
						break;
					case KETTING:
						tex->brick.bond = slg::ocl::KETTING;
						break;
				}

				tex->brick.offsetx = bt->GetOffset().x;
				tex->brick.offsety = bt->GetOffset().y;
				tex->brick.offsetz = bt->GetOffset().z;
				tex->brick.brickwidth = bt->GetBrickWidth();
				tex->brick.brickheight = bt->GetBrickHeight();
				tex->brick.brickdepth = bt->GetBrickDepth();
				tex->brick.mortarsize = bt->GetMortarSize();
				tex->brick.proportion = bt->GetProportion();
				tex->brick.invproportion = bt->GetInvProportion();
				tex->brick.run = bt->GetRun();
				tex->brick.mortarwidth = bt->GetMortarWidth();
				tex->brick.mortarheight = bt->GetMortarHeight();
				tex->brick.mortardepth = bt->GetMortarDepth();
				tex->brick.modulationBias = bt->GetModulationBias();
				break;
			}
			case ADD_TEX: {
				const AddTexture *st = static_cast<const AddTexture *>(t);

				tex->type = slg::ocl::ADD_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->addTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->addTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case SUBTRACT_TEX: {
				const SubtractTexture *st = static_cast<const SubtractTexture *>(t);

				tex->type = slg::ocl::SUBTRACT_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->subtractTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->subtractTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
            case ROUNDING_TEX: {
                const RoundingTexture *rt = static_cast<const RoundingTexture *>(t);

                tex->type = slg::ocl::ROUNDING_TEX;
                const Texture *texture = rt->GetTexture();
                tex->roundingTex.textureIndex = scene->texDefs.GetTextureIndex(texture);

                const Texture *increment = rt->GetIncrement();
                tex->roundingTex.incrementIndex = scene->texDefs.GetTextureIndex(increment);
                break;
            }
            case MODULO_TEX: {
                const ModuloTexture *mt = static_cast<const ModuloTexture *>(t);

                tex->type = slg::ocl::MODULO_TEX;
                const Texture *texture = mt->GetTexture();
                tex->moduloTex.textureIndex = scene->texDefs.GetTextureIndex(texture);

                const Texture *modulo = mt->GetModulo();
                tex->moduloTex.moduloIndex = scene->texDefs.GetTextureIndex(modulo);
                break;
            }

			case WINDY: {
				const WindyTexture *wt = static_cast<const WindyTexture *>(t);

				tex->type = slg::ocl::WINDY;
				CompileTextureMapping3D(&tex->windy.mapping, wt->GetTextureMapping());
				break;
			}
			case WRINKLED: {
				const WrinkledTexture *wt = static_cast<const WrinkledTexture *>(t);

				tex->type = slg::ocl::WRINKLED;
				CompileTextureMapping3D(&tex->wrinkled.mapping, wt->GetTextureMapping());
				tex->wrinkled.octaves = wt->GetOctaves();
				tex->wrinkled.omega = wt->GetOmega();
				break;
			}
			case BLENDER_BLEND: {
				const BlenderBlendTexture *wt = static_cast<const BlenderBlendTexture *>(t);
				tex->type = slg::ocl::BLENDER_BLEND;
				CompileTextureMapping3D(&tex->blenderBlend.mapping, wt->GetTextureMapping());
				tex->blenderBlend.direction = wt->GetDirection();
				tex->blenderBlend.bright = wt->GetBright();
				tex->blenderBlend.contrast = wt->GetContrast();

				switch (wt->GetProgressionType()) {
					default:
					case TEX_LIN:
						tex->blenderBlend.type = slg::ocl::TEX_LIN;
						break;
					case TEX_QUAD:
						tex->blenderBlend.type = slg::ocl::TEX_QUAD;
						break;
					case TEX_EASE:
						tex->blenderBlend.type = slg::ocl::TEX_EASE;
						break;
					case TEX_DIAG:
						tex->blenderBlend.type = slg::ocl::TEX_DIAG;
						break;
					case TEX_SPHERE:
						tex->blenderBlend.type = slg::ocl::TEX_SPHERE;
						break;
					case TEX_HALO:
						tex->blenderBlend.type = slg::ocl::TEX_HALO;
						break;
					case TEX_RAD:
						tex->blenderBlend.type = slg::ocl::TEX_RAD;
						break;
				}
				break;
			}
			case BLENDER_CLOUDS: {
				const BlenderCloudsTexture *wt = static_cast<const BlenderCloudsTexture *>(t);
				tex->type = slg::ocl::BLENDER_CLOUDS;
				CompileTextureMapping3D(&tex->blenderClouds.mapping, wt->GetTextureMapping());
				tex->blenderClouds.noisesize = wt->GetNoiseSize();
				tex->blenderClouds.noisedepth = wt->GetNoiseDepth();
				tex->blenderClouds.bright = wt->GetBright();
				tex->blenderClouds.contrast = wt->GetContrast();
				tex->blenderClouds.hard = wt->GetNoiseType();
				switch (wt->GetNoiseBasis()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderClouds.noisebasis = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderClouds.noisebasis = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderClouds.noisebasis = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderClouds.noisebasis = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderClouds.noisebasis = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderClouds.noisebasis = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderClouds.noisebasis = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderClouds.noisebasis = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderClouds.noisebasis = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderClouds.noisebasis = slg::ocl::CELL_NOISE;
						break;
				}
				break;
			}
			case BLENDER_DISTORTED_NOISE: {
				const BlenderDistortedNoiseTexture *wt = static_cast<const BlenderDistortedNoiseTexture *>(t);
				tex->type = slg::ocl::BLENDER_DISTORTED_NOISE;
				CompileTextureMapping3D(&tex->blenderDistortedNoise.mapping, wt->GetTextureMapping());
				tex->blenderDistortedNoise.distortion = wt->GetDistortion();
				tex->blenderDistortedNoise.noisesize = wt->GetNoiseSize();
				tex->blenderDistortedNoise.bright = wt->GetBright();
				tex->blenderDistortedNoise.contrast = wt->GetContrast();

				switch (wt->GetNoiseDistortion()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderDistortedNoise.noisedistortion = slg::ocl::CELL_NOISE;
						break;
				}

				switch (wt->GetNoiseBasis()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderDistortedNoise.noisebasis = slg::ocl::CELL_NOISE;
						break;
				}
				break;
			}
			case BLENDER_MAGIC: {
				const BlenderMagicTexture *wt = static_cast<const BlenderMagicTexture *>(t);
				tex->type = slg::ocl::BLENDER_MAGIC;
				CompileTextureMapping3D(&tex->blenderMagic.mapping, wt->GetTextureMapping());
				tex->blenderMagic.noisedepth = wt->GetNoiseDepth();
				tex->blenderMagic.turbulence = wt->GetTurbulence();
				tex->blenderMagic.bright = wt->GetBright();
				tex->blenderMagic.contrast = wt->GetContrast();
				break;
			}
			case BLENDER_MARBLE: {
				const BlenderMarbleTexture *wt = static_cast<const BlenderMarbleTexture *>(t);
				tex->type = slg::ocl::BLENDER_MARBLE;
				CompileTextureMapping3D(&tex->blenderMarble.mapping, wt->GetTextureMapping());
				tex->blenderMarble.turbulence = wt->GetTurbulence();
				tex->blenderMarble.noisesize = wt->GetNoiseSize();
				tex->blenderMarble.noisedepth = wt->GetNoiseDepth();
				tex->blenderMarble.bright = wt->GetBright();
				tex->blenderMarble.contrast = wt->GetContrast();
				tex->blenderMarble.hard = wt->GetNoiseType();

				switch (wt->GetNoiseBasis2()) {
					default:
					case TEX_SIN:
						tex->blenderMarble.noisebasis2 = slg::ocl::TEX_SIN;
						break;
					case TEX_SAW:
						tex->blenderMarble.noisebasis2 = slg::ocl::TEX_SAW;
						break;
					case TEX_TRI:
						tex->blenderMarble.noisebasis2 = slg::ocl::TEX_TRI;
						break;
				}

				switch (wt->GetMarbleType()) {
					default:
					case TEX_SOFT:
						tex->blenderMarble.type = slg::ocl::TEX_SOFT;
						break;
					case TEX_SHARP:
						tex->blenderMarble.type = slg::ocl::TEX_SHARP;
						break;
					case TEX_SHARPER:
						tex->blenderMarble.type = slg::ocl::TEX_SHARPER;
						break;
				}

				switch (wt->GetNoiseBasis()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderMarble.noisebasis = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderMarble.noisebasis = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderMarble.noisebasis = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderMarble.noisebasis = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderMarble.noisebasis = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderMarble.noisebasis = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderMarble.noisebasis = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderMarble.noisebasis = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderMarble.noisebasis = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderMarble.noisebasis = slg::ocl::CELL_NOISE;
						break;
				}
				break;
			}
			case BLENDER_MUSGRAVE: {
				const BlenderMusgraveTexture *wt = static_cast<const BlenderMusgraveTexture *>(t);
				tex->type = slg::ocl::BLENDER_MUSGRAVE;
				CompileTextureMapping3D(&tex->blenderMusgrave.mapping, wt->GetTextureMapping());
				tex->blenderMusgrave.dimension = wt->GetDimension();
				tex->blenderMusgrave.intensity = wt->GetIntensity();
				tex->blenderMusgrave.lacunarity = wt->GetLacunarity();
				tex->blenderMusgrave.offset = wt->GetOffset();
				tex->blenderMusgrave.gain = wt->GetGain();
				tex->blenderMusgrave.octaves = wt->GetOctaves();
				tex->blenderMusgrave.noisesize = wt->GetNoiseSize();
				tex->blenderMusgrave.bright = wt->GetBright();
				tex->blenderMusgrave.contrast = wt->GetContrast();

				switch (wt->GetMusgraveType()) {
					default:
					case TEX_MULTIFRACTAL:
						tex->blenderMusgrave.type = slg::ocl::TEX_MULTIFRACTAL;
						break;
					case TEX_RIDGED_MULTIFRACTAL:
						tex->blenderMusgrave.type = slg::ocl::TEX_RIDGED_MULTIFRACTAL;
						break;
					case TEX_HYBRID_MULTIFRACTAL:
						tex->blenderMusgrave.type = slg::ocl::TEX_HYBRID_MULTIFRACTAL;
						break;
					case TEX_FBM:
						tex->blenderMusgrave.type = slg::ocl::TEX_FBM;
						break;
					case TEX_HETERO_TERRAIN:
						tex->blenderMusgrave.type = slg::ocl::TEX_HETERO_TERRAIN;
						break;
				}

				switch (wt->GetNoiseBasis()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderMusgrave.noisebasis = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderMusgrave.noisebasis = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderMusgrave.noisebasis = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderMusgrave.noisebasis = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderMusgrave.noisebasis = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderMusgrave.noisebasis = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderMusgrave.noisebasis = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderMusgrave.noisebasis = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderMusgrave.noisebasis = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderMusgrave.noisebasis = slg::ocl::CELL_NOISE;
						break;
				}
				break;
			}
			case BLENDER_NOISE: {
				const BlenderNoiseTexture *nt = static_cast<const BlenderNoiseTexture *>(t);
				tex->type = slg::ocl::BLENDER_NOISE;
				tex->blenderNoise.noisedepth = nt->GetNoiseDepth();
				tex->blenderNoise.bright = nt->GetBright();
				tex->blenderNoise.contrast = nt->GetContrast();
				break;
			}
			case BLENDER_STUCCI: {
				const BlenderStucciTexture *wt = static_cast<const BlenderStucciTexture *>(t);
				tex->type = slg::ocl::BLENDER_STUCCI;
				CompileTextureMapping3D(&tex->blenderStucci.mapping, wt->GetTextureMapping());
				tex->blenderStucci.noisesize = wt->GetNoiseSize();
				tex->blenderStucci.turbulence = wt->GetTurbulence();
				tex->blenderStucci.bright = wt->GetBright();
				tex->blenderStucci.contrast = wt->GetContrast();
				tex->blenderStucci.hard = wt->GetNoiseType();

				switch (wt->GetStucciType()) {
					default:
					case TEX_PLASTIC:
						tex->blenderStucci.type = slg::ocl::TEX_PLASTIC;
						break;
					case TEX_WALL_IN:
						tex->blenderStucci.type = slg::ocl::TEX_WALL_IN;
						break;
					case TEX_WALL_OUT:
						tex->blenderStucci.type = slg::ocl::TEX_WALL_OUT;
						break;
				}

				switch (wt->GetNoiseBasis()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderStucci.noisebasis = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderStucci.noisebasis = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderStucci.noisebasis = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderStucci.noisebasis = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderStucci.noisebasis = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderStucci.noisebasis = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderStucci.noisebasis = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderStucci.noisebasis = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderStucci.noisebasis = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderStucci.noisebasis = slg::ocl::CELL_NOISE;
						break;
				}

				break;
			}
			case BLENDER_WOOD: {
				const BlenderWoodTexture *wt = static_cast<const BlenderWoodTexture *>(t);

				tex->type = slg::ocl::BLENDER_WOOD;
				CompileTextureMapping3D(&tex->blenderWood.mapping, wt->GetTextureMapping());
				tex->blenderWood.turbulence = wt->GetTurbulence();
				tex->blenderWood.bright = wt->GetBright();
				tex->blenderWood.contrast = wt->GetContrast();
				tex->blenderWood.hard = wt->GetNoiseType();
				tex->blenderWood.noisesize = wt->GetNoiseSize();
				switch (wt->GetNoiseBasis2()) {
					default:
					case TEX_SIN:
						tex->blenderWood.noisebasis2 = slg::ocl::TEX_SIN;
						break;
					case TEX_SAW:
						tex->blenderWood.noisebasis2 = slg::ocl::TEX_SAW;
						break;
					case TEX_TRI:
						tex->blenderWood.noisebasis2 = slg::ocl::TEX_TRI;
						break;
				}

				switch (wt->GetWoodType()) {
					default:
					case BANDS:
						tex->blenderWood.type = slg::ocl::BANDS;
						break;
					case RINGS:
						tex->blenderWood.type = slg::ocl::RINGS;
						break;
					case BANDNOISE:
						tex->blenderWood.type = slg::ocl::BANDNOISE;
						break;
					case RINGNOISE:
						tex->blenderWood.type = slg::ocl::RINGNOISE;
						break;
				}
				switch (wt->GetNoiseBasis()) {
					default:
					case BLENDER_ORIGINAL:
						tex->blenderWood.noisebasis = slg::ocl::BLENDER_ORIGINAL;
						break;
					case ORIGINAL_PERLIN:
						tex->blenderWood.noisebasis = slg::ocl::ORIGINAL_PERLIN;
						break;
					case IMPROVED_PERLIN:
						tex->blenderWood.noisebasis = slg::ocl::IMPROVED_PERLIN;
						break;
					case VORONOI_F1:
						tex->blenderWood.noisebasis = slg::ocl::VORONOI_F1;
						break;
					case VORONOI_F2:
						tex->blenderWood.noisebasis = slg::ocl::VORONOI_F2;
						break;
					case VORONOI_F3:
						tex->blenderWood.noisebasis = slg::ocl::VORONOI_F3;
						break;
					case VORONOI_F4:
						tex->blenderWood.noisebasis = slg::ocl::VORONOI_F4;
						break;
					case VORONOI_F2_F1:
						tex->blenderWood.noisebasis = slg::ocl::VORONOI_F2_F1;
						break;
					case VORONOI_CRACKLE:
						tex->blenderWood.noisebasis = slg::ocl::VORONOI_CRACKLE;
						break;
					case CELL_NOISE:
						tex->blenderWood.noisebasis = slg::ocl::CELL_NOISE;
						break;
				}
				break;
			}
			case BLENDER_VORONOI: {
				const BlenderVoronoiTexture *wt = static_cast<const BlenderVoronoiTexture *>(t);

				tex->type = slg::ocl::BLENDER_VORONOI;
				CompileTextureMapping3D(&tex->blenderVoronoi.mapping, wt->GetTextureMapping());
				tex->blenderVoronoi.feature_weight1 = wt->GetFeatureWeight1();
				tex->blenderVoronoi.feature_weight2 = wt->GetFeatureWeight2();
				tex->blenderVoronoi.feature_weight3 = wt->GetFeatureWeight3();
				tex->blenderVoronoi.feature_weight4 = wt->GetFeatureWeight4();
				tex->blenderVoronoi.noisesize = wt->GetNoiseSize();
				tex->blenderVoronoi.intensity = wt->GetIntensity();
				tex->blenderVoronoi.exponent = wt->GetExponent();
				tex->blenderVoronoi.bright = wt->GetBright();
				tex->blenderVoronoi.contrast = wt->GetContrast();

				switch (wt->GetDistMetric()) {
					default:
					case ACTUAL_DISTANCE:
						tex->blenderVoronoi.distancemetric = slg::ocl::ACTUAL_DISTANCE;
						break;
					case DISTANCE_SQUARED:
						tex->blenderVoronoi.distancemetric = slg::ocl::DISTANCE_SQUARED;
						break;
					case MANHATTAN:
						tex->blenderVoronoi.distancemetric = slg::ocl::MANHATTAN;
						break;
					case CHEBYCHEV:
						tex->blenderVoronoi.distancemetric = slg::ocl::CHEBYCHEV;
						break;
					case MINKOWSKI_HALF:
						tex->blenderVoronoi.distancemetric = slg::ocl::MINKOWSKI_HALF;
						break;
					case MINKOWSKI_FOUR:
						tex->blenderVoronoi.distancemetric = slg::ocl::MINKOWSKI_FOUR;
						break;
					case MINKOWSKI:
						tex->blenderVoronoi.distancemetric = slg::ocl::MANHATTAN;
						break;
				}
				break;
			}
            case UV_TEX: {
				const UVTexture *uvt = static_cast<const UVTexture *>(t);

				tex->type = slg::ocl::UV_TEX;
				CompileTextureMapping2D(&tex->uvTex.mapping, uvt->GetTextureMapping());
				break;
			}
			case BAND_TEX: {
				const BandTexture *bt = static_cast<const BandTexture *>(t);

				tex->type = slg::ocl::BAND_TEX;

				switch (bt->GetInterpolationType()) {
					case BandTexture::NONE:
						tex->band.interpType = slg::ocl::INTERP_NONE;
						break;
					default:
					case BandTexture::LINEAR:
						tex->band.interpType = slg::ocl::INTERP_LINEAR;
						break;
					case BandTexture::CUBIC:
						tex->band.interpType = slg::ocl::INTERP_CUBIC;
						break;
				}

				const Texture *amount = bt->GetAmountTexture();
				tex->band.amountTexIndex = scene->texDefs.GetTextureIndex(amount);

				const vector<float> &offsets = bt->GetOffsets();
				const vector<Spectrum> &values = bt->GetValues();
				if (offsets.size() > BAND_TEX_MAX_SIZE)
					throw runtime_error("BandTexture with more than " + ToString(BAND_TEX_MAX_SIZE) + " are not supported");
				tex->band.size = offsets.size();
				for (u_int i = 0; i < BAND_TEX_MAX_SIZE; ++i) {
					if (i < offsets.size()) {
						tex->band.offsets[i] = offsets[i];
						ASSIGN_SPECTRUM(tex->band.values[i], values[i]);
					} else {
						tex->band.offsets[i] = 1.f;
						tex->band.values[i].c[0] = 0.f;
						tex->band.values[i].c[1] = 0.f;
						tex->band.values[i].c[2] = 0.f;
					}
				}
				break;
			}
			case HITPOINTCOLOR: {
				const HitPointColorTexture *hpc = static_cast<const HitPointColorTexture *>(t);

				tex->type = slg::ocl::HITPOINTCOLOR;
				tex->hitPointColor.dataIndex = hpc->GetDataIndex();
				break;
			}
			case HITPOINTALPHA: {
				const HitPointAlphaTexture *hpa = static_cast<const HitPointAlphaTexture *>(t);

				tex->type = slg::ocl::HITPOINTALPHA;
				tex->hitPointAlpha.dataIndex = hpa->GetDataIndex();
				break;
			}
			case HITPOINTGREY: {
				const HitPointGreyTexture *hpg = static_cast<const HitPointGreyTexture *>(t);

				tex->type = slg::ocl::HITPOINTGREY;
				tex->hitPointGrey.dataIndex = hpg->GetDataIndex();
				tex->hitPointGrey.channelIndex = hpg->GetChannel();
				break;
			}
			case HITPOINTVERTEXAOV: {
				const HitPointVertexAOVTexture *hpv = static_cast<const HitPointVertexAOVTexture *>(t);

				tex->type = slg::ocl::HITPOINTVERTEXAOV;
				tex->hitPointVertexAOV.dataIndex = hpv->GetDataIndex();
				break;
			}
			case HITPOINTTRIANGLEAOV: {
				const HitPointTriangleAOVTexture *hpt = static_cast<const HitPointTriangleAOVTexture *>(t);

				tex->type = slg::ocl::HITPOINTTRIANGLEAOV;
				tex->hitPointTriangleAOV.dataIndex = hpt->GetDataIndex();
				break;
			}
            case NORMALMAP_TEX: {
                const NormalMapTexture *nmt = static_cast<const NormalMapTexture *>(t);

                tex->type = slg::ocl::NORMALMAP_TEX;
                const Texture *normalTex = nmt->GetTexture();
				tex->normalMap.texIndex = scene->texDefs.GetTextureIndex(normalTex);
				tex->normalMap.scale = nmt->GetScale();
				break;
            }
			case BLACKBODY_TEX: {
				const BlackBodyTexture *bbt = static_cast<const BlackBodyTexture *>(t);

				tex->type = slg::ocl::BLACKBODY_TEX;
				ASSIGN_SPECTRUM(tex->blackBody.rgb, bbt->GetRGB());
				break;
			}
			case IRREGULARDATA_TEX: {
				const IrregularDataTexture *idt = static_cast<const IrregularDataTexture *>(t);

				tex->type = slg::ocl::IRREGULARDATA_TEX;
				ASSIGN_SPECTRUM(tex->irregularData.rgb, idt->GetRGB());
				break;
			}
			case DENSITYGRID_TEX: {
				const DensityGridTexture *dgt = static_cast<const DensityGridTexture *>(t);

				tex->type = slg::ocl::DENSITYGRID_TEX;
				CompileTextureMapping3D(&tex->densityGrid.mapping, dgt->GetTextureMapping());

				tex->densityGrid.nx = dgt->GetWidth();
				tex->densityGrid.ny = dgt->GetHeight();
				tex->densityGrid.nz = dgt->GetDepth();
				tex->densityGrid.imageMapIndex = scene->imgMapCache.GetImageMapIndex(dgt->GetImageMap());
				break;
			}
			case FRESNELCOLOR_TEX: {
				const FresnelColorTexture *fct = static_cast<const FresnelColorTexture *>(t);

				tex->type = slg::ocl::FRESNELCOLOR_TEX;
				const Texture *krTex = fct->GetKr();
				tex->fresnelColor.krIndex = scene->texDefs.GetTextureIndex(krTex);
				break;
			}
			case FRESNELCONST_TEX: {
				const FresnelConstTexture *fct = static_cast<const FresnelConstTexture *>(t);

				tex->type = slg::ocl::FRESNELCONST_TEX;
				ASSIGN_SPECTRUM(tex->fresnelConst.n, fct->GetN());
				ASSIGN_SPECTRUM(tex->fresnelConst.k, fct->GetK());
				break;
			}
			case ABS_TEX: {
				const AbsTexture *at = static_cast<const AbsTexture *>(t);

				tex->type = slg::ocl::ABS_TEX;
				const Texture *refTex = at->GetTexture();
				tex->absTex.texIndex = scene->texDefs.GetTextureIndex(refTex);
				break;
			}
			case CLAMP_TEX: {
				const ClampTexture *ct = static_cast<const ClampTexture *>(t);

				tex->type = slg::ocl::CLAMP_TEX;
				const Texture *refTex = ct->GetTexture();
				tex->clampTex.texIndex = scene->texDefs.GetTextureIndex(refTex);
				tex->clampTex.minVal = ct->GetMinVal();
				tex->clampTex.maxVal = ct->GetMaxVal();
				break;
			}
			case BILERP_TEX: {
				const BilerpTexture *bt = static_cast<const BilerpTexture *>(t);
				tex->type = slg::ocl::BILERP_TEX;
				const Texture *t00 = bt->GetTexture00();
				const Texture *t01 = bt->GetTexture01();
				const Texture *t10 = bt->GetTexture10();
				const Texture *t11 = bt->GetTexture11();
				tex->bilerpTex.t00Index = scene->texDefs.GetTextureIndex(t00);
				tex->bilerpTex.t01Index = scene->texDefs.GetTextureIndex(t01);
				tex->bilerpTex.t10Index = scene->texDefs.GetTextureIndex(t10);
				tex->bilerpTex.t11Index = scene->texDefs.GetTextureIndex(t11);
				break;
			}
			case COLORDEPTH_TEX: {
				const ColorDepthTexture *ct = static_cast<const ColorDepthTexture *>(t);

				tex->type = slg::ocl::COLORDEPTH_TEX;
				const Texture *ktTex = ct->GetKt();
				tex->colorDepthTex.ktIndex = scene->texDefs.GetTextureIndex(ktTex);
				tex->colorDepthTex.dVal = ct->GetD();
				break;
			}
			case HSV_TEX: {
				const HsvTexture *ht = static_cast<const HsvTexture *>(t);

				tex->type = slg::ocl::HSV_TEX;
				tex->hsvTex.texIndex = scene->texDefs.GetTextureIndex(ht->GetTexture());
				tex->hsvTex.hueTexIndex = scene->texDefs.GetTextureIndex(ht->GetHue());
				tex->hsvTex.satTexIndex = scene->texDefs.GetTextureIndex(ht->GetSaturation());
				tex->hsvTex.valTexIndex = scene->texDefs.GetTextureIndex(ht->GetValue());
				break;
			}
			case DIVIDE_TEX: {
				const DivideTexture *dt = static_cast<const DivideTexture *>(t);

				tex->type = slg::ocl::DIVIDE_TEX;
				const Texture *tex1 = dt->GetTexture1();
				tex->divideTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = dt->GetTexture2();
				tex->divideTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case REMAP_TEX: {
				const RemapTexture *rt = static_cast<const RemapTexture *>(t);

				tex->type = slg::ocl::REMAP_TEX;
				const Texture *valueTex = rt->GetValueTex();
				tex->remapTex.valueTexIndex = scene->texDefs.GetTextureIndex(valueTex);
				const Texture *sourceMinTex = rt->GetSourceMinTex();
				tex->remapTex.sourceMinTexIndex = scene->texDefs.GetTextureIndex(sourceMinTex);
				const Texture *sourceMaxTex = rt->GetSourceMaxTex();
				tex->remapTex.sourceMaxTexIndex = scene->texDefs.GetTextureIndex(sourceMaxTex);
				const Texture *targetMinTex = rt->GetTargetMinTex();
				tex->remapTex.targetMinTexIndex = scene->texDefs.GetTextureIndex(targetMinTex);
				const Texture *targetMaxTex = rt->GetTargetMaxTex();
				tex->remapTex.targetMaxTexIndex = scene->texDefs.GetTextureIndex(targetMaxTex);
				break;
			}
			case OBJECTID_TEX: {
				tex->type = slg::ocl::OBJECTID_TEX;
				break;
			}
			case OBJECTID_COLOR_TEX: {
				tex->type = slg::ocl::OBJECTID_COLOR_TEX;
				break;
			}
			case OBJECTID_NORMALIZED_TEX: {
				tex->type = slg::ocl::OBJECTID_NORMALIZED_TEX;
				break;
			}
			case DOT_PRODUCT_TEX: {
				const DotProductTexture *dpt = static_cast<const DotProductTexture *>(t);

				tex->type = slg::ocl::DOT_PRODUCT_TEX;
				const Texture *tex1 = dpt->GetTexture1();
				tex->dotProductTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = dpt->GetTexture2();
				tex->dotProductTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case GREATER_THAN_TEX: {
				const GreaterThanTexture *gtt = static_cast<const GreaterThanTexture *>(t);

				tex->type = slg::ocl::GREATER_THAN_TEX;
				const Texture *tex1 = gtt->GetTexture1();
				tex->greaterThanTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = gtt->GetTexture2();
				tex->greaterThanTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case LESS_THAN_TEX: {
				const LessThanTexture *ltt = static_cast<const LessThanTexture *>(t);

				tex->type = slg::ocl::LESS_THAN_TEX;
				const Texture *tex1 = ltt->GetTexture1();
				tex->lessThanTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = ltt->GetTexture2();
				tex->lessThanTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case POWER_TEX: {
				const PowerTexture *pt = static_cast<const PowerTexture *>(t);

				tex->type = slg::ocl::POWER_TEX;
				const Texture *base = pt->GetBase();
				tex->powerTex.baseTexIndex = scene->texDefs.GetTextureIndex(base);

				const Texture *exponent = pt->GetExponent();
				tex->powerTex.exponentTexIndex = scene->texDefs.GetTextureIndex(exponent);
				break;
			}
			case SHADING_NORMAL_TEX: {
				tex->type = slg::ocl::SHADING_NORMAL_TEX;
				break;
			}
			case POSITION_TEX: {
				tex->type = slg::ocl::POSITION_TEX;
				break;
			}
			case SPLIT_FLOAT3: {
				const SplitFloat3Texture *sf3t = static_cast<const SplitFloat3Texture *>(t);

				tex->type = slg::ocl::SPLIT_FLOAT3;
				const Texture *t = sf3t->GetTexture();
				tex->splitFloat3Tex.texIndex = scene->texDefs.GetTextureIndex(t);

				tex->splitFloat3Tex.channelIndex = sf3t->GetChannel();
				break;
			}
			case MAKE_FLOAT3: {
				const MakeFloat3Texture *mf3t = static_cast<const MakeFloat3Texture *>(t);

				tex->type = slg::ocl::MAKE_FLOAT3;
				const Texture *t1 = mf3t->GetTexture1();
				tex->makeFloat3Tex.tex1Index = scene->texDefs.GetTextureIndex(t1);
				const Texture *t2 = mf3t->GetTexture2();
				tex->makeFloat3Tex.tex2Index = scene->texDefs.GetTextureIndex(t2);
				const Texture *t3 = mf3t->GetTexture3();
				tex->makeFloat3Tex.tex3Index = scene->texDefs.GetTextureIndex(t3);
				break;
			}
			case BRIGHT_CONTRAST_TEX: {
				const BrightContrastTexture *bct = static_cast<const BrightContrastTexture *>(t);

				tex->type = slg::ocl::BRIGHT_CONTRAST_TEX;
				const Texture *t = bct->GetTex();
				tex->brightContrastTex.texIndex = scene->texDefs.GetTextureIndex(t);
				const Texture *b = bct->GetBrightnessTex();
				tex->brightContrastTex.brightnessTexIndex = scene->texDefs.GetTextureIndex(b);
				const Texture *c = bct->GetContrastTex();
				tex->brightContrastTex.contrastTexIndex = scene->texDefs.GetTextureIndex(c);
				break;
			}
			case TRIPLANAR_TEX: {
				const TriplanarTexture *trit = static_cast<const TriplanarTexture *>(t);

				tex->type = slg::ocl::TRIPLANAR_TEX;
				CompileTextureMapping3D(&tex->triplanarTex.mapping, trit->GetTextureMapping());
				const Texture *t1 = trit->GetTexture1();
				tex->triplanarTex.tex1Index = scene->texDefs.GetTextureIndex(t1);
				const Texture *t2 = trit->GetTexture2();
				tex->triplanarTex.tex2Index = scene->texDefs.GetTextureIndex(t2);
				const Texture *t3 = trit->GetTexture3();
				tex->triplanarTex.tex3Index = scene->texDefs.GetTextureIndex(t3);
				tex->triplanarTex.enableUVlessBumpMap = trit->IsUVlessBumpMap();
				break;
			}
			case RANDOM_TEX: {
				const RandomTexture *rt = static_cast<const RandomTexture *>(t);

				tex->type = slg::ocl::RANDOM_TEX;
				const Texture *t1 = rt->GetTexture();
				tex->randomTex.texIndex = scene->texDefs.GetTextureIndex(t1);
				break;
			}
			case WIREFRAME_TEX: {
				const WireFrameTexture *wft = static_cast<const WireFrameTexture *>(t);

				tex->type = slg::ocl::WIREFRAME_TEX;
				tex->wireFrameTex.width = wft->GetWidth();
				const Texture *borderTex = wft->GetBorderTex();
				tex->wireFrameTex.borderTexIndex = scene->texDefs.GetTextureIndex(borderTex);

				const Texture *insideTex = wft->GetInsideTex();
				tex->wireFrameTex.insideTexIndex = scene->texDefs.GetTextureIndex(insideTex);
				break;
			}
			case DISTORT_TEX: {
				const DistortTexture *dt = static_cast<const DistortTexture *>(t);

				tex->type = slg::ocl::DISTORT_TEX;
				tex->distortTex.strength = dt->GetStrength();
				const Texture *texture = dt->GetTex();
				tex->distortTex.texIndex = scene->texDefs.GetTextureIndex(texture);

				const Texture *offsetTex = dt->GetOffset();
				tex->distortTex.offsetTexIndex = scene->texDefs.GetTextureIndex(offsetTex);
				break;
			}
			default:
				throw runtime_error("Unknown texture in CompiledScene::CompileTextures(): " + ToString(t->GetType()));
		}
	}

	CompileTextureOps();

	const double tEnd = WallClockTime();
	SLG_LOG("Textures compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

#endif

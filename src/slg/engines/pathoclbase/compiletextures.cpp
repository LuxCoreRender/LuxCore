/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

#include "slg/textures/abs.h"
#include "slg/textures/add.h"
#include "slg/textures/band.h"
#include "slg/textures/bilerp.h"
#include "slg/textures/blackbody.h"
#include "slg/textures/blender_texture.h"
#include "slg/textures/brick.h"
#include "slg/textures/checkerboard.h"
#include "slg/textures/clamp.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"
#include "slg/textures/cloud.h"
#include "slg/textures/dots.h"
#include "slg/textures/fbm.h"
#include "slg/textures/fresnelapprox.h"
#include "slg/textures/fresnel/fresnelcolor.h"
#include "slg/textures/fresnel/fresnelconst.h"
#include "slg/textures/fresnel/fresnelluxpop.h"
#include "slg/textures/fresnel/fresnelpreset.h"
#include "slg/textures/fresnel/fresnelsopra.h"
#include "slg/textures/fresnel/fresneltexture.h"
#include "slg/textures/hitpoint.h"
#include "slg/textures/imagemaptex.h"
#include "slg/textures/irregulardata.h"
#include "slg/textures/lampspectrum.h"
#include "slg/textures/marble.h"
#include "slg/textures/mix.h"
#include "slg/textures/normalmap.h"
#include "slg/textures/scale.h"
#include "slg/textures/subtract.h"
#include "slg/textures/windy.h"
#include "slg/textures/wrinkled.h"
#include "slg/textures/uv.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace slg::blender;

void CompiledScene::CompileTextureMapping2D(slg::ocl::TextureMapping2D *mapping, const TextureMapping2D *m) {
	switch (m->GetType()) {
		case UVMAPPING2D: {
			mapping->type = slg::ocl::UVMAPPING2D;
			const UVMapping2D *uvm = static_cast<const UVMapping2D *>(m);
			mapping->uvMapping2D.uScale = uvm->uScale;
			mapping->uvMapping2D.vScale = uvm->vScale;
			mapping->uvMapping2D.uDelta = uvm->uDelta;
			mapping->uvMapping2D.vDelta = uvm->vDelta;
			break;
		}
		default:
			throw runtime_error("Unknown 2D texture mapping in CompiledScene::CompileTextureMapping2D: " + boost::lexical_cast<string>(m->GetType()));
	}
}

void CompiledScene::CompileTextureMapping3D(slg::ocl::TextureMapping3D *mapping, const TextureMapping3D *m) {
	switch (m->GetType()) {
		case UVMAPPING3D: {
			mapping->type = slg::ocl::UVMAPPING3D;
			const UVMapping3D *uvm = static_cast<const UVMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &uvm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &uvm->worldToLocal.mInv, sizeof(float[4][4]));
			break;
		}
		case GLOBALMAPPING3D: {
			mapping->type = slg::ocl::GLOBALMAPPING3D;
			const GlobalMapping3D *gm = static_cast<const GlobalMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &gm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &gm->worldToLocal.mInv, sizeof(float[4][4]));
			break;
		}
		default:
			throw runtime_error("Unknown 3D texture mapping in CompiledScene::CompileTextureMapping3D: " + boost::lexical_cast<string>(m->GetType()));
	}
}

float *CompiledScene::CompileDistribution1D(const Distribution1D *dist, u_int *size) {
	const u_int count = dist->GetCount();
	*size = sizeof(u_int) + count * sizeof(float) + (count + 1) * sizeof(float);
	float *compDist = new float[*size];

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

	*size = 2 * sizeof(u_int) + marginalSize + condDists.size() * condSize;
	float *compDist = new float[*size];

	*((u_int *)&compDist[0]) = dist->GetWidth();
	*((u_int *)&compDist[1]) = dist->GetHeight();

	float *ptr = &compDist[2];
	copy(marginalDist, marginalDist + marginalSize, ptr);
	ptr += marginalSize / 4;
	delete[] marginalDist;

	const u_int condSize4 = condSize / sizeof(float);
	for (u_int i = 0; i < dist->GetHeight(); ++i) {
		copy(condDists[i], condDists[i] + condSize4, ptr);
		ptr += condSize4;
		delete[] condDists[i];
	}

	return compDist;
}

void CompiledScene::CompileTextures() {
	const u_int texturesCount = scene->texDefs.GetSize();
	SLG_LOG("Compile " << texturesCount << " Textures");
	//SLG_LOG("  Texture size: " << sizeof(slg::ocl::Texture));

	//--------------------------------------------------------------------------
	// Translate textures
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedTextureTypes.clear();
	texs.resize(texturesCount);

	for (u_int i = 0; i < texturesCount; ++i) {
		Texture *t = scene->texDefs.GetTexture(i);
		slg::ocl::Texture *tex = &texs[i];

		usedTextureTypes.insert(t->GetType());
		switch (t->GetType()) {
			case CONST_FLOAT: {
				ConstFloatTexture *cft = static_cast<ConstFloatTexture *>(t);

				tex->type = slg::ocl::CONST_FLOAT;
				tex->constFloat.value = cft->GetValue();
				break;
			}
			case CONST_FLOAT3: {
				ConstFloat3Texture *cft = static_cast<ConstFloat3Texture *>(t);

				tex->type = slg::ocl::CONST_FLOAT3;
				ASSIGN_SPECTRUM(tex->constFloat3.color, cft->GetColor());
				break;
			}
			case IMAGEMAP: {
				ImageMapTexture *imt = static_cast<ImageMapTexture *>(t);

				tex->type = slg::ocl::IMAGEMAP;
				const ImageMap *im = imt->GetImageMap();
				tex->imageMapTex.gain = imt->GetGain();
				CompileTextureMapping2D(&tex->imageMapTex.mapping, imt->GetTextureMapping());
				tex->imageMapTex.imageMapIndex = scene->imgMapCache.GetImageMapIndex(im);
				break;
			}
			case SCALE_TEX: {
				ScaleTexture *st = static_cast<ScaleTexture *>(t);

				tex->type = slg::ocl::SCALE_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->scaleTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->scaleTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case FRESNEL_APPROX_N: {
				FresnelApproxNTexture *ft = static_cast<FresnelApproxNTexture *>(t);

				tex->type = slg::ocl::FRESNEL_APPROX_N;
				const Texture *tx = ft->GetTexture();
				tex->fresnelApproxN.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case FRESNEL_APPROX_K: {
				FresnelApproxKTexture *ft = static_cast<FresnelApproxKTexture *>(t);

				tex->type = slg::ocl::FRESNEL_APPROX_K;
				const Texture *tx = ft->GetTexture();
				tex->fresnelApproxK.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case CHECKERBOARD2D: {
				CheckerBoard2DTexture *cb = static_cast<CheckerBoard2DTexture *>(t);

				tex->type = slg::ocl::CHECKERBOARD2D;
				CompileTextureMapping2D(&tex->checkerBoard2D.mapping, cb->GetTextureMapping());
				const Texture *tex1 = cb->GetTexture1();
				tex->checkerBoard2D.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = cb->GetTexture2();
				tex->checkerBoard2D.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case CHECKERBOARD3D: {
				CheckerBoard3DTexture *cb = static_cast<CheckerBoard3DTexture *>(t);

				tex->type = slg::ocl::CHECKERBOARD3D;
				CompileTextureMapping3D(&tex->checkerBoard3D.mapping, cb->GetTextureMapping());
				const Texture *tex1 = cb->GetTexture1();
				tex->checkerBoard3D.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = cb->GetTexture2();
				tex->checkerBoard3D.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case MIX_TEX: {
				MixTexture *mt = static_cast<MixTexture *>(t);

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
				CloudTexture *ft = static_cast<CloudTexture *>(t);
							
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
				FBMTexture *ft = static_cast<FBMTexture *>(t);

				tex->type = slg::ocl::FBM_TEX;
				CompileTextureMapping3D(&tex->fbm.mapping, ft->GetTextureMapping());
				tex->fbm.octaves = ft->GetOctaves();
				tex->fbm.omega = ft->GetOmega();
				break;
			}
			case MARBLE: {
				MarbleTexture *mt = static_cast<MarbleTexture *>(t);

				tex->type = slg::ocl::MARBLE;
				CompileTextureMapping3D(&tex->marble.mapping, mt->GetTextureMapping());
				tex->marble.octaves = mt->GetOctaves();
				tex->marble.omega = mt->GetOmega();
				tex->marble.scale = mt->GetScale();
				tex->marble.variation = mt->GetVariation();
				break;
			}
			case DOTS: {
				DotsTexture *dt = static_cast<DotsTexture *>(t);

				tex->type = slg::ocl::DOTS;
				CompileTextureMapping2D(&tex->dots.mapping, dt->GetTextureMapping());
				const Texture *insideTex = dt->GetInsideTex();
				tex->dots.insideIndex = scene->texDefs.GetTextureIndex(insideTex);

				const Texture *outsideTex = dt->GetOutsideTex();
				tex->dots.outsideIndex = scene->texDefs.GetTextureIndex(outsideTex);
				break;
			}
			case BRICK: {
				BrickTexture *bt = static_cast<BrickTexture *>(t);

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
				tex->brick.bevelwidth = bt->GetBevelWidth();
				tex->brick.bevelheight = bt->GetBevelHeight();
				tex->brick.beveldepth = bt->GetBevelDepth();
				tex->brick.usebevel = bt->GetUseBevel();
				break;
			}
			case ADD_TEX: {
				AddTexture *st = static_cast<AddTexture *>(t);

				tex->type = slg::ocl::ADD_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->addTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->addTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case SUBTRACT_TEX: {
				SubtractTexture *st = static_cast<SubtractTexture *>(t);
				
				tex->type = slg::ocl::SUBTRACT_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->subtractTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);
				
				const Texture *tex2 = st->GetTexture2();
				tex->subtractTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case WINDY: {
				WindyTexture *wt = static_cast<WindyTexture *>(t);

				tex->type = slg::ocl::WINDY;
				CompileTextureMapping3D(&tex->windy.mapping, wt->GetTextureMapping());
				break;
			}
			case WRINKLED: {
				WrinkledTexture *wt = static_cast<WrinkledTexture *>(t);

				tex->type = slg::ocl::WRINKLED;
				CompileTextureMapping3D(&tex->wrinkled.mapping, wt->GetTextureMapping());
				tex->wrinkled.octaves = wt->GetOctaves();
				tex->wrinkled.omega = wt->GetOmega();
				break;
			}
			case BLENDER_BLEND: {
				BlenderBlendTexture *wt = static_cast<BlenderBlendTexture *>(t);
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
				BlenderCloudsTexture *wt = static_cast<BlenderCloudsTexture *>(t);
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
				BlenderDistortedNoiseTexture *wt = static_cast<BlenderDistortedNoiseTexture *>(t);
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
				BlenderMagicTexture *wt = static_cast<BlenderMagicTexture *>(t);
				tex->type = slg::ocl::BLENDER_MAGIC;
				CompileTextureMapping3D(&tex->blenderMagic.mapping, wt->GetTextureMapping());
				tex->blenderMagic.noisedepth = wt->GetNoiseDepth();
				tex->blenderMagic.turbulence = wt->GetTurbulence();
				tex->blenderMagic.bright = wt->GetBright();
				tex->blenderMagic.contrast = wt->GetContrast();
				break;
			}
			case BLENDER_MARBLE: {
				BlenderMarbleTexture *wt = static_cast<BlenderMarbleTexture *>(t);
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
				BlenderMusgraveTexture *wt = static_cast<BlenderMusgraveTexture *>(t);
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
			case BLENDER_STUCCI: {
				BlenderStucciTexture *wt = static_cast<BlenderStucciTexture *>(t);
				tex->type = slg::ocl::BLENDER_STUCCI;
				CompileTextureMapping3D(&tex->blenderStucci.mapping, wt->GetTextureMapping());
				tex->blenderStucci.noisesize = wt->GetNoiseSize();
				tex->blenderStucci.turbulence = wt->GetTurbulence();
				tex->blenderStucci.bright = wt->GetBright();
				tex->blenderStucci.contrast = wt->GetContrast();
				tex->blenderStucci.hard = wt->GetNoiseType();

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
				BlenderWoodTexture *wt = static_cast<BlenderWoodTexture *>(t);

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
				BlenderVoronoiTexture *wt = static_cast<BlenderVoronoiTexture *>(t);

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
				UVTexture *uvt = static_cast<UVTexture *>(t);

				tex->type = slg::ocl::UV_TEX;
				CompileTextureMapping2D(&tex->uvTex.mapping, uvt->GetTextureMapping());
				break;
			}
			case BAND_TEX: {
				BandTexture *bt = static_cast<BandTexture *>(t);

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
				tex->type = slg::ocl::HITPOINTCOLOR;
				break;
			}
			case HITPOINTALPHA: {
				tex->type = slg::ocl::HITPOINTALPHA;
				break;
			}
			case HITPOINTGREY: {
				HitPointGreyTexture *hpg = static_cast<HitPointGreyTexture *>(t);

				tex->type = slg::ocl::HITPOINTGREY;
				tex->hitPointGrey.channel = hpg->GetChannel();
				break;
			}
            case NORMALMAP_TEX: {
                NormalMapTexture *nmt = static_cast<NormalMapTexture *>(t);

                tex->type = slg::ocl::NORMALMAP_TEX;
                const Texture *normalTex = nmt->GetTexture();
				tex->normalMap.texIndex = scene->texDefs.GetTextureIndex(normalTex);
				break;
            }
			case BLACKBODY_TEX: {
				BlackBodyTexture *bbt = static_cast<BlackBodyTexture *>(t);

				tex->type = slg::ocl::BLACKBODY_TEX;
				ASSIGN_SPECTRUM(tex->blackBody.rgb, bbt->GetRGB());
				break;
			}
			case IRREGULARDATA_TEX: {
				IrregularDataTexture *idt = static_cast<IrregularDataTexture *>(t);

				tex->type = slg::ocl::IRREGULARDATA_TEX;
				ASSIGN_SPECTRUM(tex->irregularData.rgb, idt->GetRGB());
				break;
			}
			case FRESNELCOLOR_TEX: {
				FresnelColorTexture *fct = static_cast<FresnelColorTexture *>(t);

				tex->type = slg::ocl::FRESNELCOLOR_TEX;
				const Texture *krTex = fct->GetKr();
				tex->fresnelColor.krIndex = scene->texDefs.GetTextureIndex(krTex);
				break;
			}
			case FRESNELCONST_TEX: {
				FresnelConstTexture *fct = static_cast<FresnelConstTexture *>(t);

				tex->type = slg::ocl::FRESNELCONST_TEX;
				ASSIGN_SPECTRUM(tex->fresnelConst.n, fct->GetN());
				ASSIGN_SPECTRUM(tex->fresnelConst.k, fct->GetK());
				break;
			}
			case ABS_TEX: {
				AbsTexture *at = static_cast<AbsTexture *>(t);

				tex->type = slg::ocl::ABS_TEX;
				const Texture *refTex = at->GetTexture();
				tex->absTex.texIndex = scene->texDefs.GetTextureIndex(refTex);
				break;
			}
			case CLAMP_TEX: {
				ClampTexture *ct = static_cast<ClampTexture *>(t);

				tex->type = slg::ocl::CLAMP_TEX;
				const Texture *refTex = ct->GetTexture();
				tex->clampTex.texIndex = scene->texDefs.GetTextureIndex(refTex);
				tex->clampTex.minVal = ct->GetMinVal();
				tex->clampTex.maxVal = ct->GetMaxVal();
				break;
			}
			case BILERP_TEX: {
				BilerpTexture *bt = static_cast<BilerpTexture *>(t);
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
			default:
				throw runtime_error("Unknown texture in CompiledScene::CompileTextures(): " + boost::lexical_cast<string>(t->GetType()));
				break;
		}
	}

	const double tEnd = WallClockTime();
	SLG_LOG("Textures compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

//------------------------------------------------------------------------------
// Dynamic OpenCL code generation for texture evaluation
//------------------------------------------------------------------------------

static void AddTextureSource(stringstream &source,  const string &texName, const string &returnType,
		const string &type, const u_int i, const string &texArgs) {
	source << returnType << " Texture_Index" << i << "_Evaluate" << type << "(__global const Texture *texture,\n"
			"\t\t__global HitPoint *hitPoint\n"
			"\t\tTEXTURES_PARAM_DECL) {\n"
			"\treturn " << texName << "Texture_ConstEvaluate" << type << "(hitPoint" <<
				((texArgs.length() > 0) ? (", " + texArgs) : "") << ");\n"
			"}\n";
}

static void AddTextureSource(stringstream &source,  const string &texName,
		const u_int i, const string &texArgs) {
	AddTextureSource(source, texName, "float", "Float", i, texArgs);
	AddTextureSource(source, texName, "float3", "Spectrum", i, texArgs);
}

static void AddTextureBumpSource(stringstream &source, const vector<slg::ocl::Texture> &texs, const u_int i) {
	const slg::ocl::Texture *tex = &texs[i];

	switch (tex->type) {
		case slg::ocl::NORMALMAP_TEX: {
			source << "#if defined(PARAM_ENABLE_TEX_NORMALMAP)\n";
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\treturn NormalMapTexture_Bump(" << i << ", hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"}\n";
			source << "#endif\n";
			break;
		}
		case slg::ocl::IMAGEMAP: {
			source << "#if defined(PARAM_ENABLE_TEX_IMAGEMAP)\n";
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\tconst __global Texture *texture = &texs[" << i << "];\n"
					"\treturn ImageMapTexture_Bump(hitPoint, sampleDistance,\n" <<
						ToString(tex->imageMapTex.gain) << ", " <<
						ToString(tex->imageMapTex.imageMapIndex) << ", " <<
						"&texture->imageMapTex.mapping"
						" IMAGEMAPS_PARAM);\n"
					"}\n";
			source << "#endif\n";
			break;
		}
		case slg::ocl::ADD_TEX: {
			source << "#if defined(PARAM_ENABLE_TEX_ADD)\n";
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\tconst float3 tex1ShadeN = Texture_Index" << tex->addTex.tex1Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tconst float3 tex2ShadeN = Texture_Index" << tex->addTex.tex2Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\treturn normalize(tex1ShadeN + tex2ShadeN - VLOAD3F(&hitPoint->shadeN.x));\n"
					"}\n";
			source << "#endif\n";
			break;
		}
		case slg::ocl::SUBTRACT_TEX: {
			source << "#if defined(PARAM_ENABLE_TEX_SUBTRACT)\n";
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\tconst float3 tex1ShadeN = Texture_Index" << tex->addTex.tex1Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tconst float3 tex2ShadeN = Texture_Index" << tex->addTex.tex2Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\treturn normalize(tex1ShadeN - tex2ShadeN + VLOAD3F(&hitPoint->shadeN.x));\n"
					"}\n";
			source << "#endif\n";
			break;
		}
		case slg::ocl::MIX_TEX: {
			source << "#if defined(PARAM_ENABLE_TEX_MIX)\n";
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\t const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);\n"
					"\tconst float3 u = normalize(VLOAD3F(&hitPoint->dpdu.x));\n"
					"\tconst float3 v = normalize(cross(shadeN, u));\n"
					"\tfloat3 n = Texture_Index" << tex->mixTex.tex1Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tfloat nn = dot(n, shadeN);\n"
					"\tconst float du1 = dot(n, u) / nn;\n"
					"\tconst float dv1 = dot(n, v) / nn;\n"
					"\tn = Texture_Index" << tex->mixTex.tex2Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tnn = dot(n, shadeN);\n"
					"\tconst float du2 = dot(n, u) / nn;\n"
					"\tconst float dv2 = dot(n, v) / nn;\n"
					"\tn = Texture_Index" << tex->mixTex.amountTexIndex << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tnn = dot(n, shadeN);\n"
					"\tconst float dua = dot(n, u) / nn;\n"
					"\tconst float dva = dot(n, v) / nn;\n"
					"\tconst float t1 = Texture_Index" << tex->mixTex.tex1Index << "_EvaluateFloat(&texs[" << tex->mixTex.tex1Index << "], hitPoint TEXTURES_PARAM);\n"
					"\tconst float t2 = Texture_Index" << tex->mixTex.tex2Index << "_EvaluateFloat(&texs[" << tex->mixTex.tex2Index << "], hitPoint TEXTURES_PARAM);\n"
					"\tconst float amt = clamp(Texture_Index" << tex->mixTex.amountTexIndex << "_EvaluateFloat(&texs[" << tex->mixTex.amountTexIndex << "], hitPoint TEXTURES_PARAM), 0.f, 1.f);\n"
					"\tconst float du = mix(du1, du2, amt) + dua * (t2 - t1);\n"
					"\tconst float dv = mix(dv1, dv2, amt) + dva * (t2 - t1);\n"
					"\treturn normalize(shadeN + du * u + dv * v);\n"
					"}\n";
			source << "#endif\n";
			break;
		}
		case slg::ocl::SCALE_TEX: {
			source << "#if defined(PARAM_ENABLE_TEX_SCALE)\n";
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\t const float3 shadeN = VLOAD3F(&hitPoint->shadeN.x);\n"
					"\tconst float3 u = normalize(VLOAD3F(&hitPoint->dpdu.x));\n"
					"\tconst float3 v = normalize(cross(shadeN, u));\n"
					"\tfloat3 n = Texture_Index" << tex->scaleTex.tex1Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tfloat nn = dot(n, shadeN);\n"
					"\tconst float du1 = dot(n, u) / nn;\n"
					"\tconst float dv1 = dot(n, v) / nn;\n"
					"\tn = Texture_Index" << tex->scaleTex.tex2Index << "_Bump(hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"\tnn = dot(n, shadeN);\n"
					"\tconst float du2 = dot(n, u) / nn;\n"
					"\tconst float dv2 = dot(n, v) / nn;\n"
					"\tconst float t1 = Texture_Index" << tex->scaleTex.tex1Index << "_EvaluateFloat(&texs[" << tex->scaleTex.tex1Index << "], hitPoint TEXTURES_PARAM);\n"
					"\tconst float t2 = Texture_Index" << tex->scaleTex.tex2Index << "_EvaluateFloat(&texs[" << tex->scaleTex.tex2Index << "], hitPoint TEXTURES_PARAM);\n"
					"\tconst float du = du1 * t2 + t1 * du2;\n"
					"\tconst float dv = dv1 * t2 + t1 * dv2;\n"
					"\treturn normalize(shadeN + du * u + dv * v);\n"
					"}\n";
			source << "#endif\n";
			break;
		}
		case slg::ocl::CONST_FLOAT: {
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\treturn VLOAD3F(&hitPoint->shadeN.x);\n"
					"}\n";
			break;
		}
		case slg::ocl::CONST_FLOAT3: {
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\treturn VLOAD3F(&hitPoint->shadeN.x);\n"
					"}\n";
			break;
		}
		default: {
			source << "float3 Texture_Index" << i << "_Bump(__global HitPoint *hitPoint,\n"
					"\t\tconst float sampleDistance\n"
					"\t\tTEXTURES_PARAM_DECL) {\n"
					"\treturn GenericTexture_Bump(" << i << ", hitPoint, sampleDistance TEXTURES_PARAM);\n"
					"}\n";
			break;
		}
	}
}

string CompiledScene::GetTexturesEvaluationSourceCode() const {
	// Generate the source code for each texture that reference other textures
	// and constant textures
	stringstream source;

	const u_int texturesCount = texs.size();
	for (u_int i = 0; i < texturesCount; ++i) {
		const slg::ocl::Texture *tex = &texs[i];

		switch (tex->type) {
			case slg::ocl::CONST_FLOAT:
				AddTextureSource(source, "ConstFloat", i, ToString(tex->constFloat.value));
				break;
			case slg::ocl::CONST_FLOAT3:
				AddTextureSource(source, "ConstFloat3", i, ToOCLString(tex->constFloat3.color));
				break;
			case slg::ocl::IMAGEMAP:
				AddTextureSource(source, "ImageMap", i,
						ToString(tex->imageMapTex.gain) + ", " +
						ToString(tex->imageMapTex.imageMapIndex) + ", " +
						"&texture->imageMapTex.mapping"
						" IMAGEMAPS_PARAM");
				break;
			case slg::ocl::SCALE_TEX: {
				AddTextureSource(source, "Scale", "float", "Float", i,
						AddTextureSourceCall("Float", tex->scaleTex.tex1Index) + ", " +
						AddTextureSourceCall("Float", tex->scaleTex.tex2Index));
				AddTextureSource(source, "Scale", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->scaleTex.tex1Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->scaleTex.tex2Index));
				break;
			}
			case FRESNEL_APPROX_N:
				AddTextureSource(source, "FresnelApproxN", "float", "Float", i,
						AddTextureSourceCall("Float", tex->fresnelApproxN.texIndex));
				AddTextureSource(source, "FresnelApproxN", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->fresnelApproxN.texIndex));
				break;
			case FRESNEL_APPROX_K:
				AddTextureSource(source, "FresnelApproxK", "float", "Float", i,
						AddTextureSourceCall("Float", tex->fresnelApproxK.texIndex));
				AddTextureSource(source, "FresnelApproxK", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->fresnelApproxK.texIndex));
				break;
			case slg::ocl::MIX_TEX: {
				AddTextureSource(source, "Mix", "float", "Float", i,
						AddTextureSourceCall("Float", tex->mixTex.amountTexIndex) + ", " +
						AddTextureSourceCall("Float", tex->mixTex.tex1Index) + ", " +
						AddTextureSourceCall("Float", tex->mixTex.tex2Index));
				AddTextureSource(source, "Mix", "float3", "Spectrum", i,
						AddTextureSourceCall("Float", tex->mixTex.amountTexIndex) + ", " +
						AddTextureSourceCall("Spectrum", tex->mixTex.tex1Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->mixTex.tex2Index));
				break;
			}
			case slg::ocl::ADD_TEX: {
				AddTextureSource(source, "Add", "float", "Float", i,
						AddTextureSourceCall("Float", tex->addTex.tex1Index) + ", " +
						AddTextureSourceCall("Float", tex->addTex.tex2Index));
				AddTextureSource(source, "Add", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->addTex.tex1Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->addTex.tex2Index));
				break;
			}
			case slg::ocl::SUBTRACT_TEX: {
				AddTextureSource(source, "Subtract", "float", "Float", i,
								 AddTextureSourceCall("Float", tex->subtractTex.tex1Index) + ", " +
								 AddTextureSourceCall("Float", tex->subtractTex.tex2Index));
				AddTextureSource(source, "Subtract", "float3", "Spectrum", i,
								 AddTextureSourceCall("Spectrum", tex->subtractTex.tex1Index) + ", " +
								 AddTextureSourceCall("Spectrum", tex->subtractTex.tex2Index));
				break;
			}
			case slg::ocl::HITPOINTCOLOR:
				AddTextureSource(source, "HitPointColor", i, "");
				break;
			case slg::ocl::HITPOINTALPHA:
				AddTextureSource(source, "HitPointAlpha", i, "");
				break;
			case slg::ocl::HITPOINTGREY:
				AddTextureSource(source, "HitPointGrey", i, "");
				break;
			case slg::ocl::BLENDER_BLEND:
				AddTextureSource(source, "BlenderBlend", i,
						ToString(tex->blenderBlend.type) + ", " +
						ToString(tex->blenderBlend.direction) + ", " +
						ToString(tex->blenderBlend.contrast) + ", " +
						ToString(tex->blenderBlend.bright) + ", " +
						"&texture->blenderBlend.mapping");
				break;
			case slg::ocl::BLENDER_CLOUDS:
				AddTextureSource(source, "BlenderClouds", i,
						ToString(tex->blenderClouds.noisebasis) + ", " +
						ToString(tex->blenderClouds.noisesize) + ", " +
						ToString(tex->blenderClouds.noisedepth) + ", " +
						ToString(tex->blenderClouds.contrast) + ", " +
						ToString(tex->blenderClouds.bright) + ", " +
						ToString(tex->blenderClouds.hard) + ", " +
						"&texture->blenderClouds.mapping");
				break;
			case slg::ocl::BLENDER_DISTORTED_NOISE:
				AddTextureSource(source, "BlenderDistortedNoise", i,
						ToString(tex->blenderDistortedNoise.noisedistortion) + ", " +
						ToString(tex->blenderDistortedNoise.noisebasis) + ", " +
						ToString(tex->blenderDistortedNoise.distortion) + ", " +
						ToString(tex->blenderDistortedNoise.noisesize) + ", " +
						ToString(tex->blenderDistortedNoise.contrast) + ", " +
						ToString(tex->blenderDistortedNoise.bright) + ", " +
						"&texture->blenderDistortedNoise.mapping");
				break;
			case slg::ocl::BLENDER_MAGIC:
				AddTextureSource(source, "BlenderMagic", "float", "Float", i,
						ToString(tex->blenderMagic.noisedepth) + ", " +
						ToString(tex->blenderMagic.turbulence) + ", " +
						ToString(tex->blenderMagic.contrast) + ", " +
						ToString(tex->blenderMagic.bright) + ", " +
						"&texture->blenderMagic.mapping");
				AddTextureSource(source, "BlenderMagic", "float3", "Spectrum", i,
						ToString(tex->blenderMagic.noisedepth) + ", " +
						ToString(tex->blenderMagic.turbulence) + ", " +
						ToString(tex->blenderMagic.contrast) + ", " +
						ToString(tex->blenderMagic.bright) + ", " +
						"&texture->blenderMagic.mapping");
				break;
			case slg::ocl::BLENDER_MARBLE:
				AddTextureSource(source, "BlenderMarble", i,
						ToString(tex->blenderMarble.type) + ", " +
						ToString(tex->blenderMarble.noisebasis) + ", " +
						ToString(tex->blenderMarble.noisebasis2) + ", " +
						ToString(tex->blenderMarble.noisesize) + ", " +
						ToString(tex->blenderMarble.turbulence) + ", " +
						ToString(tex->blenderMarble.noisedepth) + ", " +
						ToString(tex->blenderMarble.contrast) + ", " +
						ToString(tex->blenderMarble.bright) + ", " +
						ToString(tex->blenderMarble.hard) + ", " +
						"&texture->blenderMagic.mapping");
				break;
			case slg::ocl::BLENDER_MUSGRAVE:
				AddTextureSource(source, "BlenderMusgrave", i,
						ToString(tex->blenderMusgrave.type) + ", " +
						ToString(tex->blenderMusgrave.noisebasis) + ", " +
						ToString(tex->blenderMusgrave.dimension) + ", " +
						ToString(tex->blenderMusgrave.intensity) + ", " +
						ToString(tex->blenderMusgrave.lacunarity) + ", " +
						ToString(tex->blenderMusgrave.offset) + ", " +
						ToString(tex->blenderMusgrave.gain) + ", " +
						ToString(tex->blenderMusgrave.octaves) + ", " +
						ToString(tex->blenderMusgrave.noisesize) + ", " +
						ToString(tex->blenderMusgrave.contrast) + ", " +
						ToString(tex->blenderMusgrave.bright) + ", " +
						"&texture->blenderMusgrave.mapping");
				break;
			case slg::ocl::BLENDER_STUCCI:
				AddTextureSource(source, "BlenderStucci", i,
						ToString(tex->blenderStucci.type) + ", " +
						ToString(tex->blenderStucci.noisebasis) + ", " +
						ToString(tex->blenderStucci.noisesize) + ", " +
						ToString(tex->blenderStucci.turbulence) + ", " +
						ToString(tex->blenderStucci.contrast) + ", " +
						ToString(tex->blenderStucci.bright) + ", " +
						ToString(tex->blenderStucci.hard) + ", " +
						"&texture->blenderStucci.mapping");
				break;
            case slg::ocl::BLENDER_WOOD:
				AddTextureSource(source, "BlenderWood", i,
						ToString(tex->blenderWood.type) + ", " +
						ToString(tex->blenderWood.noisebasis2) + ", " +
						ToString(tex->blenderWood.noisebasis) + ", " +
						ToString(tex->blenderWood.noisesize) + ", " +
						ToString(tex->blenderWood.turbulence) + ", " +
						ToString(tex->blenderWood.contrast) + ", " +
						ToString(tex->blenderWood.bright) + ", " +
						ToString(tex->blenderWood.hard) + ", " +
						"&texture->blenderWood.mapping");
				break;
			case slg::ocl::BLENDER_VORONOI:
				AddTextureSource(source, "BlenderVoronoi", i,
						ToString(tex->blenderVoronoi.distancemetric) + ", " +
						ToString(tex->blenderVoronoi.feature_weight1) + ", " +
						ToString(tex->blenderVoronoi.feature_weight2) + ", " +
						ToString(tex->blenderVoronoi.feature_weight3) + ", " +
						ToString(tex->blenderVoronoi.feature_weight4) + ", " +
						ToString(tex->blenderVoronoi.noisesize) + ", " +
						ToString(tex->blenderVoronoi.intensity) + ", " +
						ToString(tex->blenderVoronoi.exponent) + ", " +
						ToString(tex->blenderVoronoi.contrast) + ", " +
						ToString(tex->blenderVoronoi.bright) + ", " +
						"&texture->blenderVoronoi.mapping");
				break;
			case slg::ocl::CHECKERBOARD2D:
				AddTextureSource(source, "CheckerBoard2D", "float", "Float", i,
						AddTextureSourceCall("Float", tex->checkerBoard2D.tex1Index) + ", " +
						AddTextureSourceCall("Float", tex->checkerBoard2D.tex2Index) + ", " +
						"&texture->checkerBoard2D.mapping");
				AddTextureSource(source, "CheckerBoard2D", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->checkerBoard2D.tex1Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->checkerBoard2D.tex2Index) + ", " +
						"&texture->checkerBoard2D.mapping");
				break;
			case slg::ocl::CHECKERBOARD3D:
				AddTextureSource(source, "CheckerBoard3D", "float", "Float", i,
						AddTextureSourceCall("Float", tex->checkerBoard3D.tex1Index) + ", " +
						AddTextureSourceCall("Float", tex->checkerBoard3D.tex2Index) + ", " +
						"&texture->checkerBoard3D.mapping");
				AddTextureSource(source, "CheckerBoard3D", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->checkerBoard3D.tex1Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->checkerBoard3D.tex2Index) + ", " +
						"&texture->checkerBoard3D.mapping");
				break;
			case slg::ocl::CLOUD_TEX:
				AddTextureSource(source, "Cloud", i,
					ToString(tex->cloud.radius) + ", " +
					ToString(tex->cloud.numspheres) + ", " +
					ToString(tex->cloud.spheresize) + ", " +
					ToString(tex->cloud.sharpness) + ", " +
					ToString(tex->cloud.basefadedistance) + ", " +
					ToString(tex->cloud.baseflatness) + ", " +
					ToString(tex->cloud.variability) + ", " +
					ToString(tex->cloud.omega) + ", " +
					ToString(tex->cloud.noisescale) + ", " +
					ToString(tex->cloud.noiseoffset) + ", " +
					ToString(tex->cloud.turbulence) + ", " +
					ToString(tex->cloud.octaves) + ", " +
					"&texture->cloud.mapping");
				break;			
			case slg::ocl::FBM_TEX:
				AddTextureSource(source, "FBM", i,
						ToString(tex->fbm.omega) + ", " +
						ToString(tex->fbm.octaves) + ", " +
						"&texture->fbm.mapping");
				break;
			case slg::ocl::MARBLE:
				AddTextureSource(source, "Marble", i,
						ToString(tex->marble.scale) + ", " +
						ToString(tex->marble.omega) + ", " +
						ToString(tex->marble.octaves) + ", " +
						ToString(tex->marble.variation) + ", " +
						"&texture->marble.mapping");
				break;
			case slg::ocl::DOTS:
				AddTextureSource(source, "Dots", "float", "Float", i,
						AddTextureSourceCall("Float", tex->dots.insideIndex) + ", " +
						AddTextureSourceCall("Float", tex->dots.outsideIndex) + ", " +
						"&texture->dots.mapping");
				AddTextureSource(source, "Dots", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->dots.insideIndex) + ", " +
						AddTextureSourceCall("Spectrum", tex->dots.outsideIndex) + ", " +
						"&texture->dots.mapping");
				break;
			case slg::ocl::BRICK:
				AddTextureSource(source, "Brick", "float", "Float", i,
						AddTextureSourceCall("Float", tex->brick.tex1Index) + ", " +
						AddTextureSourceCall("Float", tex->brick.tex2Index) + ", " +
						AddTextureSourceCall("Float", tex->brick.tex3Index) + ", " +
						ToString(tex->brick.bond) + ", " +
						ToString(tex->brick.brickwidth) + ", " +
						ToString(tex->brick.brickheight) + ", " +
						ToString(tex->brick.brickdepth) + ", " +
						ToString(tex->brick.mortarsize) + ", " +
						"(float3)(" + ToString(tex->brick.offsetx) + ", " + ToString(tex->brick.offsetx) + ", " + ToString(tex->brick.offsetx) + "), " +
						ToString(tex->brick.run) + ", " +
						ToString(tex->brick.mortarwidth) + ", " +
						ToString(tex->brick.mortarheight) + ", " +
						ToString(tex->brick.mortardepth) + ", " +
						ToString(tex->brick.proportion) + ", " +
						ToString(tex->brick.invproportion) + ", " +
						"&texture->brick.mapping");
				AddTextureSource(source, "Brick", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->brick.tex1Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->brick.tex2Index) + ", " +
						AddTextureSourceCall("Spectrum", tex->brick.tex3Index) + ", " +
						ToString(tex->brick.bond) + ", " +
						ToString(tex->brick.brickwidth) + ", " +
						ToString(tex->brick.brickheight) + ", " +
						ToString(tex->brick.brickdepth) + ", " +
						ToString(tex->brick.mortarsize) + ", " +
						"(float3)(" + ToString(tex->brick.offsetx) + ", " + ToString(tex->brick.offsetx) + ", " + ToString(tex->brick.offsetx) + "), " +
						ToString(tex->brick.run) + ", " +
						ToString(tex->brick.mortarwidth) + ", " +
						ToString(tex->brick.mortarheight) + ", " +
						ToString(tex->brick.mortardepth) + ", " +
						ToString(tex->brick.proportion) + ", " +
						ToString(tex->brick.invproportion) + ", " +
						"&texture->brick.mapping");
				break;
			case slg::ocl::WINDY:
				AddTextureSource(source, "Windy", i,
						"&texture->windy.mapping");
				break;
			case slg::ocl::WRINKLED:
				AddTextureSource(source, "Wrinkled", i,
						ToString(tex->marble.omega) + ", " +
						ToString(tex->marble.octaves) + ", " +
						"&texture->wrinkled.mapping");
				break;
			case slg::ocl::UV_TEX:
				AddTextureSource(source, "UV", i,
						"&texture->uvTex.mapping");
				break;
			case slg::ocl::BAND_TEX:
				AddTextureSource(source, "Band", "float", "Float", i,
						"texture->band.interpType, " +
						ToString(tex->band.size) + ", " +
						"texture->band.offsets, "
						"texture->band.values, " +
						AddTextureSourceCall("Float", tex->band.amountTexIndex));
				AddTextureSource(source, "Band", "float3", "Spectrum", i,
						"texture->band.interpType, " +
						ToString(tex->band.size) + ", " +
						"texture->band.offsets, "
						"texture->band.values, " +
						AddTextureSourceCall("Float", tex->band.amountTexIndex));
				break;
			case slg::ocl::NORMALMAP_TEX:
				AddTextureSource(source, "NormalMap", i, "");
				break;
			case slg::ocl::BLACKBODY_TEX:
				AddTextureSource(source, "BlackBody", i, ToOCLString(tex->blackBody.rgb));
				break;
			case slg::ocl::IRREGULARDATA_TEX:
				AddTextureSource(source, "IrregularData", i, ToOCLString(tex->irregularData.rgb));
				break;
			case slg::ocl::FRESNELCOLOR_TEX:
				AddTextureSource(source, "FresnelColor", "float", "Float", i,
						AddTextureSourceCall("Float", tex->fresnelColor.krIndex));
				AddTextureSource(source, "FresnelColor", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->fresnelColor.krIndex));
				break;
			case slg::ocl::FRESNELCONST_TEX:
				AddTextureSource(source, "FresnelConst", "float", "Float", i, "");
				AddTextureSource(source, "FresnelConst", "float3", "Spectrum", i, "");
				break;
			case slg::ocl::ABS_TEX: {
				AddTextureSource(source, "Abs", "float", "Float", i,
						AddTextureSourceCall("Float", tex->absTex.texIndex));
				AddTextureSource(source, "Abs", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->absTex.texIndex));
				break;
			}
			case slg::ocl::CLAMP_TEX: {
				AddTextureSource(source, "Clamp", "float", "Float", i,
						AddTextureSourceCall("Float", tex->clampTex.texIndex) + ", " +
						ToString(tex->clampTex.minVal) + ", " +
						ToString(tex->clampTex.maxVal));
				AddTextureSource(source, "Clamp", "float3", "Spectrum", i,
						AddTextureSourceCall("Spectrum", tex->clampTex.texIndex) + ", " +
						ToString(tex->clampTex.minVal) + ", " +
						ToString(tex->clampTex.maxVal));
				break;
			}
			case slg::ocl::BILERP_TEX: {
				AddTextureSource(source, "Bilerp", "float", "Float", i, AddTextureSourceCall("Float", tex->bilerpTex.t00Index) + ", " +
					AddTextureSourceCall("Float", tex->bilerpTex.t01Index) + ", " +
					AddTextureSourceCall("Float", tex->bilerpTex.t10Index) + ", " +
					AddTextureSourceCall("Float", tex->bilerpTex.t11Index));
				AddTextureSource(source, "Bilerp", "float3", "Spectrum", i, AddTextureSourceCall("Spectrum", tex->bilerpTex.t00Index) + ", " +
					AddTextureSourceCall("Spectrum", tex->bilerpTex.t01Index) + ", " +
					AddTextureSourceCall("Spectrum", tex->bilerpTex.t10Index) + ", " +
					AddTextureSourceCall("Spectrum", tex->bilerpTex.t11Index));
				break;
			}
			default:
				throw runtime_error("Unknown texture in CompiledScene::GetTexturesEvaluationSourceCode(): " + boost::lexical_cast<string>(tex->type));
				break;
		}
	}

	// Generate the code for evaluating a generic float texture
	source << "float Texture_GetFloatValue(const uint texIndex, __global HitPoint *hitPoint TEXTURES_PARAM_DECL) {\n"
			"\t __global const Texture *tex = &texs[texIndex];\n"
			"\tswitch (texIndex) {\n";
	for (u_int i = 0; i < texturesCount; ++i) {
		source << "\t\tcase " << i << ": return Texture_Index" << i << "_EvaluateFloat(tex, hitPoint TEXTURES_PARAM);\n";
	}
	source << "\t\tdefault: return 0.f;\n"
			"\t}\n"
			"}\n";

	// Generate the code for evaluating a generic spectrum texture
	source << "float3 Texture_GetSpectrumValue(const uint texIndex, __global HitPoint *hitPoint TEXTURES_PARAM_DECL) {\n"
			"\t __global const Texture *tex = &texs[texIndex];\n"
			"\tswitch (texIndex) {\n";
	for (u_int i = 0; i < texturesCount; ++i) {
		source << "\t\tcase " << i << ": return Texture_Index" << i << "_EvaluateSpectrum(tex, hitPoint TEXTURES_PARAM);\n";
	}
	source << "\t\tdefault: return BLACK;\n"
			"\t}\n"
			"}\n";

	// Add bump and normal mapping functions
	source << slg::ocl::KernelSource_texture_bump_funcs;

	source << "#if defined(PARAM_HAS_BUMPMAPS)\n";
	for (u_int i = 0; i < texturesCount; ++i)
		AddTextureBumpSource(source, texs, i);
	source << "#endif\n";

	return source.str();
}

#endif

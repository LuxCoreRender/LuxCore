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

#include <memory>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>

#include "slg/scene/scene.h"
#include "slg/utils/filenameresolver.h"

#include "slg/textures/band.h"
#include "slg/textures/bevel.h"
#include "slg/textures/bilerp.h"
#include "slg/textures/blackbody.h"
#include "slg/textures/blender_texture.h"
#include "slg/textures/bombing.h"
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
#include "slg/textures/fresnel/fresnelcauchy.h"
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
#include "slg/textures/vectormath/dotproduct.h"
#include "slg/textures/vectormath/makefloat3.h"
#include "slg/textures/vectormath/splitfloat3.h"
#include "slg/textures/windy.h"
#include "slg/textures/wireframe.h"
#include "slg/textures/wrinkled.h"
#include "slg/textures/uv.h"
#include "slg/textures/triplanar.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace slg::blender;

void Scene::ParseTextures(const Properties &props) {
	vector<string> texKeys = props.GetAllUniqueSubNames("scene.textures");
	if (texKeys.size() == 0) {
		// There is no texture definition
		return;
	}

	for(const string &key: texKeys) {
		// Extract the texture name
		const string texName = Property::ExtractField(key, 2);
		if (texName == "")
			throw runtime_error("Syntax error in texture definition: " + texName);

		if (texDefs.IsTextureDefined(texName)) {
			SDL_LOG("Texture re-definition: " << texName);
		} else {
			SDL_LOG("Texture definition: " << texName);
		}

		auto tex = CreateTexture(texName, props);
		// Density grid data are stored with image maps
		if ((tex->GetType() == IMAGEMAP) || (tex->GetType() == DENSITYGRID_TEX))
			editActions.AddAction(IMAGEMAPS_EDIT);

		if (texDefs.IsTextureDefined(texName)) {
			// A replacement for an existing texture
			auto& oldTex = texDefs.GetTexture(texName);

			// FresnelTexture can be replaced only with other FresnelTexture
			if (
				dynamic_cast<const FresnelTexture *>(&oldTex)
				&& !dynamic_cast<FresnelTexture*>(tex.get())
			) {
				throw std::runtime_error(
					"You can not replace a fresnel texture with the texture: " + texName
				);
			}

			auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(std::move(tex)));
			matDefs.UpdateTextureReferences(*oldTexPtr, newTexRef);
			moveToTrash(std::move(oldTexPtr));

		} else {
			// Only a new texture
			auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(std::move(tex)));
			moveToTrash(std::move(oldTexPtr));
		}
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

TextureUPtr Scene::CreateTexture(const string &texName, const Properties &props) {
	const string propName = "scene.textures." + texName;
	const string texType = props.Get(Property(propName + ".type")("imagemap")).Get<string>();

	TextureUPtr tex;
	if (texType == "imagemap") {
		const string name = props.Get(Property(propName + ".file")("image.png")).Get<string>();
		auto& im = imgMapCache.GetImageMap(name, ImageMapConfig(props, propName), true);

		const bool randomizedTiling = props.Get(Property(propName + ".randomizedtiling.enable")(false)).Get<bool>();
		if (randomizedTiling && (im.GetStorage().GetWrapType() != ImageMapStorage::REPEAT))
			throw runtime_error("Randomized tiling requires REPEAT wrap type in imagemap texture: " + propName);

		const float gain = props.Get(Property(propName + ".gain")(1.0)).Get<double>();
		auto mapping = CreateTextureMapping2D(propName + ".mapping", props);
		tex = ImageMapTexture::AllocImageMapTexture(
			texName,
			imgMapCache,
			im,
			std::move(mapping),
			gain,
			randomizedTiling,
			*GetRandomImageMap()
		);
	} else if (texType == "constfloat1") {
		float v = props.Get(Property(propName + ".value")(1.0)).Get<double>();
		
		ColorSpaceConfig colorCfg;
		ColorSpaceConfig::FromProperties(props, propName, colorCfg, ColorSpaceConfig::defaultNopConfig);
		colorSpaceConv.ConvertFrom(colorCfg, v);
		
		tex = std::make_unique<ConstFloatTexture>(v);
	} else if (texType == "constfloat3") {
		Spectrum c = props.Get(Property(propName + ".value")(1.f, 1.f, 1.f)).Get<Spectrum>();

		ColorSpaceConfig colorCfg;
		ColorSpaceConfig::FromProperties(props, propName, colorCfg, ColorSpaceConfig::defaultNopConfig);
		colorSpaceConv.ConvertFrom(colorCfg, c);

		tex = std::make_unique<ConstFloat3Texture>(c);
	} else if (texType == "scale") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<ScaleTexture>(tex1, tex2);
	} else if (texType == "fresnelapproxn") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture")(.5f, .5f, .5f)));
		tex = std::make_unique<FresnelApproxNTexture>(tex1);
	} else if (texType == "fresnelapproxk") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture")(.5f, .5f, .5f)));
		tex = std::make_unique<FresnelApproxKTexture>(tex1);
	} else if (texType == "checkerboard2d") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(0.f)));
		tex = std::make_unique<CheckerBoard2DTexture>(
			CreateTextureMapping2D(propName + ".mapping", props), tex1, tex2
		);
	} else if (texType == "checkerboard3d") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(0.f)));

		tex = std::make_unique<CheckerBoard3DTexture>(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "densitygrid") {
		if (!props.IsDefined(propName + ".nx") || !props.IsDefined(propName + ".ny") || !props.IsDefined(propName + ".nz"))
			throw runtime_error("Missing dimensions property in densitygrid texture: " + propName);

	    const u_int nx = props.Get(Property(propName + ".nx")(1)).Get<int>();
	    const u_int ny = props.Get(Property(propName + ".ny")(1)).Get<int>();
	    const u_int nz = props.Get(Property(propName + ".nz")(1)).Get<int>();
		const ImageMapStorage::WrapType wrapMode = ImageMapStorage::String2WrapType(
				props.Get(Property(propName + ".wrap")("repeat")).Get<string>());
		const ImageMapStorage::StorageType storageType = ImageMapStorage::String2StorageType(
				props.Get(Property(propName + ".storage")("auto")).Get<string>());

		ImageMapUPtr imgMap;
		if (props.IsDefined(propName + ".data")) {
			const Property &dataProp = props.Get(Property(propName + ".data"));

			const u_int dataSize = nx * ny * nz;
			if (dataProp.GetSize() != dataSize)
				throw runtime_error("Number of data elements (" + ToString(dataProp.GetSize()) +
						") doesn't match dimension of densitygrid texture: " + propName +
					    " (expected: " + ToString(dataSize) + ")");

			// Create an image map with the data
			imgMap = DensityGridTexture::ParseData(dataProp, false, nx, ny, nz, storageType, wrapMode);
		} else if (props.IsDefined(propName + ".data3")) {
			const Property &dataProp = props.Get(Property(propName + ".data3"));

			const u_int dataSize = nx * ny * nz * 3;
			if (dataProp.GetSize() != dataSize)
				throw runtime_error("Number of data elements (" + ToString(dataProp.GetSize()) +
						") doesn't match dimension of densitygrid texture: " + propName +
					    " (expected: " + ToString(dataSize) + ")");

			// Create an image map with the data
			imgMap = DensityGridTexture::ParseData(dataProp, true, nx, ny, nz, storageType, wrapMode);
		} else if (props.IsDefined(propName + ".openvdb.file")) {
			// Create an image map with the data
			const string fileName = SLG_FileNameResolver.ResolveFile(props.Get(Property(propName + ".openvdb.file")).Get<string>());
			const string gridName = props.Get(Property(propName + ".openvdb.grid")).Get<string>();
			imgMap = DensityGridTexture::ParseOpenVDB(fileName, gridName, nx, ny, nz, storageType, wrapMode);
		} else
			throw runtime_error("Missing data property or OpenVDB file in densitygrid texture: " + texName);

		// Add the image map to the cache
		const string name ="LUXCORE_DENSITYGRID_" + texName;
		imgMap->SetName(name);
		auto& imgMapRef = imgMapCache.DefineImageMap(std::move(imgMap));

		tex = std::make_unique<DensityGridTexture>(
			CreateTextureMapping3D(propName + ".mapping", props),
			nx, ny, nz, imgMapRef
		);
	} else if (texType == "mix") {
		auto& amtTex = GetTexture(props.Get(Property(propName + ".amount")(.5f)));
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(0.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));

		tex = std::make_unique<MixTexture>(amtTex, tex1, tex2);
	} else if (texType == "fbm") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5)).Get<double>();

		tex = std::make_unique<FBMTexture>(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "marble") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5)).Get<double>();
		const float scale = props.Get(Property(propName + ".scale")(1.0)).Get<double>();
		const float variation = props.Get(Property(propName + ".variation")(.2)).Get<double>();

		tex = std::make_unique<MarbleTexture>(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega, scale, variation);
	} else if (texType == "blender_blend") {
		const string progressiontype = props.Get(Property(propName + ".progressiontype")("linear")).Get<string>();
		const string direct = props.Get(Property(propName + ".direction")("horizontal")).Get<string>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderBlendTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				progressiontype, (direct=="vertical"), bright, contrast);
	} else if (texType == "blender_clouds") {
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const int noisedepth = Clamp(props.Get(Property(propName + ".noisedepth")(2)).Get<int>(), 0, 25);
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderCloudsTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				noisebasis, noisesize, noisedepth, (hard == "hard_noise"), bright, contrast);
	} else if (texType == "blender_distortednoise") {
		const string noisedistortion = props.Get(Property(propName + ".noise_distortion")("blender_original")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const float distortion = props.Get(Property(propName + ".distortion")(1.0)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderDistortedNoiseTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				noisedistortion, noisebasis, distortion, noisesize, bright, contrast);
	} else if (texType == "blender_magic") {
		const int noisedepth = Clamp(props.Get(Property(propName + ".noisedepth")(2)).Get<int>(), 0, 25);
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.0)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderMagicTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				noisedepth, turbulence, bright, contrast);
	} else if (texType == "blender_marble") {
		const string marbletype = props.Get(Property(propName + ".marbletype")("soft")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const int noisedepth = Clamp(props.Get(Property(propName + ".noisedepth")(2)).Get<int>(), 0, 25);
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.0)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderMarbleTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				marbletype, noisebasis, noisebasis2, noisesize, turbulence, noisedepth, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_musgrave") {
		const string musgravetype = props.Get(Property(propName + ".musgravetype")("multifractal")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float dimension = props.Get(Property(propName + ".dimension")(1.0)).Get<double>();
		const float intensity = props.Get(Property(propName + ".intensity")(1.0)).Get<double>();
		const float lacunarity = props.Get(Property(propName + ".lacunarity")(1.0)).Get<double>();
		const float offset = props.Get(Property(propName + ".offset")(1.0)).Get<double>();
		const float gain = props.Get(Property(propName + ".gain")(1.0)).Get<double>();
		const float octaves = props.Get(Property(propName + ".octaves")(2.0)).Get<double>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderMusgraveTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				musgravetype, noisebasis, dimension, intensity, lacunarity, offset, gain, octaves, noisesize, bright, contrast);
	} else if (texType == "blender_noise") {
		const int noisedepth = Clamp(props.Get(Property(propName + ".noisedepth")(2)).Get<int>(), 0, 25);
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderNoiseTexture>(noisedepth, bright, contrast);
	} else if (texType == "blender_stucci") {
		const string stuccitype = props.Get(Property(propName + ".stuccitype")("plastic")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.0)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderStucciTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				stuccitype, noisebasis, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_wood") {
		const string woodtype = props.Get(Property(propName + ".woodtype")("bands")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.0)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderWoodTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				woodtype, noisebasis2, noisebasis, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_voronoi") {
		const float intensity = props.Get(Property(propName + ".intensity")(1.0)).Get<double>();
		const float exponent = props.Get(Property(propName + ".exponent")(2.0)).Get<double>();
		const string distmetric = props.Get(Property(propName + ".distmetric")("actual_distance")).Get<string>();
		const float fw1 = props.Get(Property(propName + ".w1")(1.0)).Get<double>();
		const float fw2 = props.Get(Property(propName + ".w2")(0.0)).Get<double>();
		const float fw3 = props.Get(Property(propName + ".w3")(0.0)).Get<double>();
		const float fw4 = props.Get(Property(propName + ".w4")(0.0)).Get<double>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.250)).Get<double>();
		const float bright = props.Get(Property(propName + ".bright")(1.0)).Get<double>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.0)).Get<double>();

		tex = std::make_unique<BlenderVoronoiTexture>(CreateTextureMapping3D(propName + ".mapping", props), intensity, exponent, fw1, fw2, fw3, fw4, distmetric, noisesize, bright, contrast);
	} else if (texType == "dots") {
		auto& insideTex = GetTexture(props.Get(Property(propName + ".inside")(1.f)));
		auto& outsideTex = GetTexture(props.Get(Property(propName + ".outside")(0.f)));

		tex = std::make_unique<DotsTexture>(CreateTextureMapping2D(propName + ".mapping", props), insideTex, outsideTex);
	} else if (texType == "brick") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".bricktex")(1.f, 1.f, 1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".mortartex")(.2f, .2f, .2f)));
		auto& tex3 = GetTexture(props.Get(Property(propName + ".brickmodtex")(1.f, 1.f, 1.f)));

		const float modulationBias = Clamp(props.Get(Property(propName + ".brickmodbias")(0.0)).Get<double>(), -1.0, 1.0);
		const string brickbond = props.Get(Property(propName + ".brickbond")("running")).Get<string>();
		const float brickwidth = props.Get(Property(propName + ".brickwidth")(.30)).Get<double>();
		const float brickheight = props.Get(Property(propName + ".brickheight")(.10)).Get<double>();
		const float brickdepth = props.Get(Property(propName + ".brickdepth")(.150)).Get<double>();
		const float mortarsize = props.Get(Property(propName + ".mortarsize")(.010)).Get<double>();
		const float brickrun = props.Get(Property(propName + ".brickrun")(.750)).Get<double>();

		tex = std::make_unique<BrickTexture>(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2, tex3,
				brickwidth, brickheight, brickdepth, mortarsize, brickrun, brickbond, modulationBias);
	} else if (texType == "add") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<AddTexture>(tex1, tex2);
	} else if (texType == "subtract") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<SubtractTexture>(tex1, tex2);
	} else if (texType == "windy") {
		tex = std::make_unique<WindyTexture>(CreateTextureMapping3D(propName + ".mapping", props));
	} else if (texType == "wrinkled") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.50)).Get<double>();

		tex = std::make_unique<WrinkledTexture>(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "uv") {
		tex = std::make_unique<UVTexture>(CreateTextureMapping2D(propName + ".mapping", props));
	} else if (texType == "band") {
		const string interpTypeString = props.Get(Property(propName + ".interpolation")("linear")).Get<string>();
		const BandTexture::InterpolationType interpType = BandTexture::String2InterpolationType(interpTypeString);

		auto& amtTex = GetTexture(props.Get(Property(propName + ".amount")(.5f)));

		vector<float> offsets;
		vector<Spectrum> values;
		for (u_int i = 0; props.IsDefined(propName + ".offset" + ToString(i)); ++i) {
			const float offset = props.Get(Property(propName + ".offset" + ToString(i))(0.0)).Get<double>();
			const Spectrum value = GetColor(props.Get(Property(propName + ".value" + ToString(i))(1.f, 1.f, 1.f)));

			offsets.push_back(offset);
			values.push_back(value);
		}
		if (offsets.size() == 0)
			throw runtime_error("Empty Band texture: " + texName);

		tex = std::make_unique<BandTexture>(interpType, amtTex, offsets, values);
	} else if (texType == "hitpointcolor") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = std::make_unique<HitPointColorTexture>(dataIndex);
	} else if (texType == "hitpointalpha") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = std::make_unique<HitPointAlphaTexture>(dataIndex);
	} else if (texType == "hitpointgrey") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		const int channel = props.Get(Property(propName + ".channel")(-1)).Get<int>();

		tex = std::make_unique<HitPointGreyTexture>(dataIndex,
				((channel != 0) && (channel != 1) && (channel != 2)) ?
					numeric_limits<u_int>::max() : static_cast<u_int>(channel));
	} else if (texType == "hitpointvertexaov") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = std::make_unique<HitPointVertexAOVTexture>(dataIndex);
	} else if (texType == "hitpointtriangleaov") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = std::make_unique<HitPointTriangleAOVTexture>(dataIndex);
	} else if (texType == "cloud") {
		const float radius = props.Get(Property(propName + ".radius")(.50)).Get<double>();
		const float noisescale = props.Get(Property(propName + ".noisescale")(.50)).Get<double>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(0.010)).Get<double>();
		const float sharpness = props.Get(Property(propName + ".sharpness")(6.00)).Get<double>();
		const float noiseoffset = props.Get(Property(propName + ".noiseoffset")(.00)).Get<double>();
		const int spheres = props.Get(Property(propName + ".spheres")(0)).Get<int>();
		const int octaves = props.Get(Property(propName + ".octaves")(1)).Get<int>();
		const float omega = props.Get(Property(propName + ".omega")(.50)).Get<double>();
		const float variability = props.Get(Property(propName + ".variability")(.90)).Get<double>();
		const float baseflatness = props.Get(Property(propName + ".baseflatness")(.80)).Get<double>();
		const float spheresize = props.Get(Property(propName + ".spheresize")(.150)).Get<double>();

		tex = std::make_unique<CloudTexture>(CreateTextureMapping3D(propName + ".mapping", props), radius, noisescale, turbulence,
								sharpness, noiseoffset, spheres, octaves, omega, variability, baseflatness, spheresize);
	} else if (texType == "blackbody") {
		const float temperature = Max(props.Get(Property(propName + ".temperature")(6500.0)).Get<double>(), 0.0);
		const bool normalize = props.Get(Property(propName + ".normalize")(false)).Get<bool>();
		tex = std::make_unique<BlackBodyTexture>(temperature, normalize);
	} else if (texType == "irregulardata") {
		if (!props.IsDefined(propName + ".wavelengths"))
			throw runtime_error("Missing wavelengths property in irregulardata texture: " + propName);
		if (!props.IsDefined(propName + ".data"))
			throw runtime_error("Missing data property in irregulardata texture: " + propName);

		const Property &wl = props.Get(Property(propName + ".wavelengths"));
		const Property &dt = props.Get(Property(propName + ".data"));
		if (wl.GetSize() < 2)
			throw runtime_error("Insufficient data in irregulardata texture: " + propName);
		if (dt.GetSize() != dt.GetSize())
			throw runtime_error("Number of wavelengths doesn't match number of data values in irregulardata texture: " + propName);

		vector<float> waveLengths, data;
		for (u_int i = 0; i < wl.GetSize(); ++i) {
			waveLengths.push_back(wl.Get<double>(i));
			data.push_back(dt.Get<double>(i));
		}

		const float resolution = props.Get(Property(propName + ".resolution")(5.0)).Get<double>();
		const bool emission = props.Get(Property(propName + ".emission")(true)).Get<bool>();
		tex = std::make_unique<IrregularDataTexture>(waveLengths.size(), &waveLengths[0], &data[0], resolution, emission);
	} else if (texType == "lampspectrum") {
		tex = AllocLampSpectrumTex(props, propName);
	} else if (texType == "fresnelabbe") {
		tex = AllocFresnelAbbeTex(props, propName);
	} else if (texType == "fresnelcauchy") {
		tex = AllocFresnelCauchyTex(props, propName);
	} else if (texType == "fresnelcolor") {
		auto& col = GetTexture(props.Get(Property(propName + ".kr")(.5f)));

		tex = std::make_unique<FresnelColorTexture>(col);
	} else if (texType == "fresnelconst") {
		const Spectrum n = GetColor(props.Get(Property(propName + ".n")(1.f, 1.f, 1.f)));
		const Spectrum k = GetColor(props.Get(Property(propName + ".k")(1.f, 1.f, 1.f)));

		tex = std::make_unique<FresnelConstTexture>(n, k);
	} else if (texType == "fresnelluxpop") {
		tex = AllocFresnelLuxPopTex(props, propName);
	} else if (texType == "fresnelpreset") {
		tex = AllocFresnelPresetTex(props, propName);
	} else if (texType == "fresnelsopra") {
		tex = AllocFresnelSopraTex(props, propName);
	} else if (texType == "abs") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture")(1.f)));

		tex = std::make_unique<AbsTexture>(tex1);
	} else if (texType == "clamp") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const float minVal = props.Get(Property(propName + ".min")(0.0)).Get<double>();
		const float maxVal = props.Get(Property(propName + ".max")(0.0)).Get<double>();

		tex = std::make_unique<ClampTexture>(tex1, minVal, maxVal);
	} else if (texType == "colordepth") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".kt")(1.f)));
		const float depth = props.Get(Property(propName + ".depth")(1.00)).Get<double>();

		tex = std::make_unique<ColorDepthTexture>(depth, tex1);
	} else if (texType == "normalmap") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const float scale = Max(0.0, props.Get(Property(propName + ".scale")(1.0)).Get<double>());

		tex = std::make_unique<NormalMapTexture>(tex1, scale);
	} else if (texType == "bilerp") {
		auto& t00 = GetTexture(props.Get(Property(propName + ".texture00")(0.f)));
		auto& t01 = GetTexture(props.Get(Property(propName + ".texture01")(1.f)));
		auto& t10 = GetTexture(props.Get(Property(propName + ".texture10")(0.f)));
		auto& t11 = GetTexture(props.Get(Property(propName + ".texture11")(1.f)));

		tex = std::make_unique<BilerpTexture>(t00, t01, t10, t11);
	} else if (texType == "hsv") {
		auto& t = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		auto& h = GetTexture(props.Get(Property(propName + ".hue")(0.5f)));
		auto& s = GetTexture(props.Get(Property(propName + ".saturation")(1.f)));
		auto& v = GetTexture(props.Get(Property(propName + ".value")(1.f)));

		tex = std::make_unique<HsvTexture>(t, h, s, v);
	} else if (texType == "divide") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<DivideTexture>(tex1, tex2);
	} else if (texType == "remap") {
		auto& value = GetTexture(props.Get(Property(propName + ".value")(0.5f)));
		auto& sourceMin = GetTexture(props.Get(Property(propName + ".sourcemin")(0.f)));
		auto& sourceMax = GetTexture(props.Get(Property(propName + ".sourcemax")(1.f)));
		auto& targetMin = GetTexture(props.Get(Property(propName + ".targetmin")(0.f)));
		auto& targetMax = GetTexture(props.Get(Property(propName + ".targetmax")(1.f)));
		tex = std::make_unique<RemapTexture>(value, sourceMin, sourceMax, targetMin, targetMax);
	} else if (texType == "objectid") {
		tex = std::make_unique<ObjectIDTexture>();
	} else if (texType == "objectidcolor") {
		tex = std::make_unique<ObjectIDColorTexture>();
	} else if (texType == "objectidnormalized") {
		tex = std::make_unique<ObjectIDNormalizedTexture>();
	} else if (texType == "dotproduct") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<DotProductTexture>(tex1, tex2);
	} else if (texType == "power") {
		auto& base = GetTexture(props.Get(Property(propName + ".base")(1.f)));
		auto& exponent = GetTexture(props.Get(Property(propName + ".exponent")(1.f)));
		tex = std::make_unique<PowerTexture>(base, exponent);
	} else if (texType == "lessthan") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<LessThanTexture>(tex1, tex2);
	} else if (texType == "greaterthan") {
		auto& tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = std::make_unique<GreaterThanTexture>(tex1, tex2);
	} else if (texType == "shadingnormal") {
		tex = std::make_unique<ShadingNormalTexture>();
	} else if (texType == "position") {
		tex = std::make_unique<PositionTexture>();
	} else if (texType == "splitfloat3") {
		auto& t = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const int channel = Min(2, Max(0, props.Get(Property(propName + ".channel")(0)).Get<int>()));
		tex = std::make_unique<SplitFloat3Texture>(t, static_cast<u_int>(channel));
	} else if (texType == "makefloat3") {
		auto& t1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& t2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		auto& t3 = GetTexture(props.Get(Property(propName + ".texture3")(1.f)));
		tex = std::make_unique<MakeFloat3Texture>(t1, t2, t3);
    } else if (texType == "rounding") {
        auto& texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
        auto& increment = GetTexture(props.Get(Property(propName + ".increment")(0.5f)));
        tex = std::make_unique<RoundingTexture>(texture, increment);
    } else if (texType == "modulo") {
		auto& texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		auto& modulo = GetTexture(props.Get(Property(propName + ".modulo")(0.5f)));
		tex = std::make_unique<ModuloTexture>(texture, modulo);
	} else if (texType == "brightcontrast") {
		auto& texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		auto& brightnessTex = GetTexture(props.Get(Property(propName + ".brightness")(0.f)));
		auto& contrastTex = GetTexture(props.Get(Property(propName + ".contrast")(0.f)));
		tex = std::make_unique<BrightContrastTexture>(texture, brightnessTex, contrastTex);
	} else if (texType == "triplanar") {
		auto& t1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		auto& t2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		auto& t3 = GetTexture(props.Get(Property(propName + ".texture3")(1.f)));
		const bool enableUVlessBumpMap = props.Get(Property(propName + ".uvlessbumpmap.enable")(true)).Get<bool>();
		tex = std::make_unique<TriplanarTexture>(CreateTextureMapping3D(propName + ".mapping", props),
				t1, t2, t3, enableUVlessBumpMap);
    } else if (texType == "random") {
		auto& texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const u_int seedOffset = props.Get(Property(propName + ".seed")(0u)).Get<u_int>();
		tex = std::make_unique<RandomTexture>(texture, seedOffset);
	} else if (texType == "wireframe") {
		auto& borderTex = GetTexture(props.Get(Property(propName + ".border")(1.f)));
		auto& insideTex = GetTexture(props.Get(Property(propName + ".inside")(0.f)));
		const float width = props.Get(Property(propName + ".width")(0.0)).Get<double>();

		tex = std::make_unique<WireFrameTexture>(width, borderTex, insideTex);
	/*} else if (texType == "bevel") {
		auto bumpTex = props.IsDefined(propName + ".bumptex") ?
			GetTexture(props.Get(Property(propName + ".bumptex")(1.f))) : nullptr;
		
		const float radius = props.Get(Property(propName + ".radius")(.0250)).Get<double>();

		tex = std::make_unique<BevelTexture(bumpTex, radius);*/
	} else if (texType == "distort") {
		auto& texture = GetTexture(props.Get(Property(propName + ".texture")(0.f)));
		auto& offset = GetTexture(props.Get(Property(propName + ".offset")(0.f)));
		const float strength = props.Get(Property(propName + ".strength")(1.0)).Get<double>();

		tex = std::make_unique<DistortTexture>(texture, offset, strength);
	} else if (texType == "bombing") {
		auto& background = GetTexture(props.Get(Property(propName + ".background")(1.f)));
		auto& bullet = GetTexture(props.Get(Property(propName + ".bullet")(1.f)));
		auto& bulletMask = GetTexture(props.Get(Property(propName + ".bullet.mask")(0.f)));

		const float randomScaleFactor = Max(props.Get(Property(propName + ".bullet.randomscale.range")(.250)).Get<double>(), 0.0);
		const bool useRandomRotation = props.Get(Property(propName + ".bullet.randomrotation.enable")(true)).Get<bool>();
		const u_int multiBulletCount = Max(props.Get(Property(propName + ".bullet.count")(1u)).Get<u_int>(), 1u);

		tex = std::make_unique<BombingTexture>(
			CreateTextureMapping2D(propName + ".mapping", props),
			background,
			bullet,
			bulletMask,
			*GetRandomImageMap(),
			randomScaleFactor,
			useRandomRotation,
			multiBulletCount
		);
	} else
		throw runtime_error("Unknown texture type: " + texType);

	tex->SetName(texName);

	return tex;
}

Spectrum Scene::GetColor(const luxrays::Property &prop) {
	if (prop.GetSize() > 1) {
		string colorSpaceName = prop.Get<string>(0);

		if (colorSpaceName == ColorSpaceConfig::ColorSpaceType2String(ColorSpaceConfig::NOP_COLORSPACE)) {
			if (prop.GetSize() == 4) {
				Spectrum c(prop.Get<double>(1), prop.Get<double>(2), prop.Get<double>(3));
				colorSpaceConv.ConvertFrom(ColorSpaceConfig::defaultNopConfig, c);

				return c;
			} else
				throw runtime_error("Wrong number of arguments in the color definition with a color space: " + prop.ToString());
		} else if (colorSpaceName == ColorSpaceConfig::ColorSpaceType2String(ColorSpaceConfig::LUXCORE_COLORSPACE)) {
			if (prop.GetSize() == 5) {
				const float gamma = prop.Get<double>(1);
				Spectrum c(prop.Get<double>(2), prop.Get<double>(3), prop.Get<double>(4));
				colorSpaceConv.ConvertFrom(ColorSpaceConfig(gamma), c);

				return c;
			} else
				throw runtime_error("Wrong number of arguments in the color definition with a color space: " + prop.ToString());
		} else if (colorSpaceName == ColorSpaceConfig::ColorSpaceType2String(ColorSpaceConfig::OPENCOLORIO_COLORSPACE)) {
			if (prop.GetSize() == 6) {
				const string configName = prop.Get<string>(1);
				const string colorSapceName = prop.Get<string>(2);
				Spectrum c(prop.Get<double>(3), prop.Get<double>(4), prop.Get<double>(5));
				colorSpaceConv.ConvertFrom(ColorSpaceConfig(configName, colorSapceName), c);

				return c;
			} else
				throw runtime_error("Wrong number of arguments in the color definition with a color space: " + prop.ToString());
		}

		return prop.Get<Spectrum>();
	} else
		throw runtime_error("Wrong number of arguments in the color definition with a color space: " + prop.ToString());
}

TextureRef Scene::GetTexture(const luxrays::Property &prop) {
	const string &name = prop.GetValuesString();

	if (texDefs.IsTextureDefined(name))
		return texDefs.GetTexture(name);
	else {
		// Check if it is an implicit declaration of a constant texture
		try {
			// Check if the first element is a name space
			if (prop.GetSize() > 1) {
				string colorSpaceName = prop.Get<string>(0);

				if (colorSpaceName == ColorSpaceConfig::ColorSpaceType2String(ColorSpaceConfig::NOP_COLORSPACE)) {
					if (prop.GetSize() == 2) {
						float v = prop.Get<double>(1);				
						colorSpaceConv.ConvertFrom(ColorSpaceConfig::defaultNopConfig, v);
		
						auto tex = std::make_unique<ConstFloatTexture>(v);
						tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture"));
						auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

						moveToTrash(std::move(oldTexPtr));
						return newTexRef;
					} else if (prop.GetSize() == 4) {
						Spectrum c(prop.Get<double>(1), prop.Get<double>(2), prop.Get<double>(3));
						colorSpaceConv.ConvertFrom(ColorSpaceConfig::defaultNopConfig, c);

						auto tex = std::make_unique<ConstFloat3Texture>(c);
						tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture3"));
						auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

						moveToTrash(std::move(oldTexPtr));
						return newTexRef;
					} else
						throw runtime_error("Wrong number of arguments in the implicit definition of a constant texture with a color space: " + prop.ToString());
				} else if (colorSpaceName == ColorSpaceConfig::ColorSpaceType2String(ColorSpaceConfig::LUXCORE_COLORSPACE)) {
					if (prop.GetSize() == 3) {
						const float gamma = prop.Get<double>(1);
						float v = prop.Get<double>(2);
						colorSpaceConv.ConvertFrom(ColorSpaceConfig(gamma), v);
		
						auto tex = std::make_unique<ConstFloatTexture>(v);
						tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture"));
						auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

						moveToTrash(std::move(oldTexPtr));
						return newTexRef;
					} else if (prop.GetSize() == 5) {
						const float gamma = prop.Get<double>(1);
						Spectrum c(prop.Get<double>(2), prop.Get<double>(3), prop.Get<double>(4));
						colorSpaceConv.ConvertFrom(ColorSpaceConfig(gamma), c);

						auto tex= std::make_unique<ConstFloat3Texture>(c);
						tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture3"));
						auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

						moveToTrash(std::move(oldTexPtr));
						return newTexRef;
					} else
						throw runtime_error("Wrong number of arguments in the implicit definition of a constant texture with a color space: " + prop.ToString());
				} else if (colorSpaceName == ColorSpaceConfig::ColorSpaceType2String(ColorSpaceConfig::OPENCOLORIO_COLORSPACE)) {
					if (prop.GetSize() == 4) {
						const string configName = prop.Get<string>(1);
						const string colorSapceName = prop.Get<string>(2);
						float v = prop.Get<double>(3);
						colorSpaceConv.ConvertFrom(ColorSpaceConfig(configName, colorSapceName), v);
		
						auto tex= std::make_unique<ConstFloatTexture>(v);
						tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture"));
						auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

						moveToTrash(std::move(oldTexPtr));
						return newTexRef;
					} else if (prop.GetSize() == 6) {
						const string configName = prop.Get<string>(1);
						const string colorSapceName = prop.Get<string>(2);
						Spectrum c(prop.Get<double>(3), prop.Get<double>(4), prop.Get<double>(5));
						colorSpaceConv.ConvertFrom(ColorSpaceConfig(configName, colorSapceName), c);

						auto tex= std::make_unique<ConstFloat3Texture>(c);
						tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture3"));
						auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

						moveToTrash(std::move(oldTexPtr));
						return newTexRef;
					} else
						throw runtime_error("Wrong number of arguments in the implicit definition of a constant texture with a color space: " + prop.ToString());
				}
			}

			// Check if is a single float or a spectrum
			vector<string> strs;
			boost::split(strs, name, boost::is_any_of("\t "));

			vector<float> floats;
			for(const string &s: strs) {
				if (s.length() != 0) {
					const double f = boost::lexical_cast<double>(s);
					floats.push_back(static_cast<float>(f));
				}
			}
		
			if (floats.size() == 1) {
				auto tex= std::make_unique<ConstFloatTexture>(floats.at(0));
				tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture"));
				auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

				moveToTrash(std::move(oldTexPtr));
				return newTexRef;
			} else if (floats.size() == 3) {
				auto tex= std::make_unique<ConstFloat3Texture>(Spectrum(floats.at(0), floats.at(1), floats.at(2)));
				tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture3"));
				auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(tex));

				moveToTrash(std::move(oldTexPtr));
				return newTexRef;
			} else
				throw runtime_error("Wrong number of arguments in the implicit definition of a constant texture: " +
						ToString(floats.size()));
		} catch (boost::bad_lexical_cast &) {
			throw runtime_error("Syntax error in texture name: " + name);
		}
	}
}

//------------------------------------------------------------------------------

TextureMapping2DUPtr Scene::CreateTextureMapping2D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(Property(prefixName + ".type")("uvmapping2d")).Get<string>();

	if (mapType == "uvmapping2d") {
		const u_int dataIndex = Clamp(props.Get(Property(prefixName + ".uvindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		const float rotation = props.Get(Property(prefixName + ".rotation")(0.0)).Get<double>();
		const UV uvScale = props.Get(Property(prefixName + ".uvscale")(1.f, 1.f)).Get<UV>();
		const UV uvDelta = props.Get(Property(prefixName + ".uvdelta")(0.f, 0.f)).Get<UV>();
		const bool centerrotation = props.Get(Property(prefixName + ".centerrotation")(false)).Get<bool>();

		return std::make_unique<UVMapping2D>(dataIndex, rotation, centerrotation, uvScale.u, uvScale.v, uvDelta.u, uvDelta.v);
	} else if (mapType == "uvrandommapping2d") {
		const u_int dataIndex = Clamp(props.Get(Property(prefixName + ".uvindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		const RandomMappingSeedType seedType = String2RandomMappingSeedType(props.Get(Property(prefixName + ".seed.type")("object_id")).Get<string>());
		const u_int triAOVIndex = props.Get(Property(prefixName + ".triangleaov.index")(0u)).Get<u_int>();
		const u_int objectIDOffset = props.Get(Property(prefixName + ".objectidoffset.value")(0u)).Get<u_int>();

		const Property uvRotationDefaultProp = Property(prefixName + ".rotation")(0.f, 0.f);
		const Property &uvRotationProp = props.Get(uvRotationDefaultProp);
		const float uvRotationMin = uvRotationProp.Get<double>(0);
		const float uvRotationMax = uvRotationProp.Get<double>(1);
		const float uvRotationStep = (uvRotationProp.GetSize() > 2) ? uvRotationProp.Get<double>(2) : 0.f;

		const Property uvScaleDefaultProp = Property(prefixName + ".uvscale")(1.f, 1.f, 1.f, 1.f);
		const Property &uvScaleProp = props.Get(uvScaleDefaultProp);
		const float uScaleMin = uvScaleProp.Get<double>(0);
		const float uScaleMax = uvScaleProp.Get<double>(1);
		const float vScaleMin = uvScaleProp.Get<double>(2);
		const float vScaleMax = uvScaleProp.Get<double>(3);
		
		const bool uniformScale = props.Get(Property(prefixName + ".uvscale.uniform")(false)).Get<bool>();

		const Property uvDeltaDefaultProp = Property(prefixName + ".uvdelta")(0.f, 0.f, 0.f, 0.f);
		const Property &uvDeltaProp = props.Get(uvDeltaDefaultProp);
		const float uDeltaMin = uvDeltaProp.Get<double>(0);
		const float uDeltaMax = uvDeltaProp.Get<double>(1);
		const float vDeltaMin = uvDeltaProp.Get<double>(2);
		const float vDeltaMax = uvDeltaProp.Get<double>(3);

		return std::make_unique<UVRandomMapping2D>(dataIndex, seedType, triAOVIndex, objectIDOffset,
				uvRotationMin, uvRotationMax, uvRotationStep,
				uScaleMin, uScaleMax, vScaleMin, vScaleMax,
				uDeltaMin, uDeltaMax, vDeltaMin, vDeltaMax,
				uniformScale);
	} else
		throw runtime_error("Unknown 2D texture coordinate mapping type: " + mapType);
}

TextureMapping3DUPtr Scene::CreateTextureMapping3D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(Property(prefixName + ".type")("uvmapping3d")).Get<string>();

	if (mapType == "uvmapping3d") {
		const u_int dataIndex = Clamp(props.Get(Property(prefixName + ".uvindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return std::make_unique<UVMapping3D>(dataIndex, trans);
	} else if (mapType == "globalmapping3d") {
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return std::make_unique<GlobalMapping3D>(trans);
	} else if (mapType == "localmapping3d") {
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return std::make_unique<LocalMapping3D>(trans);
	} else if (mapType == "localrandommapping3d") {
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		const RandomMappingSeedType seedType = String2RandomMappingSeedType(props.Get(Property(prefixName + ".seed.type")("object_id")).Get<string>());
		const u_int triAOVIndex = props.Get(Property(prefixName + ".triangleaov.index")(0u)).Get<u_int>();
		const u_int objectIDOffset = props.Get(Property(prefixName + ".objectidoffset.value")(0u)).Get<u_int>();

		const Property xRotationDefaultProp = Property(prefixName + ".xrotation")(0.f, 0.f, 0.f);
		const Property yRotationDefaultProp = Property(prefixName + ".yrotation")(0.f, 0.f, 0.f);
		const Property zRotationDefaultProp = Property(prefixName + ".zrotation")(0.f, 0.f, 0.f);

		const Property &xRotationProp = props.Get(xRotationDefaultProp);
		const float xRotationMin = xRotationProp.Get<double>(0);
		const float xRotationMax = xRotationProp.Get<double>(1);
		const float xRotationStep = (xRotationProp.GetSize() > 2) ? xRotationProp.Get<double>(2) : 0.f;
		const Property &yRotationProp = props.Get(yRotationDefaultProp);
		const float yRotationMin = yRotationProp.Get<double>(0);
		const float yRotationMax = yRotationProp.Get<double>(1);
		const float yRotationStep = (yRotationProp.GetSize() > 2) ? yRotationProp.Get<double>(2) : 0.f;
		const Property &zRotationProp = props.Get(zRotationDefaultProp);
		const float zRotationMin = zRotationProp.Get<double>(0);
		const float zRotationMax = zRotationProp.Get<double>(1);
		const float zRotationStep = (zRotationProp.GetSize() > 2) ? zRotationProp.Get<double>(2) : 0.f;

		const Property xScaleDefaultProp = Property(prefixName + ".xscale")(1.f, 1.f);
		const Property yScaleDefaultProp = Property(prefixName + ".yscale")(1.f, 1.f);
		const Property zScaleDefaultProp = Property(prefixName + ".zscale")(1.f, 1.f);

		const Property &xScaleProp = props.Get(xScaleDefaultProp);
		const float xScaleMin = xScaleProp.Get<double>(0);
		const float xScaleMax = xScaleProp.Get<double>(1);
		const Property &yScaleProp = props.Get(yScaleDefaultProp);
		const float yScaleMin = yScaleProp.Get<double>(0);
		const float yScaleMax = yScaleProp.Get<double>(1);
		const Property &zScaleProp = props.Get(zScaleDefaultProp);
		const float zScaleMin = zScaleProp.Get<double>(0);
		const float zScaleMax = zScaleProp.Get<double>(1);
		
		const bool uniformScale = props.Get(Property(prefixName + ".xyzscale.uniform")(false)).Get<bool>();

		const Property xTranslateDefaultProp = Property(prefixName + ".xtranslate")(0.f, 0.f);
		const Property yTranslateDefaultProp = Property(prefixName + ".ytranslate")(0.f, 0.f);
		const Property zTranslateDefaultProp = Property(prefixName + ".ztranslate")(0.f, 0.f);
		
		const Property &xTranslateProp = props.Get(xTranslateDefaultProp);
		const float xTranslateMin = xTranslateProp.Get<double>(0);
		const float xTranslateMax = xTranslateProp.Get<double>(1);
		const Property &yTranslateProp = props.Get(yTranslateDefaultProp);
		const float yTranslateMin = yTranslateProp.Get<double>(0);
		const float yTranslateMax = yTranslateProp.Get<double>(1);
		const Property &zTranslateProp = props.Get(zTranslateDefaultProp);
		const float zTranslateMin = zTranslateProp.Get<double>(0);
		const float zTranslateMax = zTranslateProp.Get<double>(1);

		return std::make_unique<LocalRandomMapping3D>(trans, seedType, triAOVIndex, objectIDOffset,
				xRotationMin, xRotationMax, xRotationStep,
				yRotationMin, yRotationMax, yRotationStep,
				zRotationMin, zRotationMax, zRotationStep,
				xScaleMin, xScaleMax,
				yScaleMin, yScaleMax,
				zScaleMin, zScaleMax,
				xTranslateMin, xTranslateMax,
				yTranslateMin, yTranslateMax,
				zTranslateMin, zTranslateMax,
				uniformScale);
	} else
		throw runtime_error("Unknown 3D texture coordinate mapping type: " + mapType);
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

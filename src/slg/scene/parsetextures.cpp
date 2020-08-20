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

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "slg/scene/scene.h"
#include "slg/utils/filenameresolver.h"

#include "slg/textures/band.h"
#include "slg/textures/bevel.h"
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
		// There are not texture definitions
		return;
	}

	BOOST_FOREACH(const string &key, texKeys) {
		// Extract the texture name
		const string texName = Property::ExtractField(key, 2);
		if (texName == "")
			throw runtime_error("Syntax error in texture definition: " + texName);

		if (texDefs.IsTextureDefined(texName)) {
			SDL_LOG("Texture re-definition: " << texName);
		} else {
			SDL_LOG("Texture definition: " << texName);
		}

		Texture *tex = CreateTexture(texName, props);
		// Density grid data are stored with image maps
		if ((tex->GetType() == IMAGEMAP) || (tex->GetType() == DENSITYGRID_TEX))
			editActions.AddAction(IMAGEMAPS_EDIT);

		if (texDefs.IsTextureDefined(texName)) {
			// A replacement for an existing texture
			const Texture *oldTex = texDefs.GetTexture(texName);

			// FresnelTexture can be replaced only with other FresnelTexture
			if (dynamic_cast<const FresnelTexture *>(oldTex) && !dynamic_cast<const FresnelTexture *>(tex))
				throw runtime_error("You can not replace a fresnel texture with the texture: " + texName);

			texDefs.DefineTexture(tex);
			matDefs.UpdateTextureReferences(oldTex, tex);
		} else {
			// Only a new texture
			texDefs.DefineTexture(tex);
		}
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

Texture *Scene::CreateTexture(const string &texName, const Properties &props) {
	const string propName = "scene.textures." + texName;
	const string texType = props.Get(Property(propName + ".type")("imagemap")).Get<string>();

	Texture *tex = NULL;
	if (texType == "imagemap") {
		const string name = props.Get(Property(propName + ".file")("image.png")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		const float gain = props.Get(Property(propName + ".gain")(1.f)).Get<float>();

		const ImageMapStorage::ChannelSelectionType selectionType = ImageMapStorage::String2ChannelSelectionType(
				props.Get(Property(propName + ".channel")("default")).Get<string>());

		const ImageMapStorage::StorageType storageType = ImageMapStorage::String2StorageType(
			props.Get(Property(propName + ".storage")("auto")).Get<string>());
		
		const ImageMapStorage::WrapType wrapType = ImageMapStorage::String2WrapType(
			props.Get(Property(propName + ".wrap")("repeat")).Get<string>());

		ImageMap *im = imgMapCache.GetImageMap(name, gamma, selectionType, storageType, wrapType);
		tex = new ImageMapTexture(im, CreateTextureMapping2D(propName + ".mapping", props), gain);
	} else if (texType == "constfloat1") {
		const float v = props.Get(Property(propName + ".value")(1.f)).Get<float>();
		tex = new ConstFloatTexture(v);
	} else if (texType == "constfloat3") {
		const Spectrum v = props.Get(Property(propName + ".value")(1.f, 1.f, 1.f)).Get<Spectrum>();
		tex = new ConstFloat3Texture(v);
	} else if (texType == "scale") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new ScaleTexture(tex1, tex2);
	} else if (texType == "fresnelapproxn") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture")(.5f, .5f, .5f)));
		tex = new FresnelApproxNTexture(tex1);
	} else if (texType == "fresnelapproxk") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture")(.5f, .5f, .5f)));
		tex = new FresnelApproxKTexture(tex1);
	} else if (texType == "checkerboard2d") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(0.f)));

		tex = new CheckerBoard2DTexture(CreateTextureMapping2D(propName + ".mapping", props), tex1, tex2);
	} else if (texType == "checkerboard3d") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(0.f)));

		tex = new CheckerBoard3DTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2);
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

		ImageMap *imgMap;
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
		imgMapCache.DefineImageMap(imgMap);

		tex = new DensityGridTexture(CreateTextureMapping3D(propName + ".mapping", props),
				nx, ny, nz, imgMap);
	} else if (texType == "mix") {
		const Texture *amtTex = GetTexture(props.Get(Property(propName + ".amount")(.5f)));
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(0.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));

		tex = new MixTexture(amtTex, tex1, tex2);
	} else if (texType == "fbm") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5f)).Get<float>();

		tex = new FBMTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "marble") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5f)).Get<float>();
		const float scale = props.Get(Property(propName + ".scale")(1.f)).Get<float>();
		const float variation = props.Get(Property(propName + ".variation")(.2f)).Get<float>();

		tex = new MarbleTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega, scale, variation);
	} else if (texType == "blender_blend") {
		const string progressiontype = props.Get(Property(propName + ".progressiontype")("linear")).Get<string>();
		const string direct = props.Get(Property(propName + ".direction")("horizontal")).Get<string>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderBlendTexture(CreateTextureMapping3D(propName + ".mapping", props),
				progressiontype, (direct=="vertical"), bright, contrast);
	} else if (texType == "blender_clouds") {
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderCloudsTexture(CreateTextureMapping3D(propName + ".mapping", props),
				noisebasis, noisesize, noisedepth,(hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_distortednoise") {
		const string noisedistortion = props.Get(Property(propName + ".noise_distortion")("blender_original")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float distortion = props.Get(Property(propName + ".distortion")(1.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderDistortedNoiseTexture(CreateTextureMapping3D(propName + ".mapping", props),
				noisedistortion, noisebasis, distortion, noisesize, bright, contrast);
	} else if (texType == "blender_magic") {
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderMagicTexture(CreateTextureMapping3D(propName + ".mapping", props),
				noisedepth, turbulence, bright, contrast);
	} else if (texType == "blender_marble") {
		const string marbletype = props.Get(Property(propName + ".marbletype")("soft")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderMarbleTexture(CreateTextureMapping3D(propName + ".mapping", props),
				marbletype, noisebasis, noisebasis2, noisesize, turbulence, noisedepth, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_musgrave") {
		const string musgravetype = props.Get(Property(propName + ".musgravetype")("multifractal")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const float dimension = props.Get(Property(propName + ".dimension")(1.f)).Get<float>();
		const float intensity = props.Get(Property(propName + ".intensity")(1.f)).Get<float>();
		const float lacunarity = props.Get(Property(propName + ".lacunarity")(1.f)).Get<float>();
		const float offset = props.Get(Property(propName + ".offset")(1.f)).Get<float>();
		const float gain = props.Get(Property(propName + ".gain")(1.f)).Get<float>();
		const float octaves = props.Get(Property(propName + ".octaves")(2.f)).Get<float>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderMusgraveTexture(CreateTextureMapping3D(propName + ".mapping", props),
				musgravetype, noisebasis, dimension, intensity, lacunarity, offset, gain, octaves, noisesize, bright, contrast);
	} else if (texType == "blender_noise") {
		const int noisedepth = props.Get(Property(propName + ".noisedepth")(2)).Get<int>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderNoiseTexture(noisedepth, bright, contrast);
	} else if (texType == "blender_stucci") {
		const string stuccitype = props.Get(Property(propName + ".stuccitype")("plastic")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderStucciTexture(CreateTextureMapping3D(propName + ".mapping", props),
				stuccitype, noisebasis, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_wood") {
		const string woodtype = props.Get(Property(propName + ".woodtype")("bands")).Get<string>();
		const string noisebasis = props.Get(Property(propName + ".noisebasis")("blender_original")).Get<string>();
		const string noisebasis2 = props.Get(Property(propName + ".noisebasis2")("sin")).Get<string>();
		const string hard = props.Get(Property(propName + ".noisetype")("soft_noise")).Get<string>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(5.f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderWoodTexture(CreateTextureMapping3D(propName + ".mapping", props),
				woodtype, noisebasis2, noisebasis, noisesize, turbulence, (hard=="hard_noise"), bright, contrast);
	} else if (texType == "blender_voronoi") {
		const float intensity = props.Get(Property(propName + ".intensity")(1.f)).Get<float>();
		const float exponent = props.Get(Property(propName + ".exponent")(2.f)).Get<float>();
		const string distmetric = props.Get(Property(propName + ".distmetric")("actual_distance")).Get<string>();
		const float fw1 = props.Get(Property(propName + ".w1")(1.f)).Get<float>();
		const float fw2 = props.Get(Property(propName + ".w2")(0.f)).Get<float>();
		const float fw3 = props.Get(Property(propName + ".w3")(0.f)).Get<float>();
		const float fw4 = props.Get(Property(propName + ".w4")(0.f)).Get<float>();
		const float noisesize = props.Get(Property(propName + ".noisesize")(.25f)).Get<float>();
		const float bright = props.Get(Property(propName + ".bright")(1.f)).Get<float>();
		const float contrast = props.Get(Property(propName + ".contrast")(1.f)).Get<float>();

		tex = new BlenderVoronoiTexture(CreateTextureMapping3D(propName + ".mapping", props), intensity, exponent, fw1, fw2, fw3, fw4, distmetric, noisesize, bright, contrast);
	} else if (texType == "dots") {
		const Texture *insideTex = GetTexture(props.Get(Property(propName + ".inside")(1.f)));
		const Texture *outsideTex = GetTexture(props.Get(Property(propName + ".outside")(0.f)));

		tex = new DotsTexture(CreateTextureMapping2D(propName + ".mapping", props), insideTex, outsideTex);
	} else if (texType == "brick") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".bricktex")(1.f, 1.f, 1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".mortartex")(.2f, .2f, .2f)));
		const Texture *tex3 = GetTexture(props.Get(Property(propName + ".brickmodtex")(1.f, 1.f, 1.f)));

		const string brickbond = props.Get(Property(propName + ".brickbond")("running")).Get<string>();
		const float brickwidth = props.Get(Property(propName + ".brickwidth")(.3f)).Get<float>();
		const float brickheight = props.Get(Property(propName + ".brickheight")(.1f)).Get<float>();
		const float brickdepth = props.Get(Property(propName + ".brickdepth")(.15f)).Get<float>();
		const float mortarsize = props.Get(Property(propName + ".mortarsize")(.01f)).Get<float>();
		const float brickrun = props.Get(Property(propName + ".brickrun")(.75f)).Get<float>();
		const float brickbevel = props.Get(Property(propName + ".brickbevel")(0.f)).Get<float>();

		tex = new BrickTexture(CreateTextureMapping3D(propName + ".mapping", props), tex1, tex2, tex3,
				brickwidth, brickheight, brickdepth, mortarsize, brickrun, brickbevel, brickbond);
	} else if (texType == "add") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new AddTexture(tex1, tex2);
	} else if (texType == "subtract") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new SubtractTexture(tex1, tex2);
	} else if (texType == "windy") {
		tex = new WindyTexture(CreateTextureMapping3D(propName + ".mapping", props));
	} else if (texType == "wrinkled") {
		const int octaves = props.Get(Property(propName + ".octaves")(8)).Get<int>();
		const float omega = props.Get(Property(propName + ".roughness")(.5f)).Get<float>();

		tex = new WrinkledTexture(CreateTextureMapping3D(propName + ".mapping", props), octaves, omega);
	} else if (texType == "uv") {
		tex = new UVTexture(CreateTextureMapping2D(propName + ".mapping", props));
	} else if (texType == "band") {
		const string interpTypeString = props.Get(Property(propName + ".interpolation")("linear")).Get<string>();
		const BandTexture::InterpolationType interpType = BandTexture::String2InterpolationType(interpTypeString);

		const Texture *amtTex = GetTexture(props.Get(Property(propName + ".amount")(.5f)));

		vector<float> offsets;
		vector<Spectrum> values;
		for (u_int i = 0; props.IsDefined(propName + ".offset" + ToString(i)); ++i) {
			const float offset = props.Get(Property(propName + ".offset" + ToString(i))(0.f)).Get<float>();
			const Spectrum value = props.Get(Property(propName + ".value" + ToString(i))(1.f, 1.f, 1.f)).Get<Spectrum>();

			offsets.push_back(offset);
			values.push_back(value);
		}
		if (offsets.size() == 0)
			throw runtime_error("Empty Band texture: " + texName);

		tex = new BandTexture(interpType, amtTex, offsets, values);
	} else if (texType == "hitpointcolor") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = new HitPointColorTexture(dataIndex);
	} else if (texType == "hitpointalpha") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = new HitPointAlphaTexture(dataIndex);
	} else if (texType == "hitpointgrey") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		const int channel = props.Get(Property(propName + ".channel")(-1)).Get<int>();

		tex = new HitPointGreyTexture(dataIndex,
				((channel != 0) && (channel != 1) && (channel != 2)) ?
					numeric_limits<u_int>::max() : static_cast<u_int>(channel));
	} else if (texType == "hitpointvertexaov") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = new HitPointVertexAOVTexture(dataIndex);
	} else if (texType == "hitpointtriangleaov") {
		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		tex = new HitPointTriangleAOVTexture(dataIndex);
	} else if (texType == "cloud") {
		const float radius = props.Get(Property(propName + ".radius")(.5f)).Get<float>();
		const float noisescale = props.Get(Property(propName + ".noisescale")(.5f)).Get<float>();
		const float turbulence = props.Get(Property(propName + ".turbulence")(0.01f)).Get<float>();
		const float sharpness = props.Get(Property(propName + ".sharpness")(6.0f)).Get<float>();
		const float noiseoffset = props.Get(Property(propName + ".noiseoffset")(.0f)).Get<float>();
		const int spheres = props.Get(Property(propName + ".spheres")(0)).Get<int>();
		const int octaves = props.Get(Property(propName + ".octaves")(1)).Get<int>();
		const float omega = props.Get(Property(propName + ".omega")(.5f)).Get<float>();
		const float variability = props.Get(Property(propName + ".variability")(.9f)).Get<float>();
		const float baseflatness = props.Get(Property(propName + ".baseflatness")(.8f)).Get<float>();
		const float spheresize = props.Get(Property(propName + ".spheresize")(.15f)).Get<float>();

		tex = new CloudTexture(CreateTextureMapping3D(propName + ".mapping", props), radius, noisescale, turbulence,
								sharpness, noiseoffset, spheres, octaves, omega, variability, baseflatness, spheresize);
	} else if (texType == "blackbody") {
		const float temperature = props.Get(Property(propName + ".temperature")(6500.f)).Get<float>();
		const bool normalize = props.Get(Property(propName + ".normalize")(false)).Get<bool>();
		tex = new BlackBodyTexture(temperature, normalize);
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
			waveLengths.push_back(wl.Get<float>(i));
			data.push_back(dt.Get<float>(i));
		}

		const float resolution = props.Get(Property(propName + ".resolution")(5.f)).Get<float>();
		const bool emission = props.Get(Property(propName + ".emission")(true)).Get<bool>();
		tex = new IrregularDataTexture(waveLengths.size(), &waveLengths[0], &data[0], resolution, emission);
	} else if (texType == "lampspectrum") {
		tex = AllocLampSpectrumTex(props, propName);
	} else if (texType == "fresnelabbe") {
		tex = AllocFresnelAbbeTex(props, propName);
	} else if (texType == "fresnelcauchy") {
		tex = AllocFresnelCauchyTex(props, propName);
	} else if (texType == "fresnelcolor") {
		const Texture *col = GetTexture(props.Get(Property(propName + ".kr")(.5f)));

		tex = new FresnelColorTexture(col);
	} else if (texType == "fresnelconst") {
		const Spectrum n = props.Get(Property(propName + ".n")(1.f, 1.f, 1.f)).Get<Spectrum>();
		const Spectrum k = props.Get(Property(propName + ".k")(1.f, 1.f, 1.f)).Get<Spectrum>();

		tex = new FresnelConstTexture(n, k);
	} else if (texType == "fresnelluxpop") {
		tex = AllocFresnelLuxPopTex(props, propName);
	} else if (texType == "fresnelpreset") {
		tex = AllocFresnelPresetTex(props, propName);
	} else if (texType == "fresnelsopra") {
		tex = AllocFresnelSopraTex(props, propName);
	} else if (texType == "abs") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture")(1.f)));

		tex = new AbsTexture(tex1);
	} else if (texType == "clamp") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const float minVal = props.Get(Property(propName + ".min")(0.f)).Get<float>();
		const float maxVal = props.Get(Property(propName + ".max")(0.f)).Get<float>();

		tex = new ClampTexture(tex1, minVal, maxVal);
	} else if (texType == "colordepth") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".kt")(1.f)));
		const float depth = props.Get(Property(propName + ".depth")(1.0f)).Get<float>();

		tex = new ColorDepthTexture(depth, tex1);
	} else if (texType == "normalmap") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const float scale = Max(0.f, props.Get(Property(propName + ".scale")(1.f)).Get<float>());

		tex = new NormalMapTexture(tex1, scale);
	} else if (texType == "bilerp") {
		const Texture *t00 = GetTexture(props.Get(Property(propName + ".texture00")(0.f)));
		const Texture *t01 = GetTexture(props.Get(Property(propName + ".texture01")(1.f)));
		const Texture *t10 = GetTexture(props.Get(Property(propName + ".texture10")(0.f)));
		const Texture *t11 = GetTexture(props.Get(Property(propName + ".texture11")(1.f)));

		tex = new BilerpTexture(t00, t01, t10, t11);
	} else if (texType == "hsv") {
		const Texture *t = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const Texture *h = GetTexture(props.Get(Property(propName + ".hue")(0.5f)));
		const Texture *s = GetTexture(props.Get(Property(propName + ".saturation")(1.f)));
		const Texture *v = GetTexture(props.Get(Property(propName + ".value")(1.f)));

		tex = new HsvTexture(t, h, s, v);
	} else if (texType == "divide") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new DivideTexture(tex1, tex2);
	} else if (texType == "remap") {
		const Texture *value = GetTexture(props.Get(Property(propName + ".value")(0.5f)));
		const Texture *sourceMin = GetTexture(props.Get(Property(propName + ".sourcemin")(0.f)));
		const Texture *sourceMax = GetTexture(props.Get(Property(propName + ".sourcemax")(1.f)));
		const Texture *targetMin = GetTexture(props.Get(Property(propName + ".targetmin")(0.f)));
		const Texture *targetMax = GetTexture(props.Get(Property(propName + ".targetmax")(1.f)));
		tex = new RemapTexture(value, sourceMin, sourceMax, targetMin, targetMax);
	} else if (texType == "objectid") {
		tex = new ObjectIDTexture();
	} else if (texType == "objectidcolor") {
		tex = new ObjectIDColorTexture();
	} else if (texType == "objectidnormalized") {
		tex = new ObjectIDNormalizedTexture();
	} else if (texType == "dotproduct") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new DotProductTexture(tex1, tex2);
	} else if (texType == "power") {
		const Texture *base = GetTexture(props.Get(Property(propName + ".base")(1.f)));
		const Texture *exponent = GetTexture(props.Get(Property(propName + ".exponent")(1.f)));
		tex = new PowerTexture(base, exponent);
	} else if (texType == "lessthan") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new LessThanTexture(tex1, tex2);
	} else if (texType == "greaterthan") {
		const Texture *tex1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *tex2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		tex = new GreaterThanTexture(tex1, tex2);
	} else if (texType == "shadingnormal") {
		tex = new ShadingNormalTexture();
	} else if (texType == "position") {
		tex = new PositionTexture();
	} else if (texType == "splitfloat3") {
		const Texture *t = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const int channel = Min(2, Max(0, props.Get(Property(propName + ".channel")(0)).Get<int>()));
		tex = new SplitFloat3Texture(t, static_cast<u_int>(channel));
	} else if (texType == "makefloat3") {
		const Texture *t1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *t2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		const Texture *t3 = GetTexture(props.Get(Property(propName + ".texture3")(1.f)));
		tex = new MakeFloat3Texture(t1, t2, t3);
    } else if (texType == "rounding") {
        const Texture *texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
        const Texture *increment = GetTexture(props.Get(Property(propName + ".increment")(0.5f)));
        tex = new RoundingTexture(texture, increment);
    } else if (texType == "modulo") {
		const Texture *texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const Texture *modulo = GetTexture(props.Get(Property(propName + ".modulo")(0.5f)));
		tex = new ModuloTexture(texture, modulo);
	} else if (texType == "brightcontrast") {
		const Texture *texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const Texture *brightnessTex = GetTexture(props.Get(Property(propName + ".brightness")(0.f)));
		const Texture *contrastTex = GetTexture(props.Get(Property(propName + ".contrast")(0.f)));
		tex = new BrightContrastTexture(texture, brightnessTex, contrastTex);
	} else if (texType == "triplanar") {
		const Texture *t1 = GetTexture(props.Get(Property(propName + ".texture1")(1.f)));
		const Texture *t2 = GetTexture(props.Get(Property(propName + ".texture2")(1.f)));
		const Texture *t3 = GetTexture(props.Get(Property(propName + ".texture3")(1.f)));
		const bool enableUVlessBumpMap = props.Get(Property(propName + ".uvlessbumpmap.enable")(true)).Get<bool>();
		tex = new TriplanarTexture(CreateTextureMapping3D(propName + ".mapping", props),
				t1, t2, t3, enableUVlessBumpMap);
    } else if (texType == "random") {
		const Texture *texture = GetTexture(props.Get(Property(propName + ".texture")(1.f)));
		const u_int seedOffset = props.Get(Property(propName + ".seed")(0u)).Get<u_int>();
		tex = new RandomTexture(texture, seedOffset);
	} else if (texType == "wireframe") {
		const Texture *borderTex = GetTexture(props.Get(Property(propName + ".border")(1.f)));
		const Texture *insideTex = GetTexture(props.Get(Property(propName + ".inside")(0.f)));
		const float width = props.Get(Property(propName + ".width")(0.f)).Get<float>();

		tex = new WireFrameTexture(width, borderTex, insideTex);
	/*} else if (texType == "bevel") {
		const Texture *bumpTex = props.IsDefined(propName + ".bumptex") ?
			GetTexture(props.Get(Property(propName + ".bumptex")(1.f))) : nullptr;
		
		const float radius = props.Get(Property(propName + ".radius")(.025f)).Get<float>();

		tex = new BevelTexture(bumpTex, radius);*/
	} else if (texType == "distort") {
		const Texture *texture = GetTexture(props.Get(Property(propName + ".texture")(0.f)));
		const Texture *offset = GetTexture(props.Get(Property(propName + ".offset")(0.f)));
		const float strength = props.Get(Property(propName + ".strength")(1.f)).Get<float>();

		tex = new DistortTexture(texture, offset, strength);
	} else
		throw runtime_error("Unknown texture type: " + texType);

	tex->SetName(texName);

	return tex;
}

const Texture *Scene::GetTexture(const luxrays::Property &prop) {
	const string &name = prop.GetValuesString();

	if (texDefs.IsTextureDefined(name))
		return texDefs.GetTexture(name);
	else {
		// Check if it is an implicit declaration of a constant texture
		try {
			vector<string> strs;
			boost::split(strs, name, boost::is_any_of("\t "));

			vector<float> floats;
			BOOST_FOREACH(const string &s, strs) {
				if (s.length() != 0) {
					const double f = boost::lexical_cast<double>(s);
					floats.push_back(static_cast<float>(f));
				}
			}

			if (floats.size() == 1) {
				ConstFloatTexture *tex = new ConstFloatTexture(floats.at(0));
				tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture"));
				texDefs.DefineTexture(tex);

				return tex;
			} else if (floats.size() == 3) {
				ConstFloat3Texture *tex = new ConstFloat3Texture(Spectrum(floats.at(0), floats.at(1), floats.at(2)));
				tex->SetName(NamedObject::GetUniqueName("Implicit-ConstFloatTexture3"));
				texDefs.DefineTexture(tex);

				return tex;
			} else
				throw runtime_error("Wrong number of arguments in the implicit definition of a constant texture: " +
						boost::lexical_cast<string>(floats.size()));
		} catch (boost::bad_lexical_cast &) {
			throw runtime_error("Syntax error in texture name: " + name);
		}
	}
}

//------------------------------------------------------------------------------

TextureMapping2D *Scene::CreateTextureMapping2D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(Property(prefixName + ".type")("uvmapping2d")).Get<string>();

	if (mapType == "uvmapping2d") {
		const float rotation = props.Get(Property(prefixName + ".rotation")(0.f)).Get<float>();
		const u_int dataIndex = Clamp(props.Get(Property(prefixName + ".uvindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		const UV uvScale = props.Get(Property(prefixName + ".uvscale")(1.f, 1.f)).Get<UV>();
		const UV uvDelta = props.Get(Property(prefixName + ".uvdelta")(0.f, 0.f)).Get<UV>();

		return new UVMapping2D(dataIndex, rotation, uvScale.u, uvScale.v, uvDelta.u, uvDelta.v);
	} else
		throw runtime_error("Unknown 2D texture coordinate mapping type: " + mapType);
}

TextureMapping3D *Scene::CreateTextureMapping3D(const string &prefixName, const Properties &props) {
	const string mapType = props.Get(Property(prefixName + ".type")("uvmapping3d")).Get<string>();

	if (mapType == "uvmapping3d") {
		const u_int dataIndex = Clamp(props.Get(Property(prefixName + ".uvindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new UVMapping3D(dataIndex, trans);
	} else if (mapType == "globalmapping3d") {
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new GlobalMapping3D(trans);
	} else if (mapType == "localmapping3d") {
		const Matrix4x4 mat = props.Get(Property(prefixName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform trans(mat);

		return new LocalMapping3D(trans);
	} else
		throw runtime_error("Unknown 3D texture coordinate mapping type: " + mapType);
}

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

#include "slg/textures/band.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

float BandTexture::Y() const {
	switch (interpType) {
		case NONE: {
			float ret = offsets[0] * values[0].Y();
			for (u_int i = 0; i < offsets.size() - 1; ++i)
				ret += (offsets[i + 1] - offsets[i]) * values[i].Y();
			return ret;
		}
		case CUBIC: // Just an approximation
		case LINEAR: {
			float ret = offsets[0] * values[0].Y();
			for (u_int i = 0; i < offsets.size() - 1; ++i)
				ret += .5f * (offsets[i + 1] - offsets[i]) *
					(values[i + 1].Y() + values[i].Y());
			return ret;
		}
		default:
			return 0.f;
	}
}

float BandTexture::Filter() const {
	switch (interpType) {
		case NONE: {
			float ret = offsets[0] * values[0].Filter();
			for (u_int i = 0; i < offsets.size() - 1; ++i)
				ret += (offsets[i + 1] - offsets[i]) * values[i].Filter();
			return ret;
		}
		case CUBIC: // Just an approximation
		case LINEAR: {
			float ret = offsets[0] * values[0].Filter();
			for (u_int i = 0; i < offsets.size() - 1; ++i)
				ret += .5f * (offsets[i + 1] - offsets[i]) *
					(values[i + 1].Y() + values[i].Filter());
			return ret;
		}
		default:
			return 0.f;
	}
}

float BandTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum BandTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float a = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);

	if (a < offsets.front())
		return values.front();
	if (a >= offsets.back())
		return values.back();
	// upper_bound is not available on OpenCL
	//const u_int p = upper_bound(offsets.begin(), offsets.end(), a) - offsets.begin();
	int p = 0;
	for (; p < (int)offsets.size(); ++p) {
		if (a < offsets[p])
			break;
	}

	const float factor = (a - offsets[p - 1]) / (offsets[p] - offsets[p - 1]);

	switch (interpType) {
		case NONE:
			return values[p - 1];
		case LINEAR:
			return Lerp(factor,	values[p - 1], values[p]);
		case CUBIC:
			return Cerp(factor,	values[Max<int>(p - 2, 0)], values[p - 1], values[p], values[Min<int>(p + 1, values.size() - 1)]);
		default:
			return Spectrum();
	}
}

Properties BandTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("band"));
	props.Set(Property("scene.textures." + name + ".interpolation")(InterpolationType2String(interpType)));
	props.Set(Property("scene.textures." + name + ".amount")(amount->GetSDLValue()));

	for (u_int i = 0; i < offsets.size(); ++i) {
		props.Set(Property("scene.textures." + name + ".offset" + ToString(i))(offsets[i]));
		props.Set(Property("scene.textures." + name + ".value" + ToString(i))(values[i]));
	}

	return props;
}

BandTexture::InterpolationType BandTexture::String2InterpolationType(const std::string &type) {
	if (type == "none")
		return NONE;
	else if (type == "linear")
		return LINEAR;
	else if (type == "cubic")
		return CUBIC;
	else
		throw runtime_error("Unknown BandTexture interpolation type: " + type);
}

string BandTexture::InterpolationType2String(const BandTexture::InterpolationType type) {
	switch (type) {
		case NONE:
			return "none";
			break;
		case LINEAR:
			return "linear";
			break;
		case CUBIC:
			return "cubic";
			break;
		default:
			throw runtime_error("Unknown BandTexture interpolation type: " + ToString(type));
	}
}

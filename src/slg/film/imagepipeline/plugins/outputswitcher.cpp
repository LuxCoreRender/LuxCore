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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/outputswitcher.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// OutputSwitcher plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::OutputSwitcherPlugin)

ImagePipelinePlugin *OutputSwitcherPlugin::Copy() const {
	return new OutputSwitcherPlugin(type, index);
}

void OutputSwitcherPlugin::Apply(Film &film, const u_int index) {
	// Copy the data from another Film output channel

	// Do nothing if the Film is missing this particular channel
	if (!film.HasChannel(type))
		return;

	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	const u_int pixelCount = film.GetWidth() * film.GetHeight();
	switch (type) {
		case Film::RADIANCE_PER_PIXEL_NORMALIZED: {
			if (index >= film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v[3];
					film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[index]->GetWeightedPixel(i, v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::RADIANCE_PER_SCREEN_NORMALIZED: {
			if (index >= film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size())
				return;

			// Normalize factor
			const float factor = film.GetTotalSampleCount() / (film.GetHeight() * film.GetWidth());

			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v[3] = { 0.f, 0.f, 0.f};
					film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs[index]->AccumulateWeightedPixel(i, v);

					// Normalize the value
					v[0] *= factor;
					v[1] *= factor;
					v[2] *= factor;

					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::ALPHA: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float a;
					film.channel_ALPHA->GetWeightedPixel(i, &a);
					pixels[i] = Spectrum(a);
				}
			}
			break;
		}
		case Film::DEPTH: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float d;
					film.channel_DEPTH->GetWeightedPixel(i, &d);
					pixels[i] = Spectrum(d);
				}
			}
			break;
		}
		case Film::POSITION: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float *v = film.channel_POSITION->GetPixel(i);
					pixels[i].c[0] = fabs(v[0]);
					pixels[i].c[1] = fabs(v[1]);
					pixels[i].c[2] = fabs(v[2]);
				}
			}
			break;
		}
		case Film::GEOMETRY_NORMAL: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float *v = film.channel_GEOMETRY_NORMAL->GetPixel(i);
					pixels[i].c[0] = fabs(v[0]);
					pixels[i].c[1] = fabs(v[1]);
					pixels[i].c[2] = fabs(v[2]);
				}
			}
			break;
		}
		case Film::SHADING_NORMAL: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float *v = film.channel_SHADING_NORMAL->GetPixel(i);
					pixels[i].c[0] = fabs(v[0]);
					pixels[i].c[1] = fabs(v[1]);
					pixels[i].c[2] = fabs(v[2]);
				}
			}
			break;
		}
		case Film::MATERIAL_ID: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					u_int *v = film.channel_MATERIAL_ID->GetPixel(i);
					pixels[i].c[0] = (*v) & 0xff;
					pixels[i].c[1] = ((*v) & 0xff00) >> 8;
					pixels[i].c[2] = ((*v) & 0xff0000) >> 16;
				}
			}
			break;
		}
		case Film::DIRECT_DIFFUSE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_DIRECT_DIFFUSE->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::DIRECT_DIFFUSE_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_DIRECT_DIFFUSE_REFLECT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}		case Film::DIRECT_DIFFUSE_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_DIRECT_DIFFUSE_TRANSMIT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::DIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_DIRECT_GLOSSY->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::DIRECT_GLOSSY_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_DIRECT_GLOSSY_REFLECT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::DIRECT_GLOSSY_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_DIRECT_GLOSSY_TRANSMIT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::EMISSION: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_EMISSION->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_DIFFUSE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_DIFFUSE->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_DIFFUSE_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_DIFFUSE_REFLECT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_DIFFUSE_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_DIFFUSE_TRANSMIT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_GLOSSY->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_GLOSSY_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_GLOSSY_REFLECT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_GLOSSY_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_GLOSSY_TRANSMIT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_SPECULAR: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_SPECULAR->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_SPECULAR_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_SPECULAR_REFLECT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::INDIRECT_SPECULAR_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_INDIRECT_SPECULAR_TRANSMIT->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::MATERIAL_ID_MASK: {
			if (index >= film.channel_MATERIAL_ID_MASKs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_MATERIAL_ID_MASKs[index]->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::DIRECT_SHADOW_MASK: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_DIRECT_SHADOW_MASK->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::INDIRECT_SHADOW_MASK: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_INDIRECT_SHADOW_MASK->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::UV: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v[2];
					film.channel_UV->GetWeightedPixel(i, v);
					pixels[i].c[0] = v[0];
					pixels[i].c[1] = v[1];
					pixels[i].c[2] = 0.f;
				}
			}
			break;
		}
		case Film::RAYCOUNT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_RAYCOUNT->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::BY_MATERIAL_ID: {
			if (index >= film.channel_BY_MATERIAL_IDs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_BY_MATERIAL_IDs[index]->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::IRRADIANCE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v[3];
					film.channel_IRRADIANCE->GetWeightedPixel(i, v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::OBJECT_ID: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					u_int *v = film.channel_OBJECT_ID->GetPixel(i);
					pixels[i].c[0] = (*v) & 0xff;
					pixels[i].c[1] = ((*v) & 0xff00) >> 8;
					pixels[i].c[2] = ((*v) & 0xff0000) >> 16;
				}
			}
			break;
		}
		case Film::OBJECT_ID_MASK: {
			if (index >= film.channel_OBJECT_ID_MASKs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_OBJECT_ID_MASKs[index]->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::BY_OBJECT_ID: {
			if (index >= film.channel_BY_OBJECT_IDs.size())
				return;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_BY_OBJECT_IDs[index]->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::SAMPLECOUNT: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					u_int *v = film.channel_SAMPLECOUNT->GetPixel(i);
					pixels[i].c[0] = (*v) & 0xff;
					pixels[i].c[1] = ((*v) & 0xff00) >> 8;
					pixels[i].c[2] = ((*v) & 0xff0000) >> 16;
				}
			}
			break;
		}
		case Film::CONVERGENCE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_CONVERGENCE->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::MATERIAL_ID_COLOR: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_MATERIAL_ID_COLOR->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::ALBEDO: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_ALBEDO->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::AVG_SHADING_NORMAL: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i))
					film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i, pixels[i].c);
			}
			break;
		}
		case Film::NOISE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_NOISE->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		case Film::USER_IMPORTANCE: {
			for (u_int i = 0; i < pixelCount; ++i) {
				if (film.HasSamples(hasPN, hasSN, i)) {
					float v;
					film.channel_USER_IMPORTANCE->GetWeightedPixel(i, &v);
					pixels[i] = Spectrum(v);
				}
			}
			break;
		}
		default:
			throw runtime_error("Unknown film output type in OutputSwitcherPlugin::Apply(): " + ToString(type));
	}
}

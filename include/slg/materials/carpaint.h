/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_CARPAINTMAT_H
#define	_SLG_CARPAINTMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// CarPaint material
//------------------------------------------------------------------------------

class CarPaintMaterial : public Material {
public:
	CarPaintMaterial(TextureConstPtr frontTransp, TextureConstPtr backTransp,
			TextureConstPtr emitted, TextureConstPtr bump,
			TextureConstPtr kd, TextureConstPtr ks1, TextureConstPtr ks2, TextureConstPtr ks3,
			TextureConstPtr m1, TextureConstPtr m2, TextureConstPtr m3,
			TextureConstPtr r1, TextureConstPtr r2, TextureConstPtr r3, TextureConstPtr ka, TextureConstPtr d);

	virtual MaterialType GetType() const { return CARPAINT; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT; };

	virtual luxrays::Spectrum Albedo(const HitPoint &hitPoint) const;

	virtual luxrays::Spectrum Evaluate(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir, BSDFEvent *event,
		float *directPdfW = NULL, float *reversePdfW = NULL) const;
	virtual luxrays::Spectrum Sample(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, luxrays::Vector *localSampledDir,
		const float u0, const float u1, const float passThroughEvent,
		float *pdfW, BSDFEvent *event) const;
	virtual void Pdf(const HitPoint &hitPoint,
		const luxrays::Vector &localLightDir, const luxrays::Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const;

	virtual void AddReferencedTextures(std::unordered_set<const Texture *>  &referencedTexsreferencedTexs) const;
	virtual void UpdateTextureReferences(TextureConstRef oldTex, TextureRef newTex);

	virtual luxrays::PropertiesUPtr ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	struct CarPaintData {
		std::string name;
		float kd[COLOR_SAMPLES];
		float ks1[COLOR_SAMPLES];
		float ks2[COLOR_SAMPLES];
		float ks3[COLOR_SAMPLES];
		float r1, r2, r3;
		float m1, m2, m3;
	};
	static const struct CarPaintData data[8];
	static int NbPresets() { return 8; }

	TextureConstPtr Kd;
	TextureConstPtr Ks1;
	TextureConstPtr Ks2;
	TextureConstPtr Ks3;
	TextureConstPtr M1;
	TextureConstPtr M2;
	TextureConstPtr M3;
	TextureConstPtr R1;
	TextureConstPtr R2;
	TextureConstPtr R3;
	TextureConstPtr Ka;
	TextureConstPtr depth;
};

}

#endif	/* _SLG_CARPAINTMAT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

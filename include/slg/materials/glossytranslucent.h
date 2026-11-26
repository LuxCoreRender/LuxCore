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

#ifndef _SLG_GLOSSYTRANSLUCENTMAT_H
#define	_SLG_GLOSSYTRANSLUCENTMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Glossy Translucent material
//------------------------------------------------------------------------------

class GlossyTranslucentMaterial : public Material {
public:
	GlossyTranslucentMaterial(TextureConstPtr frontTransp, TextureConstPtr backTransp,
			TextureConstPtr emitted, TextureConstPtr bump,
			TextureConstPtr kd, TextureConstPtr kt, TextureConstPtr ks, TextureConstPtr ks2,
			TextureConstPtr u, TextureConstPtr u2, TextureConstPtr v, TextureConstPtr v2,
			TextureConstPtr ka, TextureConstPtr ka2, TextureConstPtr d, TextureConstPtr d2,
			TextureConstPtr i, TextureConstPtr i2, const bool mbounce, const bool mbounce2);

	virtual MaterialType GetType() const { return GLOSSYTRANSLUCENT; }
	virtual BSDFEvent GetEventTypes() const { return GLOSSY | REFLECT | TRANSMIT; };

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

	TextureConstPtr GetKd() const { return Kd; }
	TextureConstPtr GetKt() const { return Kt; }
	TextureConstPtr GetKs() const { return Ks; }
	TextureConstPtr GetKs_bf() const { return Ks_bf; }
	TextureConstPtr GetNu() const { return nu; }
	TextureConstPtr GetNu_bf() const { return nu_bf; }
	TextureConstPtr GetNv() const { return nv; }
	TextureConstPtr GetNv_bf() const { return nv_bf; }
	TextureConstPtr GetKa() const { return Ka; }
	TextureConstPtr GetKa_bf() const { return Ka_bf; }
	TextureConstPtr GetDepth() const { return depth; }
	TextureConstPtr GetDepth_bf() const { return depth_bf; }
	TextureConstPtr GetIndex() const { return index; }
	TextureConstPtr GetIndex_bf() const { return index_bf; }
	const bool IsMultibounce() const { return multibounce; }
	const bool IsMultibounce_bf() const { return multibounce_bf; }

private:
	TextureConstPtr Kd;
	TextureConstPtr Kt;
	TextureConstPtr Ks;
	TextureConstPtr Ks_bf;
	TextureConstPtr nu;
	TextureConstPtr nu_bf;
	TextureConstPtr nv;
	TextureConstPtr nv_bf;
	TextureConstPtr Ka;
	TextureConstPtr Ka_bf;
	TextureConstPtr depth;
	TextureConstPtr depth_bf;
	TextureConstPtr index;
	TextureConstPtr index_bf;
	const bool multibounce;
	const bool multibounce_bf;
};

}

#endif	/* _SLG_GLOSSYTRANSLUCENTMAT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

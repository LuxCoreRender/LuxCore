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

#ifndef _SLG_CLOTHMAT_H
#define	_SLG_CLOTHMAT_H

#include "slg/materials/material.h"

namespace slg {

//------------------------------------------------------------------------------
// Cloth material
//------------------------------------------------------------------------------

class ClothMaterial : public Material {
public:
	ClothMaterial(const Texture *frontTransp, const Texture *backTransp,
			const Texture *emitted, const Texture *bump,
            const slg::ocl::ClothPreset preset, const Texture *weft_kd, const Texture *weft_ks,
            const Texture *warp_kd, const Texture *warp_ks, const float repeat_u, const float repeat_v);

	virtual MaterialType GetType() const { return CLOTH; }
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

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const;
	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex);

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

    slg::ocl::ClothPreset GetPreset() const { return Preset; }
	const Texture *GetWeftKd() const { return Weft_Kd; }
	const Texture *GetWeftKs() const { return Weft_Ks; }
	const Texture *GetWarpKd() const { return Warp_Kd; }
	const Texture *GetWarpKs() const { return Warp_Ks; }
	const float GetRepeatU() const { return Repeat_U; }
	const float GetRepeatV() const { return Repeat_V; }
    const float GetSpecularNormalization() const { return specularNormalization; }

private:
	void SetPreset();

	const slg::ocl::Yarn *GetYarn(const float u_i, const float v_i, luxrays::UV *uv, float *umax, float *scale) const;
	void GetYarnUV(const slg::ocl::Yarn *yarn, const luxrays::Point &center, const luxrays::Point &xy, luxrays::UV *uv, float *umaxMod) const;
	
	float RadiusOfCurvature(const slg::ocl::Yarn *yarn, float u, float umaxMod) const;
	float EvalSpecular(const slg::ocl::Yarn *yarn, const luxrays::UV &uv,
        float umax, const luxrays::Vector &wo, const luxrays::Vector &vi) const;
	float EvalIntegrand(const slg::ocl::Yarn *yarn, const luxrays::UV &uv,
        float umaxMod, luxrays::Vector &om_i, luxrays::Vector &om_r) const;
	float EvalFilamentIntegrand(const slg::ocl::Yarn *yarn, const luxrays::Vector &om_i,
        const luxrays::Vector &om_r, float u, float v, float umaxMod) const;
	float EvalStapleIntegrand(const slg::ocl::Yarn *yarn, const luxrays::Vector &om_i,
        const luxrays::Vector &om_r, float u, float v, float umaxMod) const;

	const slg::ocl::ClothPreset Preset;
	const Texture *Weft_Kd;
	const Texture *Weft_Ks;
	const Texture *Warp_Kd;
	const Texture *Warp_Ks;
	const float Repeat_U;
	const float Repeat_V;
	float specularNormalization;
};

}

#endif	/* _SLG_CLOTHMAT_H */

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

#ifndef _SLG_PGICBVH_H
#define	_SLG_PGICBVH_H

#include <vector>

#include "slg/slg.h"
#include "slg/core/indexbvh.h"

namespace slg {

//------------------------------------------------------------------------------
// PGICPhotonBvh
//------------------------------------------------------------------------------

class BSDF;
class Photon;

class PGICPhotonBvh : public IndexBvh<Photon> {
public:
	PGICPhotonBvh(const std::vector<Photon> *entries, const u_int photonTracedCount,
			const float radius, const float normalAngle);
	virtual ~PGICPhotonBvh();

	float GetEntryNormalCosAngle() const { return entryNormalCosAngle; }
	
	luxrays::SpectrumGroup ConnectAllNearEntries(const BSDF &bsdf) const;

	friend class boost::serialization::access;

private:
	luxrays::SpectrumGroup ConnectCacheEntry(const Photon &photon, const BSDF &bsdf) const;

	// Used by serialization
	PGICPhotonBvh() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(IndexBvh);
		ar & entryNormalCosAngle;
	}

	float entryNormalCosAngle;
	u_int photonTracedCount;
};

//------------------------------------------------------------------------------
// PGICRadiancePhotonBvh
//------------------------------------------------------------------------------

class RadiancePhoton;

class PGICRadiancePhotonBvh : public IndexBvh<RadiancePhoton> {
public:
	PGICRadiancePhotonBvh(const std::vector<RadiancePhoton> *entries,
			const float radius, const float normalAngle);
	virtual ~PGICRadiancePhotonBvh();

	float GetEntryNormalCosAngle() const { return entryNormalCosAngle; }

	const RadiancePhoton *GetNearestEntry(const luxrays::Point &p, const luxrays::Normal &n,
			const bool isVolume) const;

	friend class boost::serialization::access;

private:
	// Used by serialization
	PGICRadiancePhotonBvh() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(IndexBvh);
		ar & entryNormalCosAngle;
	}

	float entryNormalCosAngle;
};

}

BOOST_CLASS_VERSION(slg::PGICPhotonBvh, 2)
BOOST_CLASS_VERSION(slg::PGICRadiancePhotonBvh, 1)

BOOST_CLASS_EXPORT_KEY(slg::PGICPhotonBvh)
BOOST_CLASS_EXPORT_KEY(slg::PGICRadiancePhotonBvh)

#endif	/* _SLG_PGICBVH_H */

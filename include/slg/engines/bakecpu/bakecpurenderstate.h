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

#ifndef _SLG_BAKECPURENDERSTATE_H
#define	_SLG_BAKECPURENDERSTATE_H

#include "luxrays/utils/serializationutils.h"

#include "slg/slg.h"
#include "slg/renderstate.h"

namespace slg {

class PhotonGICache;

class BakeCPURenderState : public RenderState {
public:
	BakeCPURenderState(const u_int seed, PhotonGICache *photonGICache);
	virtual ~BakeCPURenderState();

	u_int bootStrapSeed;
	PhotonGICache *photonGICache;

	friend class boost::serialization::access;

private:
	// Used by serialization
	BakeCPURenderState();

	template<class Archive> void save(Archive &ar, const unsigned int version) const;
	template<class Archive>	void load(Archive &ar, const unsigned int version);
	BOOST_SERIALIZATION_SPLIT_MEMBER()

	bool deletePhotonGICachePtr;
};

}

BOOST_CLASS_VERSION(slg::BakeCPURenderState, 1)

BOOST_CLASS_EXPORT_KEY(slg::BakeCPURenderState)

#endif	/* _SLG_BAKECPURENDERSTATE_H */

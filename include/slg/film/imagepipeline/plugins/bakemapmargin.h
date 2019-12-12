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

#ifndef _SLG_BAKEMAPMARGIN_PLUGIN_H
#define	_SLG_BAKEMAPMARGIN_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// Bake maps margin plugin
//------------------------------------------------------------------------------

class BakeMapMarginPlugin : public ImagePipelinePlugin {
public:
	BakeMapMarginPlugin(const u_int marginPixels);
	virtual ~BakeMapMarginPlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	friend class boost::serialization::access;

	u_int marginPixels;

private:
	// Used by Copy() and serialization
	BakeMapMarginPlugin() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & marginPixels;
	}
};

}

BOOST_CLASS_VERSION(slg::BakeMapMarginPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::BakeMapMarginPlugin)

#endif	/*  _SLG_BAKEMAPMARGIN_PLUGIN_H */

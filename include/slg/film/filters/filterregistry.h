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

#ifndef _SLG_FILTERREGISTRY_H
#define	_SLG_FILTERREGISTRY_H

#include <string>

#include "slg/core/objectstaticregistry.h"
#include "slg/film/filters/none.h"
#include "slg/film/filters/box.h"
#include "slg/film/filters/gaussian.h"
#include "slg/film/filters/mitchell.h"
#include "slg/film/filters/mitchellss.h"
#include "slg/film/filters/blackmanharris.h"
#include "slg/film/filters/sinc.h"
#include "slg/film/filters/catmullrom.h"

namespace slg {

//------------------------------------------------------------------------------
// FilterRegistry
//------------------------------------------------------------------------------

class FilterRegistry {
protected:
	FilterRegistry() { }

	//--------------------------------------------------------------------------

	typedef FilterType ObjectType;
	// Used to register all sub-class String2FilterType() static methods
	typedef FilterType (*GetObjectType)();
	// Used to register all sub-class FilterType2String() static methods
	typedef std::string (*GetObjectTag)();
	// Used to register all sub-class ToProperties() static methods
	typedef luxrays::Properties (*ToProperties)(const luxrays::Properties &cfg);
	// Used to register all sub-class FromProperties() static methods
	typedef Filter *(*FromProperties)(const luxrays::Properties &cfg);
	// Used to register all sub-class FromPropertiesOCL() static methods
	typedef slg::ocl::Filter *(*FromPropertiesOCL)(const luxrays::Properties &cfg);

	OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(FilterRegistry);

	//--------------------------------------------------------------------------

	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, NoneFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, BoxFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, GaussianFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, MitchellFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, MitchellSSFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, BlackmanHarrisFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, SincFilter);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(FilterRegistry, CatmullRomFilter);
	// Just add here any new Filter (don't forget in the .cpp too)

	friend class Filter;
};

}

#endif	/* _SLG_FILTERREGISTRY_H */

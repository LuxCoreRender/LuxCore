/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_LIGHTSTRATEGYREGISTRY_H
#define	_SLG_LIGHTSTRATEGYREGISTRY_H

#include <string>
#include <vector>

#include "slg/core/objectstaticregistry.h"
#include "slg/lights/lightstrategy.h"

namespace slg {

//------------------------------------------------------------------------------
// LightStrategyRegistry
//------------------------------------------------------------------------------

class LightStrategyRegistry {
protected:
	LightStrategyRegistry() { }

	//--------------------------------------------------------------------------

	typedef LightStrategyType ObjectType;
	// Used to register all sub-class String2LightStrategyType() static methods
	typedef LightStrategyType (*GetObjectType)();
	// Used to register all sub-class LightStrategyType2String() static methods
	typedef std::string (*GetObjectTag)();
	// Used to register all sub-class ToProperties() static methods
	typedef luxrays::Properties (*ToProperties)(const luxrays::Properties &cfg);
	// Used to register all sub-class FromProperties() static methods
	typedef LightStrategy *(*FromProperties)(const luxrays::Properties &cfg);
	// Used to register all sub-class FromPropertiesOCL() static methods
	typedef std::string (*FromPropertiesOCL)(const luxrays::Properties &cfg);

	OBJECTSTATICREGISTRY_DECLARE_STATICFIELDS(LightStrategyRegistry);

	//--------------------------------------------------------------------------

	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(LightStrategyRegistry, LightStrategyUniform);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(LightStrategyRegistry, LightStrategyPower);
	OBJECTSTATICREGISTRY_DECLARE_REGISTRATION(LightStrategyRegistry, LightStrategyLogPower);
	// Just add here any new LightStrategy (don't forget in the .cpp too)

	friend class LightStrategy;
};

}

#endif	/* _SLG_LIGHTSTRATEGYREGISTRY_H */

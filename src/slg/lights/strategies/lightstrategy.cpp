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

#include "slg/lights/strategies/lightstrategyregistry.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightStrategy
//------------------------------------------------------------------------------

LightStrategyType LightStrategy::GetType(const luxrays::Properties &cfg) {
	const string type = cfg.Get(Property("lightstrategy.type")(LightStrategyLogPower::GetObjectTag())).Get<string>();
	
	return String2LightStrategyType(type);
}

//------------------------------------------------------------------------------
// Static methods used by LightStrategyRegistry
//------------------------------------------------------------------------------

Properties LightStrategy::ToProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("lightstrategy.type")(LightStrategyLogPower::GetObjectTag())).Get<string>();

	LightStrategyRegistry::ToProperties func;

	if (LightStrategyRegistry::STATICTABLE_NAME(ToProperties).Get(type, func)) {
		return func(cfg);
	} else
		throw runtime_error("Unknown light strategy type in LightStrategy::ToProperties(): " + type);
}

LightStrategy *LightStrategy::FromProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("lightstrategy.type")(LightStrategyLogPower::GetObjectTag())).Get<string>();

	LightStrategyRegistry::FromProperties func;
	if (LightStrategyRegistry::STATICTABLE_NAME(FromProperties).Get(type, func))
		return func(cfg);
	else
		throw runtime_error("Unknown filter type in LightStrategy::FromProperties(): " + type);
}

string LightStrategy::FromPropertiesOCL(const Properties &cfg) {
	throw runtime_error("Called LightStrategy::FromPropertiesOCL()");
}

LightStrategyType LightStrategy::String2LightStrategyType(const string &type) {
	LightStrategyRegistry::GetObjectType func;
	if (LightStrategyRegistry::STATICTABLE_NAME(GetObjectType).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown light strategy type in LightStrategy::String2LightStrategyType(): " + type);
}

string LightStrategy::LightStrategyType2String(const LightStrategyType type) {
	LightStrategyRegistry::GetObjectTag func;
	if (LightStrategyRegistry::STATICTABLE_NAME(GetObjectTag).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown light strategy type in LightStrategy::LightStrategyType2String(): " + ToString(type));
}

const Properties &LightStrategy::GetDefaultProps() {
	static Properties props;

	return props;
}

//------------------------------------------------------------------------------
// LightStrategyRegistry
//
// For the registration of each LightStrategy sub-class with LightStrategy StaticTables
//
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because otherwise
// static members initialization order is not defined.
//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_STATICFIELDS(LightStrategyRegistry);

//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyUniform);
OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyPower);
OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyLogPower);
OBJECTSTATICREGISTRY_REGISTER(LightStrategyRegistry, LightStrategyDLSCache);
// Just add here any new LightStrategy (don't forget in the .h too)

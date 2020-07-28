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

#include "slg/film/filters/filter.h"
#include "slg/film/filters/filterregistry.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Filter
//------------------------------------------------------------------------------

Properties Filter::ToProperties() const {
	Properties props;
	
	props << Property("film.filter.type")(FilterType2String(GetType()));
	
	if (xWidth == yWidth)
		props << Property("film.filter.width")(xWidth);
	else
		props <<
				Property("film.filter.xwidth")(xWidth) <<
				Property("film.filter.ywidth")(yWidth);
	
	return props;
}

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

Properties Filter::ToProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("film.filter.type")(BlackmanHarrisFilter::GetObjectTag())).Get<string>();

	FilterRegistry::ToProperties func;

	if (FilterRegistry::STATICTABLE_NAME(ToProperties).Get(type, func)) {
		Properties props;

		const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
		const Property filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth));
		const Property filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth));

		if (filterXWidth.Get<float>() == filterYWidth.Get<float>())
			props << Property("film.filter.width")(filterXWidth.Get<float>());
		else
			props << filterXWidth << filterYWidth;

		return func(cfg) << props;
	} else
		throw runtime_error("Unknown filter type in Filter::ToProperties(): " + type);
}

Filter *Filter::FromProperties(const Properties &cfg) {
	const string type = cfg.Get(Property("film.filter.type")(BlackmanHarrisFilter::GetObjectTag())).Get<string>();

	FilterRegistry::FromProperties func;
	if (FilterRegistry::STATICTABLE_NAME(FromProperties).Get(type, func))
		return func(cfg);
	else
		throw runtime_error("Unknown filter type in Filter::FromProperties(): " + type);
}

slg::ocl::Filter *Filter::FromPropertiesOCL(const Properties &cfg) {
	const string type = cfg.Get(Property("film.filter.type")(BlackmanHarrisFilter::GetObjectTag())).Get<string>();

	FilterRegistry::FromPropertiesOCL func;
	if (FilterRegistry::STATICTABLE_NAME(FromPropertiesOCL).Get(type, func))
		return func(cfg);
	else
		throw runtime_error("Unknown filter type in Filter::FromPropertiesOCL(): " + type);
}

FilterType Filter::String2FilterType(const string &type) {
	FilterRegistry::GetObjectType func;
	if (FilterRegistry::STATICTABLE_NAME(GetObjectType).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown filter type in Filter::String2FilterType(): " + type);
}

const string Filter::FilterType2String(const FilterType type) {
	FilterRegistry::GetObjectTag func;
	if (FilterRegistry::STATICTABLE_NAME(GetObjectTag).Get(type, func))
		return func();
	else
		throw runtime_error("Unknown filter type in Filter::FilterType2String(): " + boost::lexical_cast<string>(type));
}

const Properties &Filter::GetDefaultProps() {
	static Properties props = Properties() <<
			Property("film.filter.width")(2.f);

	return props;
}

//------------------------------------------------------------------------------
// FilterRegistry
//
// For the registration of each Filter sub-class with Filter StaticTables
//
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because otherwise
// static members initialization order is not defined.
//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_STATICFIELDS(FilterRegistry);

//------------------------------------------------------------------------------

OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, NoneFilter);
OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, BoxFilter);
OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, GaussianFilter);
OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, MitchellFilter);
OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, MitchellSSFilter);
OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, BlackmanHarrisFilter);
OBJECTSTATICREGISTRY_REGISTER(FilterRegistry, SincFilter);
// Just add here any new Filter (don't forget in the .h too)

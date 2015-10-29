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

#include "slg/film/filters/blackmanharris.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BlackmanHarrisFilter)

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

Properties BlackmanHarrisFilter::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("film.filter.type"));
}

Filter *BlackmanHarrisFilter::FromProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	return new BlackmanHarrisFilter(filterXWidth, filterYWidth);
}

slg::ocl::Filter *BlackmanHarrisFilter::FromPropertiesOCL(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	oclFilter->type = slg::ocl::FILTER_BLACKMANHARRIS;
	oclFilter->blackmanharris.widthX = filterXWidth;
	oclFilter->blackmanharris.widthY = filterYWidth;

	return oclFilter;
}

Properties BlackmanHarrisFilter::GetDefaultProps() {
	static Properties props = Filter::GetDefaultProps() <<
			Property("film.filter.type")(GetObjectTag());

	return props;
}

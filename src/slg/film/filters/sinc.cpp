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

#include "slg/film/filters/sinc.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::SincFilter)

Properties SincFilter::ToProperties() const {
	return Filter::ToProperties() <<
			Property("film.filter.sinc.tau")(alpha);
}

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

Properties SincFilter::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("film.filter.type")) <<
			cfg.Get(GetDefaultProps().Get("film.filter.sinc.tau"));
}

Filter *SincFilter::FromProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	const float tau = cfg.Get(GetDefaultProps().Get("film.filter.sinc.tau")).Get<float>();

	return new SincFilter(filterXWidth, filterYWidth, tau);
}

slg::ocl::Filter *SincFilter::FromPropertiesOCL(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	oclFilter->widthX = filterXWidth;
	oclFilter->widthY = filterYWidth;

	return oclFilter;
}

const Properties &SincFilter::GetDefaultProps() {
	static Properties props = Properties() <<
			Filter::GetDefaultProps() <<
			Property("film.filter.type")(GetObjectTag()) <<
			Property("film.filter.sinc.tau")(3.f);

	return props;
}

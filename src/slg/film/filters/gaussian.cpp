/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#include "slg/film/filters/gaussian.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::GaussianFilter)

Properties GaussianFilter::ToProperties() const {
	return Filter::ToProperties() <<
			Property("film.filter.gaussian.alpha")(alpha);
}

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

Properties GaussianFilter::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("film.filter.type")) <<
			cfg.Get(GetDefaultProps().Get("film.filter.gaussian.alpha"));
}

Filter *GaussianFilter::FromProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	const float alpha = cfg.Get(GetDefaultProps().Get("film.filter.gaussian.alpha")).Get<float>();

	return new GaussianFilter(filterXWidth, filterYWidth, alpha);
}

slg::ocl::Filter *GaussianFilter::FromPropertiesOCL(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	const float alpha = cfg.Get(GetDefaultProps().Get("film.filter.gaussian.alpha")).Get<float>();

	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	oclFilter->type = slg::ocl::FILTER_GAUSSIAN;
	oclFilter->gaussian.widthX = filterXWidth;
	oclFilter->gaussian.widthY = filterYWidth;
	oclFilter->gaussian.alpha = alpha;

	return oclFilter;
}

const Properties &GaussianFilter::GetDefaultProps() {
	static Properties props = Properties() <<
			Filter::GetDefaultProps() <<
			Property("film.filter.type")(GetObjectTag()) <<
			Property("film.filter.gaussian.alpha")(2.f);

	return props;
}

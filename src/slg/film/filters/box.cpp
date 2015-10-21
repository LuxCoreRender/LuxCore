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

#include "slg/film/filters/box.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::BoxFilter)

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

Properties BoxFilter::ToProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(defaultProps.Get("film.filter.width")).Get<float>();

	return Properties() <<
			cfg.Get(defaultProps.Get("film.filter.type")) <<
			cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)) <<
			cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth));
}


Filter *BoxFilter::FromProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(defaultProps.Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	return new BoxFilter(filterXWidth, filterYWidth);
}

slg::ocl::Filter *BoxFilter::FromPropertiesOCL(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(defaultProps.Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	oclFilter->type = slg::ocl::FILTER_NONE;
	oclFilter->box.widthX = filterXWidth;
	oclFilter->box.widthY = filterYWidth;

	return oclFilter;
}

Properties BoxFilter::defaultProps = Properties() <<
			Property("film.filter.type")(BoxFilter::GetObjectTag()) <<
			Property("film.filter.width")(2.f);

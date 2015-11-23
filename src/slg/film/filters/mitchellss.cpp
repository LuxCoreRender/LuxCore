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

#include "slg/film/filters/mitchellss.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::MitchellSSFilter)

Properties MitchellSSFilter::ToProperties() const {
	return Properties() <<
			Property("film.filter.filter.mitchell.b")(B) <<
			Property("film.filter.filter.mitchell.c")(C);
}

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

Properties MitchellSSFilter::ToProperties(const Properties &cfg) {
	return Properties() <<
			cfg.Get(GetDefaultProps().Get("film.filter.type")) <<
			cfg.Get(GetDefaultProps().Get("film.filter.mitchellss.b")) <<
			cfg.Get(GetDefaultProps().Get("film.filter.mitchellss.c"));
}

Filter *MitchellSSFilter::FromProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	const float b = cfg.Get(GetDefaultProps().Get("film.filter.mitchellss.b")).Get<float>();
	const float c = cfg.Get(GetDefaultProps().Get("film.filter.mitchellss.c")).Get<float>();

	return new MitchellSSFilter(filterXWidth, filterYWidth, b, c);
}

slg::ocl::Filter *MitchellSSFilter::FromPropertiesOCL(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps().Get("film.filter.width")).Get<float>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<float>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<float>();

	const float b = cfg.Get(GetDefaultProps().Get("film.filter.mitchellss.b")).Get<float>();
	const float c = cfg.Get(GetDefaultProps().Get("film.filter.mitchellss.c")).Get<float>();

	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	oclFilter->type = slg::ocl::FILTER_MITCHELL_SS;
	oclFilter->mitchellss.widthX = filterXWidth;
	oclFilter->mitchellss.widthY = filterYWidth;
	oclFilter->mitchellss.B = b;
	oclFilter->mitchellss.C = c;
	oclFilter->mitchellss.a0 = CalcA0(b, c);
	oclFilter->mitchellss.a1 = CalcA1(oclFilter->mitchellss.a0);

	return oclFilter;
}

Properties MitchellSSFilter::GetDefaultProps() {
	static Properties props = Filter::GetDefaultProps() <<
			Property("film.filter.type")(GetObjectTag()) <<
			Property("film.filter.mitchell.b")(1.f / 3.f) <<
			Property("film.filter.mitchell.c")(1.f / 3.f);

	return props;
}

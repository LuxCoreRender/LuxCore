/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "slg/film/filters/catmullrom.h"
#include <memory>

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::CatmullRomFilter)

PropertiesUPtr CatmullRomFilter::ToProperties() const {
	auto props = std::make_unique<Properties>();
	*props << Filter::ToProperties() <<
			Property("film.filter.sinc.tau")(alpha);
	return props;
}

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

PropertiesUPtr CatmullRomFilter::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	
	*props <<
				cfg.Get(GetDefaultProps()->Get("film.filter.type"));
	
	return props;
}

FilterUPtr CatmullRomFilter::FromProperties(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps()->Get("film.filter.width")).Get<double>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<double>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<double>();

	return std::make_unique<CatmullRomFilter>(filterXWidth, filterYWidth);
}

slg::ocl::Filter *CatmullRomFilter::FromPropertiesOCL(const Properties &cfg) {
	const float defaultFilterWidth = cfg.Get(GetDefaultProps()->Get("film.filter.width")).Get<double>();
	const float filterXWidth = cfg.Get(Property("film.filter.xwidth")(defaultFilterWidth)).Get<double>();
	const float filterYWidth = cfg.Get(Property("film.filter.ywidth")(defaultFilterWidth)).Get<double>();

	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	oclFilter->widthX = filterXWidth;
	oclFilter->widthY = filterYWidth;

	return oclFilter;
}

PropertiesUPtr CatmullRomFilter::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			Filter::GetDefaultProps() <<
			Property("film.filter.type")(GetObjectTag());

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

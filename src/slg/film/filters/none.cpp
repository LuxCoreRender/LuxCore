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

#include "slg/film/filters/none.h"

using namespace std;
using namespace luxrays;
using namespace slg;

BOOST_CLASS_EXPORT_IMPLEMENT(slg::NoneFilter)

//------------------------------------------------------------------------------
// Static methods used by FilterRegistry
//------------------------------------------------------------------------------

PropertiesUPtr NoneFilter::ToProperties(const Properties &cfg) {
	PropertiesUPtr props = std::make_unique<Properties>();
	
	*props <<
				cfg.Get(GetDefaultProps()->Get("film.filter.type"));
	
	return props;
}

FilterUPtr NoneFilter::FromProperties(const Properties &cfg) {
	return std::make_unique<NoneFilter>();
}

slg::ocl::Filter *NoneFilter::FromPropertiesOCL(const Properties &cfg) {
	slg::ocl::Filter *oclFilter = new slg::ocl::Filter();

	//oclFilter->type = slg::ocl::FILTER_NONE;

	oclFilter->widthX = .5f;
	oclFilter->widthY = .5f;

	return oclFilter;
}

PropertiesUPtr NoneFilter::GetDefaultProps() {
	auto props = std::make_unique<Properties>();
	*props <<
			Filter::GetDefaultProps() <<
			Property("film.filter.type")(GetObjectTag());

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

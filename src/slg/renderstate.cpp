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

#include "luxrays/luxrays.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/renderstate.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// RenderState
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::RenderState)

RenderState::RenderState(const std::string &tag) : engineTag(tag) {
}

RenderState::~RenderState() {
}

void RenderState::CheckEngineTag(const std::string &tag) {
	if (tag != engineTag)
		throw runtime_error("Wrong engine type in a render state: " + engineTag + " instead of " + tag);
}

RenderState *RenderState::LoadSerialized(const std::string &fileName) {
	SerializationInputFile<LuxInputBinArchive> sif(fileName);

	RenderState *renderState;
	sif.GetArchive() >> renderState;

	if (!sif.IsGood())
		throw runtime_error("Error while loading serialized render state: " + fileName);

	return renderState;
}

void RenderState::SaveSerialized(const std::string &fileName) {
	SLG_LOG("Saving render state: " << fileName);

	SerializationOutputFile<LuxOutputBinArchive> sof(fileName);

	// The following line is a workaround to a clang bug
	RenderState *state = this;
	sof.GetArchive() << state;

	if (!sof.IsGood())
		throw runtime_error("Error while saving serialized render state: " + fileName);

	sof.Flush();

	const streamoff size = sof.GetPosition();
	if (size < 1024) {
		SLG_LOG("Render state saved: " << size << " bytes");
	} else {
		SLG_LOG("Render state saved: " << (size / 1024) << " Kbytes");
	}
}

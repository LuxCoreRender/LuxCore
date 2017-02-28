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

#ifndef _LUXCOREIMPL_H
#define	_LUXCOREIMPL_H

#include <luxcore/luxcore.h>
#include <slg/renderconfig.h>
#include <slg/rendersession.h>
#include <slg/renderstate.h>
#include <slg/scene/scene.h>
#include <slg/film/film.h>

namespace luxcore {

class RenderSession;

class FilmImpl : public Film {
public:
	FilmImpl(const std::string &fileName);
	FilmImpl(const RenderSession &session);
	~FilmImpl();

	unsigned int GetWidth() const;
	unsigned int GetHeight() const;

	void SaveOutputs() const;
	void SaveOutput(const std::string &fileName, const FilmOutputType type, const luxrays::Properties &props) const;
	void SaveFilm(const std::string &fileName) const;

	double GetTotalSampleCount() const;

	size_t GetOutputSize(const FilmOutputType type) const;
	bool HasOutput(const FilmOutputType type) const;
	
	unsigned int GetRadianceGroupCount() const;
	unsigned int GetChannelCount(const FilmChannelType type) const;

	void GetOutputFloat(const FilmOutputType type, float *buffer, const unsigned int index);
	void GetOutputUInt(const FilmOutputType type, unsigned int *buffer, const unsigned int index);
	
	const float *GetChannelFloat(const FilmChannelType type, const unsigned int index);
	const unsigned int *GetChannelUInt(const FilmChannelType type, const unsigned int index);

	void Parse(const luxrays::Properties &props);

	friend class RenderSession;

private:
	slg::Film *GetSLGFilm() const;

	const RenderSession *renderSession;
	slg::Film *standAloneFilm;
};

}

#endif	/* _LUXCOREIMPL_H */

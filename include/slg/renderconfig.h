/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _SLG_RENDERCONFIG_H
#define	_SLG_RENDERCONFIG_H

#include <boost/thread/mutex.hpp>

#include "luxrays/core/randomgen.h"
#include "luxrays/utils/properties.h"
#include "slg/slg.h"
#include "slg/sampler/sampler.h"
#include "slg/sdl/scene.h"

namespace slg {

class RenderConfig {
public:
	RenderConfig(const luxrays::Properties &props, Scene *scene = NULL);
	~RenderConfig();

	void SetScreenRefreshInterval(const u_int t);
	u_int GetScreenRefreshInterval() const;
	bool GetFilmSize(u_int *filmFullWidth, u_int *filmFullHeight,
		u_int *filmSubRegion) const;
	void GetScreenSize(u_int *width, u_int *height) const;

	Film *AllocFilm(FilmOutputs &filmOutputs) const;
	Sampler *AllocSampler(luxrays::RandomGenerator *rndGen, Film *film,
		double *metropolisSharedTotalLuminance, double *metropolisSharedSampleCount) const;
	RenderEngine *AllocRenderEngine(Film *film, boost::mutex *filmMutex) const;

	luxrays::Properties cfg;
	Scene *scene;

private:
	u_int screenRefreshInterval;
	bool allocatedScene;
};

}

#endif	/* _SLG_RENDERCONFIG_H */

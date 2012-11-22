/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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

#ifndef _RENDERCONFIG_H
#define	_RENDERCONFIG_H

#include "smalllux.h"

#include <boost/thread/mutex.hpp>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/sampler/sampler.h"

class RenderConfig {
public:
	RenderConfig(const string &fileName, const Properties *additionalProperties);
	~RenderConfig();

	void SetScreenRefreshInterval(const unsigned int t);
	unsigned int GetScreenRefreshInterval() const;
	void GetScreenSize(u_int *width, u_int *height) const;
	void GetFilmSize(u_int *width, u_int *height, u_int *subRegion) const;

	Sampler *AllocSampler(RandomGenerator *rndGen, Film *film) const;
	
	Properties cfg;
	Scene *scene;

private:
	unsigned int screenRefreshInterval;
};

#endif	/* _RENDERCONFIG_H */

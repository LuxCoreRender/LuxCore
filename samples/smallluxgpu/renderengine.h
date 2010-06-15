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

#ifndef _RENDERENGINE_H
#define	_RENDERENGINE_H

#include "smalllux.h"

#include "luxrays/utils/film/film.h"

class RenderEngine {
public:
	RenderEngine(SLGScene *scn, Film *flm) {
		scene = scn;
		film = flm;
	};
	virtual ~RenderEngine() { };

	virtual void Start() {
		assert (!started);
		started = true;
	}
    virtual void Interrupt() = 0;
	virtual void Stop() {
		assert (started);
		started = false;
	}

	virtual void Reset() = 0;

	virtual unsigned int GetPass() const = 0;
	virtual unsigned int GetThreadCount() const = 0;

protected:
	SLGScene *scene;
	Film *film;

	bool started;
};

#endif	/* _RENDERENGINE_H */

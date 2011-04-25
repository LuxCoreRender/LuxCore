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

enum RenderEngineType {
	PATHOCL
};

class RenderEngine {
public:
	RenderEngine(Scene *scn, Film *flm, boost::mutex *flmMutex) {
		scene = scn;
		film = flm;
		filmMutex = flmMutex;
		screenRefreshInterval = 100;
		started = false;
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

	virtual void SetScreenRefreshInterval(const unsigned int t) {  screenRefreshInterval = t; }
	unsigned int GetScreenRefreshInterval() const {  return screenRefreshInterval; }

	virtual unsigned int GetPass() const = 0;
	virtual unsigned int GetThreadCount() const = 0;
	virtual RenderEngineType GetEngineType() const = 0;

protected:
	Scene *scene;
	Film *film;
	boost::mutex *filmMutex;

	unsigned int screenRefreshInterval; // in ms

	bool started;
};

#endif	/* _RENDERENGINE_H */

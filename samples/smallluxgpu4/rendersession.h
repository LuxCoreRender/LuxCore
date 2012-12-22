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

#ifndef _SLG_RENDERSESSION_H
#define	_SLG_RENDERSESSION_H

#include "slg.h"
#include "renderconfig.h"
#include "renderengine.h"

#include "luxrays/utils/film/film.h"

namespace slg {
	
class RenderSession {
public:
	RenderSession(RenderConfig *cfg);
	~RenderSession();

	void Start();
	void Stop();

	void BeginEdit();
	void EndEdit();

	void SetRenderingEngineType(const RenderEngineType engineType);

	bool NeedPeriodicSave();
	void SaveFilmImage();

	RenderConfig *renderConfig;
	RenderEngine *renderEngine;

	boost::mutex filmMutex;
	luxrays::utils::Film *film;

	EditActionList editActions;

protected:
	double lastPeriodicSave, periodiceSaveTime;

	bool started, editMode, periodicSaveEnabled;
};

}

#endif	/* _SLG_RENDERSESSION_H */

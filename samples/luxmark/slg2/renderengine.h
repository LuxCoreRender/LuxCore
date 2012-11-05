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
#include "renderconfig.h"
#include "editaction.h"

#include "luxrays/utils/film/nativefilm.h"

enum RenderEngineType {
	PATHOCL
};

class RenderEngine {
public:
	RenderEngine(RenderConfig *cfg, NativeFilm *flm, boost::mutex *flmMutex);
	virtual ~RenderEngine();

	virtual void Start() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		StartLockLess();
	}
	virtual void Stop() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		StopLockLess();
	}

	virtual void BeginEdit() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		BeginEditLockLess();
	}

	virtual void EndEdit(const EditActionList &editActions) {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		EndEditLockLess(editActions);
	}

	virtual void UpdateFilm() = 0;

	virtual RenderEngineType GetEngineType() const = 0;
	virtual unsigned int GetPass() const = 0;
	virtual double GetTotalSamplesSec() const = 0;
	virtual double GetRenderingTime() const = 0;

protected:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	boost::mutex engineMutex;

	RenderConfig *renderConfig;
	NativeFilm *film;
	boost::mutex *filmMutex;

	bool started, editMode;
};

class OCLRenderEngine : public RenderEngine {
public:
	OCLRenderEngine(RenderConfig *cfg, NativeFilm *flm, boost::mutex *flmMutex);
	virtual ~OCLRenderEngine();

	virtual void Start() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		StartLockLess();
	}
	virtual void Stop() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		StopLockLess();
	}

	virtual void BeginEdit() {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		BeginEditLockLess();
	}

	virtual void EndEdit(const EditActionList &editActions) {
		boost::unique_lock<boost::mutex> lock(engineMutex);

		EndEditLockLess(editActions);
	}

	const vector<OpenCLIntersectionDevice *> &GetIntersectionDevices() const {
		return oclIntersectionDevices;
	}

	const std::vector<DeviceDescription *> &GetAvailableDeviceDescriptions() {
		return ctx->GetAvailableDeviceDescriptions();
	}

protected:
	void StartLockLess();
	void StopLockLess();

	void BeginEditLockLess();
	void EndEditLockLess(const EditActionList &editActions);

	Context *ctx;
	vector<OpenCLIntersectionDevice *> oclIntersectionDevices;
};

#endif	/* _RENDERENGINE_H */

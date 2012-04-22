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

#ifndef _LUXMARKAPP_H
#define _LUXMARKAPP_H

#include <QtGui/QApplication>
#include <QTimer>

#include <boost/thread.hpp>

#include "mainwindow.h"
#include "rendersession.h"
#include "hardwaretree.h"

//------------------------------------------------------------------------------
// LuxMark Qt application
//------------------------------------------------------------------------------

// List of supported scenes
#define SCENE_ROOM "scenes/room/render.cfg"
#define SCENE_SALA "scenes/sala/render.cfg"
#define SCENE_LUXBALL_HDR "scenes/luxball/render-hdr.cfg"
#define SCENE_LUXBALL "scenes/luxball/render.cfg"
#define SCENE_LUXBALL_SKY "scenes/luxball/render-sunset.cfg"

class LuxMarkApp : public QApplication {
	Q_OBJECT

public:
	MainWindow *mainWin;

	LuxMarkApp(int argc, char **argv);
	~LuxMarkApp();
	
	void Init();
	void Stop();

	void SetMode(LuxMarkAppMode m);
	void SetScene(const char *name);

	void HandleMouseMoveEvent(QGraphicsSceneMouseEvent *event);
	void HandleMousePressEvent(QGraphicsSceneMouseEvent *event);

private:
	static void EngineInitThreadImpl(LuxMarkApp *app);

	void InitRendering(LuxMarkAppMode m, const char *scnName);

	LuxMarkAppMode mode;
	const char *sceneName;

	HardwareTreeModel *hardwareTreeModel;

	boost::thread *engineInitThread;
	double renderingStartTime;
	bool engineInitDone;
	RenderSession *renderSession;

	QTimer *renderRefreshTimer;

	bool mouseButton0;
	bool mouseButton2;
	qreal mouseGrabLastX;
	qreal mouseGrabLastY;
	double lastMouseUpdate;

private slots:
	void RenderRefreshTimeout();
};

#endif // _LUXMARKAPP_H

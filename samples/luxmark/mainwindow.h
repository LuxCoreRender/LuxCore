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

#ifndef _MAINWINDOW_H
#define	_MAINWINDOW_H

#include <cstddef>
#include <string>

#include <boost/thread/mutex.hpp>

#include "ui_mainwindow.h"

#include <QGraphicsPixmapItem>

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow(QWidget *parent = NULL, Qt::WindowFlags flags = 0);
	~MainWindow();

	void PrintLog(const std::string &msg);

	void ShowLogo();
	bool IsShowingLogo() const;
	void ShowFrameBuffer(const float *frameBuffer,
		const unsigned int width, const unsigned int height);

private:
	Ui::MainWindow *ui;
	QGraphicsPixmapItem *luxLogo;

	QGraphicsPixmapItem *luxFrameBuffer;
	unsigned char *frameBuffer;
	unsigned int fbWidth, fbHeight;

	boost::mutex logMutex;

	QGraphicsScene *renderScene;

private slots:
	void exitApp();
	void showAbout();
};

extern MainWindow *LogWindow;

#define LM_LOG(a) { if (LogWindow) { std::stringstream _LM_LOG_LOCAL_SS; _LM_LOG_LOCAL_SS << a; LogWindow->PrintLog(_LM_LOG_LOCAL_SS.str().c_str()); } }

#endif	/* _MAINWINDOW_H */


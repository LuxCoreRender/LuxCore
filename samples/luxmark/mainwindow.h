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
#include "hardwaretree.h"

#include <QGraphicsPixmapItem>

class LuxMarkApp;

class LuxFrameBuffer : public QGraphicsPixmapItem {
public:
	LuxFrameBuffer(const QPixmap &pixmap);

	void SetLuxApp(LuxMarkApp *la) { luxApp = la; }

private:
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
	void mousePressEvent(QGraphicsSceneMouseEvent *event);

	LuxMarkApp *luxApp;
};

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow(QWidget *parent = NULL, Qt::WindowFlags flags = 0);
	~MainWindow();

	void ShowLogo();
	bool IsShowingLogo() const;
	void ShowFrameBuffer(const float *frameBuffer,
		const unsigned int width, const unsigned int height);

	void SetModeCheck(const int index);
	void SetSceneCheck(const int index);
	void UpdateScreenLabel(const char *msg, const bool valid);
	void SetHardwareTreeModel(HardwareTreeModel *treeModel);
	void SetLuxApp(LuxMarkApp *la) { luxFrameBuffer->SetLuxApp(la); }

	void Pause();

private:
	bool event(QEvent *event);

	Ui::MainWindow *ui;
	QGraphicsPixmapItem *luxLogo;
	LuxFrameBuffer *luxFrameBuffer;
	unsigned char *frameBuffer;
	unsigned int fbWidth, fbHeight;
	QGraphicsSimpleTextItem *authorLabel;
	QGraphicsSimpleTextItem *screenLabel;
	QGraphicsRectItem *screenLabelBack;
	QLabel *statusbarLabel;

	QGraphicsScene *renderScene;

private slots:
	void exitApp();
	void showAbout();

	void setLuxBallScene();
	void setLuxBallHDRScene();
	void setLuxBallSkyScene();
	void setSalaScene();

	void setBenchmarkGPUsMode();
	void setBenchmarkCPUsGPUsMode();
	void setBenchmarkCPUsMode();
	void setBenchmarkCustomMode();
	void setInteractiveMode();
	void setPauseMode();
};

#endif	/* _MAINWINDOW_H */

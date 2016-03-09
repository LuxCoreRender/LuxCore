/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#ifndef RENDERVIEW_H
#define RENDERVIEW_H

#include <algorithm>

#include <QGraphicsItemGroup>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QWidget>

#include "luxcorePlaceholder.h"

class RenderView : public QGraphicsView
{
	Q_OBJECT

public:
	RenderView(QWidget *parent = 0);
	~RenderView ();

	void setZoomEnabled (bool enabled = true) { zoomEnabled = enabled; }
	void setOverlayStatistics (bool value = true) { overlayStats = value; }
	void setShowAlpha (bool value = true) { showAlpha = value; }
	void setShowUserSamplingMap(bool value = true);

	void setUserSamplingPen(const bool addType);
	void setUserSamplingPenSize(const int size);
	void setUserSamplingPenSprayIntensity(const float i);
	void setUserSamplingMapOpacity(const float v);

	void applyUserSampling();
	void undoUserSampling();
	void resetUserSampling();

	void reload ();
	void setLogoMode ();
	int getZoomFactor ();
	int getWidth ();
	int getHeight ();
	void copyToClipboard ();
	float origh;
	float origw;

private:
	bool zoomEnabled;
	float zoomfactor;

	bool overlayStats;
	bool showAlpha;
	bool showUserSamplingMap;
	
	QGraphicsScene *renderscene;
	QGraphicsPixmapItem *luxlogo;
	QGraphicsPixmapItem *luxfb;

	// For user driven sampling
	float *userSamplingMap;
	QGraphicsPixmapItem *userSamplingPixmap;
	QImage *userSamplingMapImage;
	QGraphicsItemGroup *penItemGroup;
	bool userSamplingAddPenType;
	bool userSamplingPenPressed;
	int userSamplingPenX, userSamplingPenY;
	int userSamplingPenSize;
	float userSamplingPenSprayIntensity;
	float userSamplingMapOpacity;

	void addUserSamplingPen();
	void removeUserSamplingPen();

	void updateUserSamplingPixmap();
	void updateUserSamplingPixmap(int x, int y, int width, int height);

	void drawPenOnUserSamplingMap(const int x, const int y);

	void wheelEvent (QWheelEvent *event);
	void mousePressEvent (QMouseEvent *event);
	void mouseReleaseEvent (QMouseEvent *event);
	void mouseMoveEvent (QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);
    
    void choosePixmapAntialiasing();

signals:
	void viewChanged ();
};

#endif // RENDERVIEW_H

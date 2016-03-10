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

#ifndef HISTOGRAMVIEW_H
#define HISTOGRAMVIEW_H

#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWidget>
#include <QImage>
#include <QPixmap>

#include "luxcorePlaceholder.h"

class HistogramView : public QGraphicsView
{
	Q_OBJECT

public:
	HistogramView(QWidget *parent = 0);
	~HistogramView ();

	void Update();
	void SetEnabled(bool enabled);
	void ClearOption(int option);
	void SetOption(int option);
	void LogChanged(int value);
	
private:
	QWidget *frame;
	int m_Options;
	int w, h;
	bool m_IsEnabled;

	QGraphicsScene *scene;
	QGraphicsPixmapItem *pixmap;
	unsigned char *imagebuffer;

};

#endif // HISTOGRAMVIEW_H

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


//#include "api.h"

#include "histogramview.hxx"

HistogramView::HistogramView(QWidget *parent) : QGraphicsView(parent)
{
	frame = parent;
	scene = new QGraphicsScene();
	scene->setBackgroundBrush(QColor(0,0,0));
	pixmap = scene->addPixmap(QPixmap());
	pixmap->show ();
	imagebuffer = NULL;
//	w = -1;
//	h = -1;
	
	w = 400;
	h = 200;

	setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
	
	imagebuffer = new unsigned char[w * h * 3];

	m_Options=LUX_HISTOGRAM_RGB_ADD;
	setScene(scene);
}

HistogramView::~HistogramView ()
{
	if (imagebuffer != NULL)
		delete imagebuffer;
	delete pixmap;
	delete scene;
}

void HistogramView::Update()
{
	if (!isVisible())
		return;

	h = frame->height() - 1;
	w = frame->width() - 1;
	delete imagebuffer;
	imagebuffer = new unsigned char[w * h * 3];
	for (int i=0; i < (w*h*3); i++)
		imagebuffer[i] = 0;

	if (luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
		luxGetHistogramImage(imagebuffer, w, h, m_Options);
		pixmap->setPixmap(QPixmap::fromImage(QImage(imagebuffer, w, h, w*3, QImage::Format_RGB888)));
	}
	
	fitInView(0,0,w,h);
}

void HistogramView::SetOption(int option)
{
	option = 1 << option;
	m_Options &= ~(LUX_HISTOGRAM_RGB|LUX_HISTOGRAM_RGB_ADD|LUX_HISTOGRAM_RED|LUX_HISTOGRAM_GREEN|LUX_HISTOGRAM_BLUE|LUX_HISTOGRAM_VALUE);
	m_Options |= option;
	Update();
}

void HistogramView::LogChanged(int value)
{
	if (value == Qt::Checked)
		m_Options |=  LUX_HISTOGRAM_LOG;
	else
		m_Options &= ~LUX_HISTOGRAM_LOG;

	Update();
}

void HistogramView::SetEnabled(bool enabled)
{
	m_IsEnabled = enabled;
	if (luxStatistics("sceneIsReady") || luxStatistics("filmIsReady")) {
		// Update lux's film
		luxSetParameterValue(LUX_FILM, LUX_FILM_HISTOGRAM_ENABLED, enabled, 0);
	}
}

void HistogramView::ClearOption(int option)
{
	m_Options &= ~option;
}

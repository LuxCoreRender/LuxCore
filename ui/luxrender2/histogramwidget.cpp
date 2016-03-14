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

#include <QFont>

//#include "api.h"

#include "histogramwidget.hxx"
#include "ui_histogram.h"

HistogramWidget::HistogramWidget(QWidget *parent) : QWidget(parent), ui(new Ui::HistogramWidget)
{
	ui->setupUi(this);
	
	histogramView = new HistogramView(ui->frame_histogram);
	ui->histogramLayout->addWidget(histogramView, 0, 0, 1, 1);
	
	connect(ui->comboBox_histogramChannel, SIGNAL(currentIndexChanged(int)), this, SLOT(SetOption(int)));
	connect(ui->checkBox_histogramLog, SIGNAL(stateChanged(int)), this, SLOT(LogChanged(int)));
	
#if defined(__APPLE__)
	setFont(QFont  ("Lucida Grande", 11));
#endif

	histogramView->SetEnabled (true);
	histogramView->show ();
}

HistogramWidget::~HistogramWidget()
{
	delete histogramView;
}

void HistogramWidget::Update()
{
	histogramView->Update ();
}

void HistogramWidget::SetEnabled(bool enabled)
{
	histogramView->SetEnabled(enabled);
}

void HistogramWidget::SetOption(int option)
{
	histogramView->SetOption(option);
}

void HistogramWidget::LogChanged(int value)
{
	histogramView->LogChanged(value);
}

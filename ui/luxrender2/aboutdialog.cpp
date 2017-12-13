/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include "aboutdialog.hxx"
#include "ui_aboutdialog.h"
//#include "version.h"

AboutDialog::AboutDialog(QWidget *parent)
	: QDialog(parent, Qt::FramelessWindowHint), ui(new Ui::AboutDialog)
{
	ui->setupUi(this);

	imageview = new AboutImage(this);
	imageview->setFixedSize(550, 330);
	imageview->setFrameShape(QFrame::NoFrame);
	imageview->show();

	connect(imageview, SIGNAL(clicked()), this, SLOT(close()));
}

AboutDialog::~AboutDialog()
{
	delete imageview;
	delete ui;
}

AboutImage::AboutImage(QWidget *parent) : QGraphicsView(parent)
{
	scene = new QGraphicsScene;
	scene->setSceneRect(0, 0, 550, 330);
	this->setScene(scene);
	this->setBackgroundBrush(QImage(":/images/splash.png"));
	this->setCacheMode(QGraphicsView::CacheBackground);
  
  versionNo = new QGraphicsTextItem(QString("Version %1").arg(LUX_VERSION_STRING));
  versionNo->setDefaultTextColor(Qt::white);
  scene->addItem(versionNo);
  versionNo->setPos(220,250);
  
	authors = new QGraphicsTextItem(QString::fromUtf8("Jean-Philippe Grimaldi, David Bucciarelli, Asbjørn Heid, Tom Bech, Jean-François Romang, Doug Hammond, Jens Verwiebe, Vlad Miller, Jason Clarke, Simon Wendsche, Guillaume Bouchard, Matt Pharr, Greg Humphreys, Thomas De Bodt, Thomas Ludwig, David Washington, Abel Groenewolt, Paolo Ciccone, Pedro Alcaide, Michael Klemm, Stig Atle Steffensen, Ian Blew, Dan Farrel, Patrick Walz, Amir Mohammadkhani-Aminabadi, Mimmo Briganti, Liang Ma, Peter Bienkowski, Pascal Aebischer, Michael Gangolf, Anir-Ban Deb, Károly Zsolnai, Terrence Vergauwen, Ricardo Lipas Augusto, Campbell Barton, Seth Berrier, Wenzel Jakob"));
	authors->setDefaultTextColor(Qt::white);
	scene->addItem(authors);
	authors->setPos(540,297);

	scrolltimer = new QTimer();
	scrolltimer->start(10);
	connect(scrolltimer, SIGNAL(timeout()), SLOT(scrollTimeout()));
}

AboutImage::~AboutImage()
{
	scrolltimer->stop();
	delete scrolltimer;
	delete authors;
	delete scene;
}

void AboutImage::scrollTimeout()
{
	qreal xpos = authors->x();
	qreal endpos = xpos+authors->sceneBoundingRect().width();

	if (endpos < 0)
		xpos = 540.0f;
	else
		xpos = xpos - 1.0f;

	authors->setPos(xpos, authors->y());
}

void AboutImage::mousePressEvent(QMouseEvent* event)
{
	emit clicked();
}

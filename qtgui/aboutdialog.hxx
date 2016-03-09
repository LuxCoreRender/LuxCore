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

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>

#include "luxcorePlaceholder.h"

using namespace std;

namespace Ui
{
	class AboutDialog;
}

class AboutDialog : public QDialog
{
	Q_OBJECT

public:
	AboutDialog(QWidget *parent = 0);
	~AboutDialog();

private:
	QGraphicsView *imageview;
	Ui::AboutDialog *ui;
    
private slots:
//	void exitApp ();

};

class AboutImage : public QGraphicsView
{
	Q_OBJECT

public:
	AboutImage(QWidget *parent = 0);
	~AboutImage();

protected:
	void mousePressEvent(QMouseEvent* event);

private:
	QGraphicsScene *scene;
	QGraphicsTextItem *authors, *versionNo;
	QTimer *scrolltimer;

private slots:
	void scrollTimeout();

signals:
	void clicked();
};

#endif // ABOUTDIALOG_H

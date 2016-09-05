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

#ifndef ADVANCEDINFOWIDGET_H
#define ADVANCEDINFOWIDGET_H

#include <QShowEvent>
#include <QWidget>
#include <QString>
#include <QStringList>

#include "luxcorePlaceholder.h"
#include "guiutil.h"
#include "ui_advancedinfo.h"

namespace Ui
{
	class AdvancedInfoWidget;
}

class AdvancedInfoWidget : public QWidget
{
	Q_OBJECT

public:
	AdvancedInfoWidget(QWidget *parent = 0);
	~AdvancedInfoWidget();

	//void SetWidgetsEnabled(bool enabled);
    
	void updateWidgetValues();
	//void resetValues();
	//void resetFromFilm (bool useDefaults);

protected:
	virtual void showEvent(QShowEvent *event);

private:
	Ui::AdvancedInfoWidget *ui;

	int m_TM_kernel;

signals:
	void valuesChanged();

private slots:

};

#endif // AdvancedInfoWidget_H

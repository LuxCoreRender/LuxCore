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

#ifndef GAMMAWIDGET_H
#define GAMMAWIDGET_H

#include <QEvent>
#include <QString>
#include <QWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QVariant>

#include "luxcorePlaceholder.h"
#include "guiutil.h"
#include "mainwindow.hxx"
#include "ui_gamma.h"

#define TORGB_GAMMA_RANGE 5.0f

namespace Ui
{
	class GammaWidget;
}

class GammaWidget : public QWidget
{
	Q_OBJECT

public:
	GammaWidget(QWidget *parent = 0);
	~GammaWidget();

	//void SetWidgetsEnabled(bool enabled);
    
	void updateWidgetValues();
	void resetValues();
	void resetFromFilm (bool useDefaults);

	void SaveSettings( QString fName );
	void LoadSettings( QString fName );

	bool m_Gamma_enabled;
	double m_TORGB_gamma;

	bool m_CRF_enabled;
	QString m_CRF_file;
	
	void Update();

signals:
	void valuesChanged();

private:
	Ui::GammaWidget *ui;
	QString m_lastOpendir;
	void activateCRF();
	void deactivateCRF();

	void addPreset( QString listName, QString realName );

protected:
	void changeEvent(QEvent * event);

private slots:
	void gammaChanged (int value);
	void gammaChanged (double value);
	void CRFChanged (int value);
	void SetCRFPreset(QString sOption);
	void loadCRF();
};

#endif // GAMMAWIDGET_H

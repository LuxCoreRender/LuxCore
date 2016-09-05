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

#ifndef LIGHTGROUPWIDGET_H
#define LIGHTGROUPWIDGET_H

#include <QColor>
#include <QEvent>
#include <QString>
#include <QWidget>

#include "luxcorePlaceholder.h"

namespace Ui
{
	class LightGroupWidget;
}

class LightGroupWidget : public QWidget
{
	Q_OBJECT

public:
	LightGroupWidget(QWidget *parent = 0);
	~LightGroupWidget();

	QString GetTitle();
	int GetIndex();
	void SetIndex(int index);
	void UpdateWidgetValues();
	void ResetValues();
	void ResetValuesFromFilm(bool useDefaults);
	void SetWidgetsEnabled(bool enabled);
	void SetFromValues();

	void SaveSettings( QString fName );
	void LoadSettings( QString fName );

signals:
	void valuesChanged();

protected:
	void changeEvent(QEvent * event);

private:
	Ui::LightGroupWidget *ui;
	
	QString title;

	int m_Index;

	bool m_LG_enable;
	double m_LG_scale;
	bool m_LG_temperature_enabled;
	double m_LG_temperature;
	bool m_LG_rgb_enabled;
	double m_LG_scaleRed, m_LG_scaleGreen, m_LG_scaleBlue;
	double m_LG_scaleX, m_LG_scaleY;

	float SliderValToScale(int sliderval);
	int ScaleToSliderVal(float scale);

private slots:
	void rgbEnabledChanged(int);
	void bbEnabledChanged(int);

	void gainChanged(int value);
	void gainChanged(double value);
	void colortempChanged(int value);
	void colortempChanged(double value);
	void colorPicker();

   void colorSelected(const QColor & color);
};

#endif // LIGHTGROUPWIDGET_H

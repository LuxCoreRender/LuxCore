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

#ifndef LENSEFFECTSWIDGET_H
#define LENSEFFECTSWIDGET_H

#include <QEvent>
#include <QWidget>

#include <cmath>

#include <QFont>
#include <QFileDialog>
#include <QFileInfo>

#include "luxcorePlaceholder.h"
#include "mainwindow.hxx"
#include "ui_lenseffects.h"

#define BLOOMRADIUS_RANGE 1.0f
#define BLOOMWEIGHT_RANGE 1.0f

#define VIGNETTING_SCALE_RANGE 1.0f
#define ABERRATION_AMOUNT_RANGE .1f

#define GLARE_AMOUNT_RANGE 0.3f
#define GLARE_RADIUS_RANGE 0.2f
#define GLARE_BLADES_MIN 3
#define GLARE_BLADES_MAX 100
#define GLARE_THRESHOLD_RANGE 1.0f

namespace Ui
{
	class LensEffectsWidget;
}

class LensEffectsWidget : public QWidget
{
	Q_OBJECT

public:
	LensEffectsWidget(QWidget *parent = 0);
	~LensEffectsWidget();

	//void SetWidgetsEnabled(bool enabled);
    
	void updateWidgetValues();
	void resetValues();
	void resetFromFilm (bool useDefaults);

	bool m_Lenseffects_enabled;

signals:
	void valuesChanged();
	void forceUpdate();

protected:
	void changeEvent(QEvent * event);

private:
	Ui::LensEffectsWidget *ui;

	double m_bloomradius, m_bloomweight;

	bool m_Vignetting_Enabled;
	double m_Vignetting_Scale;

	bool m_Aberration_enabled;
	double m_Aberration_amount;

	double m_Glare_amount, m_Glare_radius;
	int m_Glare_blades;
	double m_Glare_threshold;
	bool m_Glare_map;
	QString m_Glare_pupil, m_Glare_lashes;
	QString m_lastOpendir;

private slots:
	// Lens effects slots
	void gaussianAmountChanged (int value);
	void gaussianAmountChanged (double value);
	void gaussianRadiusChanged (int value);
	void gaussianRadiusChanged (double value);
	void computeBloomLayer ();
	void deleteBloomLayer ();
	void vignettingAmountChanged (int value);
	void vignettingAmountChanged (double value);
	void vignettingEnabledChanged (int value);
	void caAmountChanged (int value);
	void caAmountChanged (double value);
	void caEnabledChanged (int value);
	void glareAmountChanged (int value);
	void glareAmountChanged (double value);
	void glareRadiusChanged (int value);
	void glareRadiusChanged (double value);
	void glareBladesChanged (int value);
	void glareThresholdSliderChanged (int value);
	void glareThresholdSpinBoxChanged (double value);
	void glareMapChanged(int);
	void glareBrowsePupilMap();
	void glareBrowseLashesMap();
	void computeGlareLayer ();
	void deleteGlareLayer ();
};

#endif // LENSEFFECTSWIDGET_H

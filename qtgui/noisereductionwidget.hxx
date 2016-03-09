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

#ifndef NOISEREDUCTIONWIDGET_H
#define NOISEREDUCTIONWIDGET_H

#include <QEvent>
#include <QWidget>

#define GREYC_AMPLITUDE_RANGE 200.0f
#define GREYC_SHARPNESS_RANGE 2.0f
#define GREYC_ANISOTROPY_RANGE 1.0f
#define GREYC_ALPHA_RANGE 12.0f
#define GREYC_SIGMA_RANGE 12.0f
#define GREYC_GAUSSPREC_RANGE 12.0f
#define GREYC_DL_RANGE 1.0f
#define GREYC_DA_RANGE 90.0f
#define GREYC_NB_ITER_RANGE 16.0f

namespace Ui
{
	class NoiseReductionWidget;
}

class NoiseReductionWidget : public QWidget
{
	Q_OBJECT

public:
	NoiseReductionWidget(QWidget *parent = 0);
	~NoiseReductionWidget();

	//void SetWidgetsEnabled(bool enabled);
    
	void updateWidgetValues();
	void resetValues();
	void resetFromFilm (bool useDefaults);

	bool m_Noisereduction_enabled;

	bool m_GREYC_enabled, m_GREYC_fast_approx;
	double m_GREYC_amplitude, m_GREYC_sharpness, m_GREYC_anisotropy, m_GREYC_alpha, m_GREYC_sigma, m_GREYC_gauss_prec, m_GREYC_dl, m_GREYC_da;
	double m_GREYC_nb_iter;
	int m_GREYC_interp;

	bool m_Chiu_enabled, m_Chiu_includecenter;
	double m_Chiu_radius;

signals:
	void valuesChanged();

protected:
	void changeEvent(QEvent * event);

private:
	Ui::NoiseReductionWidget *ui;

private slots:
	void regularizationEnabledChanged(int value);
	void fastApproximationEnabledChanged(int value);
	void chiuEnabledChanged(int value);
	void includeCenterEnabledChanged(int value);
	void iterationsChanged(int value);
	void iterationsChanged(double value);
	void amplitudeChanged(int value);
	void amplitudeChanged(double value);
	void precisionChanged(int value);
	void precisionChanged(double value);
	void alphaChanged(int value);
	void alphaChanged(double value);
	void sigmaChanged(int value);
	void sigmaChanged(double value);
	void sharpnessChanged(int value);
	void sharpnessChanged(double value);
	void anisoChanged(int value);
	void anisoChanged(double value);
	void spatialChanged(int value);
	void spatialChanged(double value);
	void angularChanged(int value);
	void angularChanged(double value);
	void setInterpolType(int value);
	void chiuRadiusChanged(int value);
	void chiuRadiusChanged(double value);
};

#endif // NOISEREDUCTIONWIDGET_H

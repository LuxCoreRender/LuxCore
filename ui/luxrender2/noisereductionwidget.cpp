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

#include <QFont>

//#include "api.h"
#include "luxcorePlaceholder.h"

#include "mainwindow.hxx"
#include "noisereductionwidget.hxx"
#include "ui_noisereduction.h"

NoiseReductionWidget::NoiseReductionWidget(QWidget *parent) : QWidget(parent), ui(new Ui::NoiseReductionWidget)
{
	ui->setupUi(this);
	
	connect(ui->checkBox_regularizationEnabled, SIGNAL(stateChanged(int)), this, SLOT(regularizationEnabledChanged(int)));
	connect(ui->checkBox_fastApproximation, SIGNAL(stateChanged(int)), this, SLOT(fastApproximationEnabledChanged(int)));
	connect(ui->checkBox_chiuEnabled, SIGNAL(stateChanged(int)), this, SLOT(chiuEnabledChanged(int)));
	connect(ui->checkBox_includeCenter, SIGNAL(stateChanged(int)), this, SLOT(includeCenterEnabledChanged(int)));
	connect(ui->slider_iterations, SIGNAL(valueChanged(int)), this, SLOT(iterationsChanged(int)));
	connect(ui->spinBox_iterations, SIGNAL(valueChanged(double)), this, SLOT(iterationsChanged(double)));
	connect(ui->slider_amplitude, SIGNAL(valueChanged(int)), this, SLOT(amplitudeChanged(int)));
	connect(ui->spinBox_amplitude, SIGNAL(valueChanged(double)), this, SLOT(amplitudeChanged(double)));
	connect(ui->slider_precision, SIGNAL(valueChanged(int)), this, SLOT(precisionChanged(int)));
	connect(ui->spinBox_precision, SIGNAL(valueChanged(double)), this, SLOT(precisionChanged(double)));
	connect(ui->slider_alpha, SIGNAL(valueChanged(int)), this, SLOT(alphaChanged(int)));
	connect(ui->spinBox_alpha, SIGNAL(valueChanged(double)), this, SLOT(alphaChanged(double)));
	connect(ui->slider_sigma, SIGNAL(valueChanged(int)), this, SLOT(sigmaChanged(int)));
	connect(ui->spinBox_sigma, SIGNAL(valueChanged(double)), this, SLOT(sigmaChanged(double)));
	connect(ui->slider_sharpness, SIGNAL(valueChanged(int)), this, SLOT(sharpnessChanged(int)));
	connect(ui->spinBox_sharpness, SIGNAL(valueChanged(double)), this, SLOT(sharpnessChanged(double)));
	connect(ui->slider_aniso, SIGNAL(valueChanged(int)), this, SLOT(anisoChanged(int)));
	connect(ui->spinBox_aniso, SIGNAL(valueChanged(double)), this, SLOT(anisoChanged(double)));
	connect(ui->slider_aniso, SIGNAL(valueChanged(int)), this, SLOT(anisoChanged(int)));
	connect(ui->spinBox_aniso, SIGNAL(valueChanged(double)), this, SLOT(anisoChanged(double)));
	connect(ui->slider_spatial, SIGNAL(valueChanged(int)), this, SLOT(spatialChanged(int)));
	connect(ui->spinBox_spatial, SIGNAL(valueChanged(double)), this, SLOT(spatialChanged(double)));
	connect(ui->slider_angular, SIGNAL(valueChanged(int)), this, SLOT(angularChanged(int)));
	connect(ui->spinBox_angular, SIGNAL(valueChanged(double)), this, SLOT(angularChanged(double)));
	connect(ui->comboBox_interpolType, SIGNAL(currentIndexChanged(int)), this, SLOT(setInterpolType(int)));
	connect(ui->slider_chiuRadius, SIGNAL(valueChanged(int)), this, SLOT(chiuRadiusChanged(int)));
	connect(ui->spinBox_chiuRadius, SIGNAL(valueChanged(double)), this, SLOT(chiuRadiusChanged(double)));
	
#if defined(__APPLE__)
	ui->tab_chiu->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_regularization->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_adv->setFont(QFont  ("Lucida Grande", 11));
	ui->groupBox_integration->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_filter->setFont(QFont  ("Lucida Grande", 11));
#endif
}

NoiseReductionWidget::~NoiseReductionWidget()
{
}

void NoiseReductionWidget::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::EnabledChange) {
		updateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED, m_GREYC_enabled && this->isEnabled());
		updateParam(LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED, m_Chiu_enabled && this->isEnabled());
		
		emit valuesChanged ();
	}
}

void NoiseReductionWidget::updateWidgetValues()
{
	// GREYC widgets
	updateWidgetValue(ui->slider_iterations, (int) m_GREYC_nb_iter );
	updateWidgetValue(ui->spinBox_iterations, m_GREYC_nb_iter);

	updateWidgetValue(ui->slider_amplitude, (int)((FLOAT_SLIDER_RES / GREYC_AMPLITUDE_RANGE) * m_GREYC_amplitude));
	updateWidgetValue(ui->spinBox_amplitude, m_GREYC_amplitude);

	updateWidgetValue(ui->slider_precision, (int)((FLOAT_SLIDER_RES / GREYC_GAUSSPREC_RANGE) * m_GREYC_gauss_prec));
	updateWidgetValue(ui->spinBox_precision, m_GREYC_gauss_prec);

	updateWidgetValue(ui->slider_alpha, (int)((FLOAT_SLIDER_RES / GREYC_ALPHA_RANGE) * m_GREYC_alpha));
	updateWidgetValue(ui->spinBox_alpha, m_GREYC_alpha);

	updateWidgetValue(ui->slider_sigma, (int)((FLOAT_SLIDER_RES / GREYC_SIGMA_RANGE) * m_GREYC_sigma));
	updateWidgetValue(ui->spinBox_sigma, m_GREYC_sigma);

	updateWidgetValue(ui->slider_sharpness, (int)((FLOAT_SLIDER_RES / GREYC_SHARPNESS_RANGE) * m_GREYC_sharpness));
	updateWidgetValue(ui->spinBox_sharpness, m_GREYC_sharpness);

	updateWidgetValue(ui->slider_aniso, (int)((FLOAT_SLIDER_RES / GREYC_ANISOTROPY_RANGE) * m_GREYC_anisotropy));
	updateWidgetValue(ui->spinBox_aniso, m_GREYC_anisotropy);

	updateWidgetValue(ui->slider_spatial, (int)((FLOAT_SLIDER_RES / GREYC_DL_RANGE) * m_GREYC_dl));
	updateWidgetValue(ui->spinBox_spatial, m_GREYC_dl);

	updateWidgetValue(ui->slider_angular, (int)((FLOAT_SLIDER_RES / GREYC_DA_RANGE) * m_GREYC_da));
	updateWidgetValue(ui->spinBox_angular, m_GREYC_da);

	ui->comboBox_interpolType->setCurrentIndex(m_GREYC_interp);

	ui->checkBox_regularizationEnabled->setChecked(m_GREYC_enabled);
	ui->checkBox_fastApproximation->setChecked(m_GREYC_fast_approx);
	ui->checkBox_chiuEnabled->setChecked(m_Chiu_enabled);
	ui->checkBox_includeCenter->setChecked(m_Chiu_includecenter);

	updateWidgetValue(ui->slider_chiuRadius,  (int)((FLOAT_SLIDER_RES / (CHIU_RADIUS_MAX-CHIU_RADIUS_MIN)) * (m_Chiu_radius-CHIU_RADIUS_MIN)) );
	updateWidgetValue(ui->spinBox_chiuRadius, m_Chiu_radius);
}

void NoiseReductionWidget::resetValues()
{
	m_GREYC_enabled = false;
	m_GREYC_fast_approx = true;
	m_GREYC_amplitude = 40.0;
	m_GREYC_sharpness = 0.8;
	m_GREYC_anisotropy = 0.2;
	m_GREYC_alpha = 0.8;
	m_GREYC_sigma = 1.1;
	m_GREYC_gauss_prec = 2.0;
	m_GREYC_dl = 0.8;
	m_GREYC_da = 30.0;
	m_GREYC_nb_iter = 1;
	m_GREYC_interp = 0;

	m_Chiu_enabled = false;
	m_Chiu_includecenter = false;
	m_Chiu_radius = 3;
}

void NoiseReductionWidget::resetFromFilm (bool useDefaults)
{
	double t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED);
	
	if (t != 0.0)
		m_GREYC_enabled = true;
	else
		m_GREYC_enabled = false;
	
	t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_FASTAPPROX);
	
	if(t != 0.0)
		m_GREYC_fast_approx = true;
	else
		m_GREYC_fast_approx = false;

	m_GREYC_amplitude = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE);
	m_GREYC_sharpness = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS);
	m_GREYC_anisotropy = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY);
	m_GREYC_alpha = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA);
	m_GREYC_sigma = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA);
	m_GREYC_gauss_prec = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC);
	m_GREYC_dl = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_DL);
	m_GREYC_da = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_DA);
	m_GREYC_nb_iter = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER);
	m_GREYC_interp = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_GREYC_INTERP);

	t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED);
	m_Chiu_enabled = t != 0.0;
	t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_CHIU_INCLUDECENTER);
	m_Chiu_includecenter = t != 0.0;
	m_Chiu_radius = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS);

	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED, m_GREYC_enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_FASTAPPROX, m_GREYC_fast_approx);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE, m_GREYC_amplitude);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS, m_GREYC_sharpness);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY, m_GREYC_anisotropy);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA, m_GREYC_alpha);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA, m_GREYC_sigma);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC, m_GREYC_gauss_prec);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_DL, m_GREYC_dl);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_DA, m_GREYC_da);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER, m_GREYC_nb_iter);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_GREYC_INTERP, m_GREYC_interp);

	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED, m_Chiu_enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_CHIU_INCLUDECENTER, m_Chiu_includecenter);
	luxSetParameterValue(LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS, m_Chiu_radius);
}

void NoiseReductionWidget::regularizationEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_GREYC_enabled = true;
	else
		m_GREYC_enabled = false;

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_ENABLED, m_GREYC_enabled);
	
	emit valuesChanged();
}

void NoiseReductionWidget::fastApproximationEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_GREYC_fast_approx = true;
	else
		m_GREYC_fast_approx = false;

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_FASTAPPROX, m_GREYC_fast_approx);
	
	emit valuesChanged();
}

void NoiseReductionWidget::chiuEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_Chiu_enabled = true;
	else
		m_Chiu_enabled = false;

	updateParam (LUX_FILM, LUX_FILM_NOISE_CHIU_ENABLED, m_Chiu_enabled);
	
	emit valuesChanged();
}

void NoiseReductionWidget::includeCenterEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_Chiu_includecenter = true;
	else
		m_Chiu_includecenter = false;

	updateParam (LUX_FILM, LUX_FILM_NOISE_CHIU_INCLUDECENTER, m_Chiu_includecenter);
	
	if (m_Chiu_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::iterationsChanged(int value)
{
	iterationsChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_NB_ITER_RANGE ) );
}

void NoiseReductionWidget::iterationsChanged(double value)
{
	m_GREYC_nb_iter = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_NB_ITER_RANGE) * m_GREYC_nb_iter);

	updateWidgetValue(ui->slider_iterations, sliderval);
	updateWidgetValue(ui->spinBox_iterations, m_GREYC_nb_iter);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_NBITER, m_GREYC_nb_iter);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::amplitudeChanged(int value)
{
	amplitudeChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_AMPLITUDE_RANGE ) );
}

void NoiseReductionWidget::amplitudeChanged(double value)
{
	m_GREYC_amplitude = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_AMPLITUDE_RANGE) * m_GREYC_amplitude);

	updateWidgetValue(ui->slider_amplitude, sliderval);
	updateWidgetValue(ui->spinBox_amplitude, m_GREYC_amplitude);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_AMPLITUDE, m_GREYC_amplitude);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::precisionChanged(int value)
{
	precisionChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_GAUSSPREC_RANGE ) );
}

void NoiseReductionWidget::precisionChanged(double value)
{
	m_GREYC_gauss_prec = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_GAUSSPREC_RANGE) * m_GREYC_gauss_prec);

	updateWidgetValue(ui->slider_precision, sliderval);
	updateWidgetValue(ui->spinBox_precision, m_GREYC_gauss_prec);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_GAUSSPREC, m_GREYC_gauss_prec);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::alphaChanged(int value)
{
	alphaChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_ALPHA_RANGE ) );
}

void NoiseReductionWidget::alphaChanged(double value)
{
	m_GREYC_alpha = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_ALPHA_RANGE) * m_GREYC_alpha);

	updateWidgetValue(ui->slider_alpha, sliderval);
	updateWidgetValue(ui->spinBox_alpha, m_GREYC_alpha);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_ALPHA, m_GREYC_alpha);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::sigmaChanged(int value)
{
	sigmaChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_SIGMA_RANGE ) );
}

void NoiseReductionWidget::sigmaChanged(double value)
{
	m_GREYC_sigma = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_SIGMA_RANGE) * m_GREYC_sigma);

	updateWidgetValue(ui->slider_sigma, sliderval);
	updateWidgetValue(ui->spinBox_sigma, m_GREYC_sigma);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_SIGMA, m_GREYC_sigma);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::sharpnessChanged(int value)
{
	sharpnessChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_SHARPNESS_RANGE ) );
}

void NoiseReductionWidget::sharpnessChanged(double value)
{
	m_GREYC_sharpness = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_SHARPNESS_RANGE) * m_GREYC_sharpness);

	updateWidgetValue(ui->slider_sharpness, sliderval);
	updateWidgetValue(ui->spinBox_sharpness, m_GREYC_sharpness);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_SHARPNESS, m_GREYC_sharpness);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::anisoChanged(int value)
{
	anisoChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_ANISOTROPY_RANGE ) );
}

void NoiseReductionWidget::anisoChanged(double value)
{
	m_GREYC_anisotropy = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_ANISOTROPY_RANGE) * m_GREYC_anisotropy);

	updateWidgetValue(ui->slider_aniso, sliderval);
	updateWidgetValue(ui->spinBox_aniso, m_GREYC_anisotropy);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_ANISOTROPY, m_GREYC_anisotropy);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::spatialChanged(int value)
{
	spatialChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_DL_RANGE ) );
}

void NoiseReductionWidget::spatialChanged(double value)
{
	m_GREYC_dl = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_DL_RANGE) * m_GREYC_dl);

	updateWidgetValue(ui->slider_spatial, sliderval);
	updateWidgetValue(ui->spinBox_spatial, m_GREYC_dl);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_DL, m_GREYC_dl);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::angularChanged(int value)
{
	angularChanged ( (double)value / ( FLOAT_SLIDER_RES / GREYC_DA_RANGE ) );
}

void NoiseReductionWidget::angularChanged(double value)
{
	m_GREYC_da = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GREYC_DA_RANGE) * m_GREYC_da);

	updateWidgetValue(ui->slider_angular, sliderval);
	updateWidgetValue(ui->spinBox_angular, m_GREYC_da);

	updateParam (LUX_FILM, LUX_FILM_NOISE_GREYC_DA, m_GREYC_da);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::setInterpolType(int value)
{
	m_GREYC_interp = value;
	
	updateParam(LUX_FILM, LUX_FILM_NOISE_GREYC_INTERP, m_GREYC_interp);

	if (m_GREYC_enabled)
		emit valuesChanged();
}

void NoiseReductionWidget::chiuRadiusChanged(int value)
{
	chiuRadiusChanged ( (double)value / (FLOAT_SLIDER_RES / (CHIU_RADIUS_MAX - CHIU_RADIUS_MIN)) + CHIU_RADIUS_MIN );
}

void NoiseReductionWidget::chiuRadiusChanged(double value)
{
	m_Chiu_radius = value;

	if ( m_Chiu_radius > CHIU_RADIUS_MAX )
		m_Chiu_radius = CHIU_RADIUS_MAX;
	else if ( m_Chiu_radius < CHIU_RADIUS_MIN )
		m_Chiu_radius = CHIU_RADIUS_MIN;

	int sliderval = (int)(( FLOAT_SLIDER_RES / (CHIU_RADIUS_MAX - CHIU_RADIUS_MIN) ) * (m_Chiu_radius - CHIU_RADIUS_MIN) );

	updateWidgetValue(ui->slider_chiuRadius, sliderval);
	updateWidgetValue(ui->spinBox_chiuRadius, m_Chiu_radius);

	updateParam (LUX_FILM, LUX_FILM_NOISE_CHIU_RADIUS, m_Chiu_radius);

	if (m_Chiu_enabled)
		emit valuesChanged();
}

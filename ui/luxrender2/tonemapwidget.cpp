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

#include <cmath>

#include <QFont>
#include <QSettings>

//#include "api.h"
#include "luxcorePlaceholder.h"

#include "guiutil.h"
#include "mainwindow.hxx"
#include "tonemapwidget.hxx"
#include "ui_tonemap.h"
//#include "tonemaps/falsecolors.h"
#include "luxrays/core/color/color.h"

static double sensitivity_presets[NUM_SENSITITIVITY_PRESETS] = {6400.0f, 5000.0f, 4000.0f, 3200.0f, 2500.0f, 2000.0f, 1600.0f, 1250.0f, 1000.0f, 800.0f, 640.0f, 500.0f, 400.0f, 320.0f, 250.0f, 200.0f, 160.0f, 125.0f, 100.0f, 80.0f, 64.0f, 50.0f, 40.0f, 32.0f, 25.0f, 20.0f};

static double exposure_presets[NUM_EXPOSURE_PRESETS] = {30.0f, 20.0f, 10.0f, 5.0f, 3.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.066f, 0.033f, 0.016f, 0.008f, 0.004f, 0.002f, 0.001f};

static double fstop_presets[NUM_FSTOP_PRESETS] = {128.0, 90.0, 64.0, 45.0, 32.0, 22.0, 16.0, 11.0, 8.0, 5.6, 4.0, 2.8, 2.0, 1.4, 1.0, 0.7, 0.5};

static bool EqualDouble(const double a, const double b)
{
	return (std::fabs(a-b) < 1e-7f);
}

using namespace std;

ToneMapWidget::ToneMapWidget(QWidget *parent) : QWidget(parent), ui(new Ui::ToneMapWidget)
{
	ui->setupUi(this);
	
	// Tonemap page - only show Reinhard frame by default
	
	ui->frame_toneMapLinear->hide();
	ui->frame_toneMapContrast->hide();
	ui->frame_toneMapFalse->hide();

	connect(ui->comboBox_kernel, SIGNAL(currentIndexChanged(int)), this, SLOT(setTonemapKernel(int)));
	connect(ui->comboBox_clampMethod, SIGNAL(currentIndexChanged(int)), this, SLOT(setClampMethod(int)));

	// Reinhard
	connect(ui->slider_prescale, SIGNAL(valueChanged(int)), this, SLOT(prescaleChanged(int)));
	connect(ui->spinBox_prescale, SIGNAL(valueChanged(double)), this, SLOT(prescaleChanged(double)));
	connect(ui->slider_postscale, SIGNAL(valueChanged(int)), this, SLOT(postscaleChanged(int)));
	connect(ui->spinBox_postscale, SIGNAL(valueChanged(double)), this, SLOT(postscaleChanged(double)));
	connect(ui->slider_burn, SIGNAL(valueChanged(int)), this, SLOT(burnChanged(int)));
	connect(ui->spinBox_burn, SIGNAL(valueChanged(double)), this, SLOT(burnChanged(double)));

	// Linear
	connect(ui->slider_sensitivity, SIGNAL(valueChanged(int)), this, SLOT(sensitivityChanged(int)));
	connect(ui->comboBox_SensitivityPreset, SIGNAL(currentIndexChanged(int)), this, SLOT(setSensitivityPreset(int))); // New feature
	connect(ui->spinBox_sensitivity, SIGNAL(valueChanged(double)), this, SLOT(sensitivityChanged(double)));
	connect(ui->slider_exposure, SIGNAL(valueChanged(int)), this, SLOT(exposureChanged(int)));
	connect(ui->comboBox_ExposurePreset, SIGNAL(currentIndexChanged(int)), this, SLOT(setExposurePreset(int))); // New feature
	connect(ui->spinBox_exposure, SIGNAL(valueChanged(double)), this, SLOT(exposureChanged(double)));
	connect(ui->slider_fstop, SIGNAL(valueChanged(int)), this, SLOT(fstopChanged(int)));
	connect(ui->comboBox_FStopPreset, SIGNAL(currentIndexChanged(int)), this, SLOT(setFStopPreset(int))); // New feature
	connect(ui->spinBox_fstop, SIGNAL(valueChanged(double)), this, SLOT(fstopChanged(double)));
	connect(ui->slider_gamma_linear, SIGNAL(valueChanged(int)), this, SLOT(gammaLinearChanged(int)));
	connect(ui->spinBox_gamma_linear, SIGNAL(valueChanged(double)), this, SLOT(gammaLinearChanged(double)));
	connect(ui->button_linearEstimate, SIGNAL(clicked()), this, SLOT(estimateLinear()));

#if defined(Q_WS_X11) // On Linux the pulldowns look oversized, changed individually to not break other OS balanced outcome
	ui->comboBox_SensitivityPreset->setFont(QFont  ("Sans Serif", 10));
	ui->comboBox_ExposurePreset->setFont(QFont  ("Sans Serif", 10));
	ui->comboBox_FStopPreset->setFont(QFont  ("Sans Serif", 10));
#endif
	// Max contrast
	connect(ui->slider_ywa, SIGNAL(valueChanged(int)), this, SLOT(ywaChanged(int)));
	connect(ui->spinBox_ywa, SIGNAL(valueChanged(double)), this, SLOT(ywaChanged(double)));

	// False Colors
	connect(ui->lineEdit_false_valuemaxsat, SIGNAL(returnPressed()), this, SLOT(falsemaxSatChanged()));
	connect(ui->lineEdit_false_valueminsat, SIGNAL(returnPressed()), this, SLOT(falseminSatChanged()));
	connect(ui->comboBox_false_Method, SIGNAL(currentIndexChanged(int)), this, SLOT(setFalseMethod(int)));
	connect(ui->comboBox_false_colorscale, SIGNAL(currentIndexChanged(int)), this, SLOT(setFalseColorScale(int)));
	connect(ui->lineEdit_false_legendeTest, SIGNAL(returnPressed()), this, SLOT(legendeChanged()));
	
#if defined(__APPLE__)
	ui->frame_toneMapReinhard->setFont(QFont  ("Lucida Grande", 11));
	ui->frame_toneMapLinear->setFont(QFont  ("Lucida Grande", 11));
	ui->frame_toneMapContrast->setFont(QFont  ("Lucida Grande", 11));
	ui->frame_toneMapFalse->setFont(QFont  ("Lucida Grande", 11));
#endif
}

ToneMapWidget::~ToneMapWidget() { }

void ToneMapWidget::updateWidgetValues()
{
	// Tonemap kernel selection
	setTonemapKernel (m_TM_kernel);

	// Reinhard widgets
	
	updateWidgetValue(ui->slider_prescale, (int)((FLOAT_SLIDER_RES / TM_REINHARD_PRESCALE_RANGE) * m_TM_reinhard_prescale) );
	updateWidgetValue(ui->spinBox_prescale, m_TM_reinhard_prescale);

	updateWidgetValue(ui->slider_postscale, (int)((FLOAT_SLIDER_RES / TM_REINHARD_POSTSCALE_RANGE) * (m_TM_reinhard_postscale)));
	updateWidgetValue(ui->spinBox_postscale, m_TM_reinhard_postscale);

	updateWidgetValue(ui->slider_burn, (int)((FLOAT_SLIDER_RES / TM_REINHARD_BURN_RANGE) * m_TM_reinhard_burn));
	updateWidgetValue(ui->spinBox_burn, m_TM_reinhard_burn);

	// Linear widgets
	updateWidgetValue(ui->slider_exposure, ValueToLogSliderVal(m_TM_linear_exposure, TM_LINEAR_EXPOSURE_LOG_MIN, TM_LINEAR_EXPOSURE_LOG_MAX, FLOAT_SLIDER_RES) );
	updateWidgetValue(ui->spinBox_exposure, m_TM_linear_exposure);

	updateWidgetValue(ui->slider_sensitivity, (int)((FLOAT_SLIDER_RES / TM_LINEAR_SENSITIVITY_RANGE) * m_TM_linear_sensitivity) );
	updateWidgetValue(ui->spinBox_sensitivity, m_TM_linear_sensitivity);

	updateWidgetValue(ui->slider_fstop, (int)((FLOAT_SLIDER_RES / TM_LINEAR_FSTOP_RANGE) * m_TM_linear_fstop));
	updateWidgetValue(ui->spinBox_fstop, m_TM_linear_fstop);

	updateWidgetValue(ui->slider_gamma_linear, (int)((FLOAT_SLIDER_RES / TM_LINEAR_GAMMA_RANGE) * m_TM_linear_gamma));
	updateWidgetValue(ui->spinBox_gamma_linear, m_TM_linear_gamma);

	// Contrast widgets
	updateWidgetValue(ui->slider_ywa, ValueToLogSliderVal(m_TM_contrast_ywa, TM_CONTRAST_YWA_LOG_MIN, TM_CONTRAST_YWA_LOG_MAX, FLOAT_SLIDER_RES) );
	updateWidgetValue(ui->spinBox_ywa, m_TM_contrast_ywa);

	// False Colors widgets
	updateWidgetValue(ui->label_false_valuemax, m_TM_FalseMax);
	updateWidgetValue(ui->label_false_valuemin, m_TM_FalseMin);
	updateWidgetValue(ui->lineEdit_false_valuemaxsat, m_TM_FalseMaxSat);
	updateWidgetValue(ui->lineEdit_false_valueminsat, m_TM_FalseMinSat);
	updateWidgetValue(ui->label_false_valueaverageluminance, m_TM_FalseAvgLum);
	updateWidgetValue(ui->label_false_valueaverageluminousemittance, m_TM_FalseAvgEmi);
	updateWidgetValue(ui->lineEdit_false_legendeTest, m_TM_FalseLegendeNumber);
}

void ToneMapWidget::updateFilmValues()
{
	m_TM_FalseMax = retrieveParam(false, LUX_FILM, LUX_FILM_TM_FALSE_MAX);
	updateWidgetValue(ui->label_false_valuemax, m_TM_FalseMax);
	m_TM_FalseMin = retrieveParam(false, LUX_FILM, LUX_FILM_TM_FALSE_MIN);
	updateWidgetValue(ui->label_false_valuemin, m_TM_FalseMin);
	m_TM_FalseAvgLum = luxGetFloatAttribute("film", "averageLuminance");
	updateWidgetValue(ui->label_false_valueaverageluminance, m_TM_FalseAvgLum);
	updateWidgetValue(ui->label_false_valueaverageluminousemittance, m_TM_FalseAvgEmi);
}

void ToneMapWidget::setSensitivityPreset(int choice)
{
	ui->comboBox_SensitivityPreset->blockSignals(true);
	ui->comboBox_SensitivityPreset->setCurrentIndex(choice);
	ui->comboBox_SensitivityPreset->blockSignals(false);
	
	// first choice is "User-defined"
	if (choice < 1)
		return;
	
	m_TM_linear_sensitivity = sensitivity_presets[choice-1];
	
	// Update values in film trough API
	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY, m_TM_linear_sensitivity);
	
	updateWidgetValues ();
	
	emit valuesChanged();
}

void ToneMapWidget::setExposurePreset(int choice)
{
	ui->comboBox_ExposurePreset->blockSignals(true);
	ui->comboBox_ExposurePreset->setCurrentIndex(choice);
	ui->comboBox_ExposurePreset->blockSignals(false);
	
	// first choice is "User-defined"
	if (choice < 1)
		return;
	
	m_TM_linear_exposure = exposure_presets[choice-1];
	
	// Update values in film trough API
	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE, m_TM_linear_exposure);
	updateWidgetValues ();
	
	emit valuesChanged();
}

void ToneMapWidget::setFStopPreset(int choice)
{	
	ui->comboBox_FStopPreset->blockSignals(true);
	ui->comboBox_FStopPreset->setCurrentIndex(choice);
	ui->comboBox_FStopPreset->blockSignals(false);

	// first choice is "User-defined"
	if (choice < 1)
		return;
	
	m_TM_linear_fstop = fstop_presets[choice-1];
	
	// Update values in film trough API
	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP, m_TM_linear_fstop);
	
	updateWidgetValues ();
	
	emit valuesChanged();
}

void ToneMapWidget::resetValues()
{
	m_TM_kernel = 0;
	m_clamp_method = 0;

	m_TM_reinhard_prescale = 1.0;
	m_TM_reinhard_postscale = 1.0;
	m_TM_reinhard_burn = 6.0;

	m_TM_linear_exposure = 1.0;
	m_TM_linear_sensitivity = 50.0;
	m_TM_linear_fstop = 2.8;
	m_TM_linear_gamma = 1.0;

	m_TM_contrast_ywa = 0.1;

	m_TM_FalseMax = 0.;
	m_TM_FalseMin = 0.;
	m_TM_FalseMaxSat = 0.;
	m_TM_FalseMinSat = 0.;
	m_TM_FalseAvgLum = 0.;
	m_TM_FalseAvgEmi = 0.;
	m_false_method = 0;
	m_false_colorscale = 0;
	m_TM_FalseLegendeNumber = 5;
}

int ToneMapWidget::sensitivityToPreset(double value)
{
	int i = 0;
	while (i < NUM_SENSITITIVITY_PRESETS) {
		if (EqualDouble(value, sensitivity_presets[i]))
			break;
		i++;
	}
	if (i == NUM_SENSITITIVITY_PRESETS)
		i = -1;

	return i+1;
}

int ToneMapWidget::exposureToPreset(double value)
{
	int i = 0;
	while (i < NUM_EXPOSURE_PRESETS) {
		if (EqualDouble(value, exposure_presets[i]))
			break;
		i++;
	}
	if (i == NUM_EXPOSURE_PRESETS)
		i = -1;

	return i+1;
}

int ToneMapWidget::fstopToPreset(double value)
{
	int i = 0;
	while (i < NUM_FSTOP_PRESETS) {
		if (EqualDouble(value, fstop_presets[i]))
			break;
		i++;
	}
	if (i == NUM_FSTOP_PRESETS)
		i = -1;

	return i+1;
}

void ToneMapWidget::resetFromFilm (bool useDefaults)
{
	m_TM_kernel = (int)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_TONEMAPKERNEL);
	m_clamp_method = (int)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LDR_CLAMP_METHOD);

	m_TM_reinhard_prescale = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE);
	m_TM_reinhard_postscale = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE);
	m_TM_reinhard_burn = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_REINHARD_BURN);

	m_TM_linear_exposure = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE);
	m_TM_linear_sensitivity = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY);
	m_TM_linear_fstop = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP);
	m_TM_linear_gamma = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA);

	m_TM_contrast_ywa = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TM_CONTRAST_YWA);

	m_TM_FalseMax = retrieveParam(useDefaults, LUX_FILM, LUX_FILM_TM_FALSE_MAX);
	m_TM_FalseMin = retrieveParam(useDefaults, LUX_FILM, LUX_FILM_TM_FALSE_MIN);
	m_TM_FalseMaxSat = retrieveParam(useDefaults, LUX_FILM, LUX_FILM_TM_FALSE_MAXSAT);
	m_TM_FalseMinSat = retrieveParam(useDefaults, LUX_FILM, LUX_FILM_TM_FALSE_MINSAT);
	m_false_method = (int)retrieveParam(useDefaults, LUX_FILM, LUX_FILM_TM_FALSE_METHOD);
	m_false_colorscale = (int)retrieveParam(useDefaults, LUX_FILM, LUX_FILM_TM_FALSE_COLORSCALE);

	SetFromValues();
}

void ToneMapWidget::SetFromValues ()
{
	ui->comboBox_SensitivityPreset->setCurrentIndex(sensitivityToPreset(m_TM_linear_sensitivity));
	ui->comboBox_ExposurePreset->setCurrentIndex(exposureToPreset(m_TM_linear_exposure));
	ui->comboBox_FStopPreset->setCurrentIndex(fstopToPreset(m_TM_linear_fstop));
	ui->comboBox_clampMethod->setCurrentIndex(m_clamp_method);

	luxSetParameterValue(LUX_FILM, LUX_FILM_LDR_CLAMP_METHOD, (double)m_clamp_method);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE, m_TM_reinhard_prescale);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE, m_TM_reinhard_postscale);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_REINHARD_BURN, m_TM_reinhard_burn);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE, m_TM_linear_exposure);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY, m_TM_linear_sensitivity);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP, m_TM_linear_fstop);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA, m_TM_linear_gamma);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_CONTRAST_YWA, m_TM_contrast_ywa);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_FALSE_MAXSAT, m_TM_FalseMaxSat);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TM_FALSE_MINSAT, m_TM_FalseMinSat);
	ui->comboBox_false_Method->setCurrentIndex(m_false_method);
	ui->comboBox_false_colorscale->setCurrentIndex(m_false_colorscale);
}


void ToneMapWidget::setTonemapKernel(int choice)
{
	ui->comboBox_kernel->setCurrentIndex(choice);
	updateParam(LUX_FILM, LUX_FILM_TM_TONEMAPKERNEL, (double)choice);
	m_TM_kernel = choice;
	switch (choice) {
		case 0:
			// Reinhard
			ui->frame_toneMapReinhard->show();
			ui->frame_toneMapLinear->hide();
			ui->frame_toneMapContrast->hide();
			ui->frame_toneMapFalse->hide();
			break;
		case 1:
			// Linear
			ui->frame_toneMapReinhard->hide();
			ui->frame_toneMapLinear->show();
			ui->frame_toneMapContrast->hide();
			ui->frame_toneMapFalse->hide();
			break;
		case 2:
			// Contrast
			ui->frame_toneMapReinhard->hide();
			ui->frame_toneMapLinear->hide();
			ui->frame_toneMapContrast->show();
			ui->frame_toneMapFalse->hide();
			break;
		case 3:
			// MaxWhite
			ui->frame_toneMapReinhard->hide();
			ui->frame_toneMapLinear->hide();
			ui->frame_toneMapContrast->hide();
			ui->frame_toneMapFalse->hide();
			break;
		case 4:
			// Auto Linear
			ui->frame_toneMapReinhard->hide();
			ui->frame_toneMapLinear->hide();
			ui->frame_toneMapContrast->hide();
			ui->frame_toneMapFalse->hide();
			break;
		case 5:
			// False Colors
			initFalseColor();

			ui->frame_toneMapReinhard->hide();
			ui->frame_toneMapLinear->hide();
			ui->frame_toneMapContrast->hide();
			ui->frame_toneMapFalse->show();
			break;
		default:
			ui->frame_toneMapReinhard->hide();
			ui->frame_toneMapLinear->hide();
			ui->frame_toneMapContrast->hide();
			ui->frame_toneMapFalse->hide();
			break;
	}

	emit valuesChanged();
}

void ToneMapWidget::setClampMethod(int choice)
{
	ui->comboBox_clampMethod->setCurrentIndex(choice);
	// index -> method
	m_clamp_method = choice;
	updateParam(LUX_FILM, LUX_FILM_LDR_CLAMP_METHOD, (double)m_clamp_method);

	emit valuesChanged();
}

void ToneMapWidget::prescaleChanged (int value)
{
	prescaleChanged ((double)value / ( FLOAT_SLIDER_RES / TM_REINHARD_PRESCALE_RANGE ));
}

void ToneMapWidget::prescaleChanged (double value)
{
	m_TM_reinhard_prescale = value;
	int sliderval = (int)((FLOAT_SLIDER_RES / TM_REINHARD_PRESCALE_RANGE) * m_TM_reinhard_prescale);

	updateWidgetValue(ui->slider_prescale, sliderval);
	updateWidgetValue(ui->spinBox_prescale, m_TM_reinhard_prescale);

	updateParam (LUX_FILM, LUX_FILM_TM_REINHARD_PRESCALE, m_TM_reinhard_prescale);

	emit valuesChanged();
}

void ToneMapWidget::postscaleChanged (int value)
{
	postscaleChanged ((double)value / ( FLOAT_SLIDER_RES / TM_REINHARD_POSTSCALE_RANGE ));
}

void ToneMapWidget::postscaleChanged (double value)
{
	m_TM_reinhard_postscale = value;
	int sliderval = (int)((FLOAT_SLIDER_RES / TM_REINHARD_POSTSCALE_RANGE) * m_TM_reinhard_postscale);

	updateWidgetValue(ui->slider_postscale, sliderval);
	updateWidgetValue(ui->spinBox_postscale, m_TM_reinhard_postscale);

	updateParam (LUX_FILM, LUX_FILM_TM_REINHARD_POSTSCALE, m_TM_reinhard_postscale);

	emit valuesChanged();
}

void ToneMapWidget::burnChanged (int value)
{
	burnChanged ((double)value / ( FLOAT_SLIDER_RES / TM_REINHARD_BURN_RANGE ) );
}

void ToneMapWidget::burnChanged (double value)
{
	m_TM_reinhard_burn = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TM_REINHARD_BURN_RANGE) * m_TM_reinhard_burn);

	updateWidgetValue(ui->slider_burn, sliderval);
	updateWidgetValue(ui->spinBox_burn, m_TM_reinhard_burn);

	updateParam(LUX_FILM, LUX_FILM_TM_REINHARD_BURN, m_TM_reinhard_burn);

	emit valuesChanged();
}

void ToneMapWidget::sensitivityChanged (int value)
{
	sensitivityChanged ((double)value / ( FLOAT_SLIDER_RES / TM_LINEAR_SENSITIVITY_RANGE ) );
}

void ToneMapWidget::sensitivityChanged (double value)
{
	m_TM_linear_sensitivity = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TM_LINEAR_SENSITIVITY_RANGE) * m_TM_linear_sensitivity);

	updateWidgetValue(ui->slider_sensitivity, sliderval);
	updateWidgetValue(ui->spinBox_sensitivity, m_TM_linear_sensitivity);
	// Don't trigger yet another event
	ui->comboBox_SensitivityPreset->blockSignals(true);
	ui->comboBox_SensitivityPreset->setCurrentIndex(sensitivityToPreset(m_TM_linear_sensitivity));
	ui->comboBox_SensitivityPreset->blockSignals(false);

	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_SENSITIVITY, m_TM_linear_sensitivity);

	emit valuesChanged();
}

void ToneMapWidget::exposureChanged (int value)
{
	exposureChanged ( LogSliderValToValue(value, TM_LINEAR_EXPOSURE_LOG_MIN, TM_LINEAR_EXPOSURE_LOG_MAX, FLOAT_SLIDER_RES) );
}

void ToneMapWidget::exposureChanged (double value)
{
	m_TM_linear_exposure = value;

	int sliderval = ValueToLogSliderVal (m_TM_linear_exposure, TM_LINEAR_EXPOSURE_LOG_MIN, TM_LINEAR_EXPOSURE_LOG_MAX, FLOAT_SLIDER_RES);

	updateWidgetValue(ui->slider_exposure, sliderval);
	updateWidgetValue(ui->spinBox_exposure, m_TM_linear_exposure);
	// Don't trigger yet another event
	ui->comboBox_ExposurePreset->blockSignals(true);
	ui->comboBox_ExposurePreset->setCurrentIndex(exposureToPreset(m_TM_linear_exposure));
	ui->comboBox_ExposurePreset->blockSignals(false);

	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_EXPOSURE, m_TM_linear_exposure);

	emit valuesChanged();
}

void ToneMapWidget::fstopChanged (int value)
{
	fstopChanged ( (double)value / ( FLOAT_SLIDER_RES / TM_LINEAR_FSTOP_RANGE ) );
}

void ToneMapWidget::fstopChanged (double value)
{
	m_TM_linear_fstop = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TM_LINEAR_FSTOP_RANGE) * m_TM_linear_fstop);

	updateWidgetValue(ui->slider_fstop, sliderval);
	updateWidgetValue(ui->spinBox_fstop, m_TM_linear_fstop);
	// Don't trigger yet another event
	ui->comboBox_FStopPreset->blockSignals(true);
	ui->comboBox_FStopPreset->setCurrentIndex(fstopToPreset(m_TM_linear_fstop));
	ui->comboBox_FStopPreset->blockSignals(false);

	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_FSTOP, m_TM_linear_fstop);

	emit valuesChanged();
}

void ToneMapWidget::gammaLinearChanged (int value)
{
	gammaLinearChanged ( (double)value / ( FLOAT_SLIDER_RES / TM_LINEAR_GAMMA_RANGE ) );
}

void ToneMapWidget::gammaLinearChanged (double value)
{
	m_TM_linear_gamma = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TM_LINEAR_GAMMA_RANGE) * m_TM_linear_gamma);

	updateWidgetValue(ui->slider_gamma_linear, sliderval);
	updateWidgetValue(ui->spinBox_gamma_linear, m_TM_linear_gamma);

	updateParam (LUX_FILM, LUX_FILM_TM_LINEAR_GAMMA, m_TM_linear_gamma);

	emit valuesChanged();
}

void ToneMapWidget::estimateLinear ()
{
	// estimate linear tonemapping parameters
	const float gamma = retrieveParam(false, LUX_FILM, LUX_FILM_TORGB_GAMMA);
	const float Y =  luxGetFloatAttribute("film", "averageLuminance");

	const double gfactor = std::pow(118.f / 255.f, gamma);

	// this is the target factor that autolinear would calculate
	// we try to get as close as possible to this
	const double targetFactor = (1.25 / Y * gfactor);

	const int numFStopSettings = 10;
	static double fStopSettings[10] = { 2.8, 4.0, 5.6, 8.0, 11.0, 16.0, 22.0, 32.0, 45.0, 64.0 };
	int fStopIdx = 2;

	const int numExposureSettings = 17;
	static double exposureSettings[17] = { 1.0/1000.0, 1.0/500.0, 1.0/250.0, 1.0/125.0, 1.0/60.0, 1.0/30.0, 1.0/15.0, 1.0/8.0, 1.0/4.0, 1.0/2.0, 1.0, 2.0, 3.0, 5.0, 10.0, 20.0, 30.0 };
	int exposureIdx = 3;

	const int numSensitivitySettings = 8;
	static double sensitivitySettings[8] = { 50.0, 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0 };
	int sensitivityIdx = 2;

	while (true) {
		//double exposure = exposureSettings[exposureIdx];
		double sensitivity = sensitivitySettings[sensitivityIdx];
		double fstop = fStopSettings[fStopIdx];

		// factor = exposure / (fstop * fstop) * sensitivity / 10.f * powf(118.f / 255.f, gamma);
		double targetExposure = targetFactor * (fstop * fstop) * 10.0 / (sensitivity * gfactor);
		
		if (targetExposure < exposureSettings[0]) {
			// want shorter exposure, ie too bright
			if (sensitivityIdx <= 0) {
				sensitivityIdx = 0;
				exposureIdx = 0;
				
				fStopIdx++;
				if (fStopIdx >= numFStopSettings) {
					fStopIdx = numFStopSettings-1;
					break;
				}
			} else
				sensitivityIdx--;
		} else if (targetExposure > exposureSettings[numExposureSettings-1]) {
			// want longer exposure, ie too dark
			if (sensitivityIdx >= numSensitivitySettings-1) {
				sensitivityIdx = numSensitivitySettings-1;
				exposureIdx = numExposureSettings-1;
				
				fStopIdx--;
				if (fStopIdx < 0) {
					fStopIdx = 0;
					break;
				}
			} else
				sensitivityIdx++;
		} else {
			exposureIdx = static_cast<int>(std::upper_bound(exposureSettings, exposureSettings+(numExposureSettings-1), targetExposure) - exposureSettings);
			if (targetExposure < exposureSettings[exposureIdx])
				exposureIdx--;
			break;
		}
	}

	gammaLinearChanged(gamma);
	sensitivityChanged(sensitivitySettings[sensitivityIdx]);
	exposureChanged(exposureSettings[exposureIdx]);
	fstopChanged(fStopSettings[fStopIdx]);
}

void ToneMapWidget::ywaChanged (int value)
{
	ywaChanged (LogSliderValToValue(value, TM_CONTRAST_YWA_LOG_MIN, TM_CONTRAST_YWA_LOG_MAX, FLOAT_SLIDER_RES) );
}

void ToneMapWidget::ywaChanged (double value)
{
	m_TM_contrast_ywa = value;

	int sliderval = ValueToLogSliderVal (m_TM_contrast_ywa, TM_CONTRAST_YWA_LOG_MIN, TM_CONTRAST_YWA_LOG_MAX, FLOAT_SLIDER_RES);

	updateWidgetValue(ui->slider_ywa, sliderval);
	updateWidgetValue(ui->spinBox_ywa, m_TM_contrast_ywa);

	updateParam (LUX_FILM, LUX_FILM_TM_CONTRAST_YWA, m_TM_contrast_ywa);

	emit valuesChanged();
}

void ToneMapWidget::falsemaxSatChanged()
{
	m_TM_FalseMaxSat = ui->lineEdit_false_valuemaxsat->text().toDouble();
	updateParam(LUX_FILM, LUX_FILM_TM_FALSE_MAXSAT, m_TM_FalseMaxSat);

	falseLegend();
	emit valuesChanged();
}

void ToneMapWidget::falsemaxSatChanged(double value)
{
	m_TM_FalseMaxSat = value;
	updateParam(LUX_FILM, LUX_FILM_TM_FALSE_MAXSAT, m_TM_FalseMaxSat);
	updateWidgetValue(ui->lineEdit_false_valuemaxsat, value);

	emit valuesChanged();
}

void ToneMapWidget::falseminSatChanged()
{
	m_TM_FalseMinSat = ui->lineEdit_false_valueminsat->text().toDouble();
	updateParam(LUX_FILM, LUX_FILM_TM_FALSE_MINSAT, m_TM_FalseMinSat);

	falseLegend();
	emit valuesChanged();
}

void ToneMapWidget::falseminSatChanged(double value)
{
	m_TM_FalseMinSat = value;
	updateParam(LUX_FILM, LUX_FILM_TM_FALSE_MINSAT, m_TM_FalseMinSat);
	updateWidgetValue(ui->lineEdit_false_valueminsat, value);

	emit valuesChanged();
}

void ToneMapWidget::falsemaxChanged(double value)
{
	m_TM_FalseMax = value;
	updateWidgetValue(ui->label_false_valuemax, value);

	emit valuesChanged();
}

void ToneMapWidget::falseminChanged(double value)
{
	m_TM_FalseMin = value;
	updateWidgetValue(ui->label_false_valuemin, value);

	emit valuesChanged();
}

void ToneMapWidget::initFalseColor()
{
	updateFilmValues();
	falsemaxSatChanged(retrieveParam(false, LUX_FILM, LUX_FILM_TM_FALSE_MAXSAT));
	falseminSatChanged(retrieveParam(false, LUX_FILM, LUX_FILM_TM_FALSE_MINSAT));
	setFalseMethod(retrieveParam(false, LUX_FILM, LUX_FILM_TM_FALSE_METHOD));
	setFalseColorScale(retrieveParam(false, LUX_FILM, LUX_FILM_TM_FALSE_COLORSCALE));
	legendeChanged(5);
}

void ToneMapWidget::setFalseMethod(int choice)
{
	ui->comboBox_false_Method->setCurrentIndex(choice);
	m_false_method = choice;
	updateParam(LUX_FILM, LUX_FILM_TM_FALSE_METHOD, (double)m_false_method);

	falseLegend();
	emit valuesChanged();
}

void ToneMapWidget::setFalseColorScale(int choice)
{
	ui->comboBox_false_colorscale->setCurrentIndex(choice);
	m_false_colorscale = choice;
	updateParam(LUX_FILM, LUX_FILM_TM_FALSE_COLORSCALE, (double)m_false_colorscale);

	falseLegend();
	emit valuesChanged();
}

void ToneMapWidget::legendeChanged()
{
	m_TM_FalseLegendeNumber = Clamp(ui->lineEdit_false_legendeTest->text().toInt(), 2, 10);
	updateWidgetValue(ui->lineEdit_false_legendeTest, m_TM_FalseLegendeNumber);

	falseLegend();
	emit valuesChanged();
}
void ToneMapWidget::legendeChanged(int value)
{
	m_TM_FalseLegendeNumber = value;
	updateWidgetValue(ui->lineEdit_false_legendeTest, value);

	falseLegend();
	emit valuesChanged();
}

void ToneMapWidget::falseLegend()
{
	QPixmap pix(150, 220);
	falseDrawLegend(&pix);
	ui->Qlabel_image_false_legende->setPixmap(pix);
}

void ToneMapWidget::falseDrawLegend(QPaintDevice * dev)
{
	QPainter p(dev);

	const u_int _widthPix = 80;
	const u_int _heightPix = 200;

	p.setBrush(QBrush(Qt::black));
	p.drawRect(0, 0, dev->width(), dev->height());

	QColor c;

	const u_int nb = m_TM_FalseLegendeNumber;

	luxrays::RGBColor vcolor(0.f);

	if (nb <= 1) {
		for (u_int i = 0; i <= _heightPix; ++i) {
			c.setRgb(vcolor.c[0], vcolor.c[1], vcolor.c[2]);
			p.setPen(QPen(c));
			p.setBrush(QBrush(c));
			p.drawRect(5, 10 + i, _widthPix, 1);
		}
	} else {
		vector<float> tabVal(nb);
		vector<u_int> tabPix(nb);
		tabPix[0] = _heightPix;
		tabVal[0] = m_TM_FalseMaxSat;
		for (u_int i = 0; i < nb; ++i) {
			const float coeff = 1.f - i / (nb - 1.f);
			tabPix[i] = luxrays::Floor2Int(_heightPix * coeff);

			tabVal[i] = Clamp<float>(lux::InverseValueScale(lux::FalseScaleMethod(m_false_method), coeff) *
				(m_TM_FalseMaxSat - m_TM_FalseMinSat) +
				m_TM_FalseMinSat, m_TM_FalseMinSat, m_TM_FalseMaxSat);
		}
		u_int j = 0;
		for (u_int i = 0; i <= _heightPix; ++i) {
			vcolor = 255.f *
				lux::ValuetoRGB(lux::FalseColorScale(m_false_colorscale),
				static_cast<float>(_heightPix - i) / _heightPix);
			c.setRgb(vcolor.c[0], vcolor.c[1], vcolor.c[2]);

			p.setPen(QPen(c));
			p.setBrush(QBrush(c));
			p.drawRect(5, 10 + i, _widthPix, 1);

			if (tabPix[j] == _heightPix - i) {
				QString s;
				s.setNum(tabVal[j], 'g', 3);
				if (i == _heightPix ) {
					// Don't write in black
					c.setRgb(128, 128, 128);
					p.setPen(QPen(c));
					p.setBrush(QBrush(c));
				}
				p.drawRect(_widthPix + 5, 10 + i, 5, 1);
				p.drawText(_widthPix + 15, 10 + i + 4, s);
				++j;
			}
		}
	}
}

///////////////////////////////////////////
// Save and Load settings from a ini file.

void ToneMapWidget::SaveSettings(QString fName)
{
	QSettings settings(fName, QSettings::IniFormat);

	settings.beginGroup("tonemapping");
	if (settings.status())
		return;

	settings.setValue("TM_kernel", m_TM_kernel);
	settings.setValue("clamp_method", m_clamp_method);

	settings.setValue("TM_reinhard_prescale", m_TM_reinhard_prescale);
	settings.setValue("TM_reinhard_postscale", m_TM_reinhard_postscale);
	settings.setValue("TM_reinhard_burn", m_TM_reinhard_burn);

	settings.setValue("TM_linear_exposure", m_TM_linear_exposure);
	settings.setValue("TM_linear_sensitivity", m_TM_linear_sensitivity);
	settings.setValue("TM_linear_fstop", m_TM_linear_fstop);
	settings.setValue("TM_linear_gamma", m_TM_linear_gamma);

	settings.setValue("TM_contrast_ywa", m_TM_contrast_ywa);

	settings.setValue("TM_false_maxsat", m_TM_FalseMaxSat);
	settings.setValue("TM_false_minsat", m_TM_FalseMinSat);
	settings.setValue("TM_false_method", m_false_method);
	settings.setValue("TM_false_colorscale", m_false_colorscale);
	settings.setValue("TM_false_legendNumber", m_TM_FalseLegendeNumber);

	settings.endGroup();
}

void ToneMapWidget::LoadSettings(QString fName)
{
	QSettings settings(fName, QSettings::IniFormat);

	settings.beginGroup("tonemapping");
	if (settings.status())
		return;

	m_TM_kernel = settings.value("TM_kernel", 0).toInt();
	m_clamp_method = settings.value("clamp_method", 0).toInt();

	m_TM_reinhard_prescale = settings.value("TM_reinhard_prescale", 1.).toDouble();
	m_TM_reinhard_postscale = settings.value("TM_reinhard_postscale", 1.).toDouble();
	m_TM_reinhard_burn = settings.value("TM_reinhard_burn", 6.).toDouble();

	m_TM_linear_exposure = settings.value("TM_linear_exposure", 1.).toDouble();
	m_TM_linear_sensitivity = settings.value("TM_linear_sensitivity", 50.).toDouble();
	m_TM_linear_fstop = settings.value("TM_linear_fstop", 2.8).toDouble();
	m_TM_linear_gamma = settings.value("TM_linear_gamma", 1.).toDouble();

	m_TM_contrast_ywa = settings.value("TM_contrast_ywa", 0.1).toDouble();

	m_TM_FalseMaxSat = settings.value("TM_false_maxsat", m_TM_FalseMaxSat).toDouble();
	m_TM_FalseMinSat = settings.value("TM_false_minsat", m_TM_FalseMinSat).toDouble();
	m_false_method = settings.value("TM_false_method", 0).toInt();
	m_false_colorscale = settings.value("TM_false_colorscale", 0).toInt();
	m_TM_FalseLegendeNumber = settings.value("TM_false_legendNumber", m_TM_FalseLegendeNumber).toInt();

	settings.endGroup();

	SetFromValues();
	updateWidgetValues();

	emit valuesChanged();
}

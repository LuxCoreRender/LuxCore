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



//#include "api.h"


#include "lenseffectswidget.hxx"


LensEffectsWidget::LensEffectsWidget(QWidget *parent) : QWidget(parent), ui(new Ui::LensEffectsWidget)
{
	ui->setupUi(this);
	
	// Bloom
	connect(ui->slider_gaussianAmount, SIGNAL(valueChanged(int)), this, SLOT(gaussianAmountChanged(int)));
	connect(ui->spinBox_gaussianAmount, SIGNAL(valueChanged(double)), this, SLOT(gaussianAmountChanged(double)));
	connect(ui->slider_gaussianRadius, SIGNAL(valueChanged(int)), this, SLOT(gaussianRadiusChanged(int)));
	connect(ui->spinBox_gaussianRadius, SIGNAL(valueChanged(double)), this, SLOT(gaussianRadiusChanged(double)));
	connect(ui->button_gaussianComputeLayer, SIGNAL(clicked()), this, SLOT(computeBloomLayer()));
	connect(ui->button_gaussianDeleteLayer, SIGNAL(clicked()), this, SLOT(deleteBloomLayer()));
	
	// Vignetting
	connect(ui->slider_vignettingAmount, SIGNAL(valueChanged(int)), this, SLOT(vignettingAmountChanged(int)));
	connect(ui->spinBox_vignettingAmount, SIGNAL(valueChanged(double)), this, SLOT(vignettingAmountChanged(double)));
	connect(ui->checkBox_vignettingEnabled, SIGNAL(stateChanged(int)), this, SLOT(vignettingEnabledChanged(int)));
	
	// Chromatic abberration
	connect(ui->slider_caAmount, SIGNAL(valueChanged(int)), this, SLOT(caAmountChanged(int)));
	connect(ui->spinBox_caAmount, SIGNAL(valueChanged(double)), this, SLOT(caAmountChanged(double)));
	connect(ui->checkBox_caEnabled, SIGNAL(stateChanged(int)), this, SLOT(caEnabledChanged(int)));

	// Glare
	connect(ui->slider_glareAmount, SIGNAL(valueChanged(int)), this, SLOT(glareAmountChanged(int)));
	connect(ui->spinBox_glareAmount, SIGNAL(valueChanged(double)), this, SLOT(glareAmountChanged(double)));
	connect(ui->slider_glareRadius, SIGNAL(valueChanged(int)), this, SLOT(glareRadiusChanged(int)));
	connect(ui->spinBox_glareRadius, SIGNAL(valueChanged(double)), this, SLOT(glareRadiusChanged(double)));
	connect(ui->spinBox_glareBlades, SIGNAL(valueChanged(int)), this, SLOT(glareBladesChanged(int)));
	connect(ui->slider_glareThreshold, SIGNAL(valueChanged(int)), this, SLOT(glareThresholdSliderChanged(int)));
	connect(ui->spinBox_glareThreshold, SIGNAL(valueChanged(double)), this, SLOT(glareThresholdSpinBoxChanged(double)));
	connect(ui->button_glareComputeLayer, SIGNAL(clicked()), this, SLOT(computeGlareLayer()));
	connect(ui->button_glareDeleteLayer, SIGNAL(clicked()), this, SLOT(deleteGlareLayer()));
	connect(ui->checkBox_glareMap, SIGNAL(stateChanged(int)), this, SLOT(glareMapChanged(int)));
	connect(ui->button_browsePupilMap, SIGNAL(clicked()), this, SLOT(glareBrowsePupilMap()));
	connect(ui->button_browseLashesMap, SIGNAL(clicked()), this, SLOT(glareBrowseLashesMap()));

#if defined(__APPLE__) // for better design on OSX
	ui->tab_gaussianBloom->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_vignetting->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_chromaticAbberationTab->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_glare->setFont(QFont  ("Lucida Grande", 11));
#endif
}

LensEffectsWidget::~LensEffectsWidget()
{
}

void LensEffectsWidget::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::EnabledChange) {
		updateParam(LUX_FILM, LUX_FILM_VIGNETTING_ENABLED, m_Vignetting_Enabled && this->isEnabled());
		updateParam(LUX_FILM, LUX_FILM_ABERRATION_ENABLED, m_Aberration_enabled && this->isEnabled());
		updateParam(LUX_FILM, LUX_FILM_GLARE_AMOUNT, this->isEnabled() ? m_Glare_amount : 0.0);
		updateParam(LUX_FILM, LUX_FILM_BLOOMWEIGHT, this->isEnabled() ? m_bloomweight : 0.0);

		if (!this->isEnabled())
			// prevent bloom update
			updateParam(LUX_FILM, LUX_FILM_UPDATEBLOOMLAYER, 0.0);
		
		emit valuesChanged ();
	}
}

void LensEffectsWidget::updateWidgetValues()
{
	int sliderval;

	// Bloom widgets
	updateWidgetValue (ui->slider_gaussianAmount, (int)((FLOAT_SLIDER_RES / BLOOMWEIGHT_RANGE) * m_bloomweight));
	updateWidgetValue (ui->spinBox_gaussianAmount, m_bloomweight);

	updateWidgetValue (ui->slider_gaussianRadius, (int)((FLOAT_SLIDER_RES / BLOOMRADIUS_RANGE) * m_bloomradius));
	updateWidgetValue (ui->spinBox_gaussianRadius, m_bloomradius);

	// Vignetting
	if (m_Vignetting_Scale >= 0.f)
		sliderval = (int) (FLOAT_SLIDER_RES/2) + (( (FLOAT_SLIDER_RES/2) / VIGNETTING_SCALE_RANGE ) * (m_Vignetting_Scale));
	else
		sliderval = (int)(( FLOAT_SLIDER_RES/2 * VIGNETTING_SCALE_RANGE ) * (1.f - std::fabs(m_Vignetting_Scale)));

	updateWidgetValue (ui->checkBox_vignettingEnabled, m_Vignetting_Enabled);	
	updateWidgetValue (ui->slider_vignettingAmount, sliderval);
	updateWidgetValue (ui->spinBox_vignettingAmount, m_Vignetting_Scale);

	// Chromatic aberration
	updateWidgetValue (ui->checkBox_caEnabled, m_Aberration_enabled);	
	updateWidgetValue(ui->slider_caAmount, (int) (( FLOAT_SLIDER_RES / ABERRATION_AMOUNT_RANGE ) * (m_Aberration_amount)));
	updateWidgetValue(ui->spinBox_caAmount, m_Aberration_amount);
	
	// Glare
	updateWidgetValue(ui->slider_glareAmount, (int)((FLOAT_SLIDER_RES / GLARE_AMOUNT_RANGE) * m_Glare_amount));
	updateWidgetValue(ui->spinBox_glareAmount, m_Glare_amount);
	
	updateWidgetValue(ui->slider_glareRadius, (int)((FLOAT_SLIDER_RES / GLARE_RADIUS_RANGE) * m_Glare_radius));
	updateWidgetValue(ui->spinBox_glareRadius, m_Glare_radius);
	
	updateWidgetValue(ui->spinBox_glareBlades, m_Glare_blades);
	
	updateWidgetValue(ui->slider_glareThreshold, (int)((FLOAT_SLIDER_RES / GLARE_THRESHOLD_RANGE) * m_Glare_threshold));
	updateWidgetValue(ui->spinBox_glareThreshold, m_Glare_threshold);
	updateWidgetValue(ui->checkBox_glareMap, m_Glare_map);
	updateWidgetValue(ui->lineEdit_pupilMap, m_Glare_pupil);
	updateWidgetValue(ui->lineEdit_lashesMap, m_Glare_lashes);
}

void LensEffectsWidget::resetValues()
{
	m_bloomradius = 0.07f;
	m_bloomweight = 0.25f;

	m_Vignetting_Enabled = false;
	m_Vignetting_Scale = 0.4;
	
	m_Aberration_enabled = false;
	m_Aberration_amount = ABERRATION_AMOUNT_RANGE;

	m_Glare_amount = 0.03f;
	m_Glare_radius = 0.03f;
	m_Glare_blades = 3;
	m_Glare_threshold = 0.5f;
	m_Glare_map = false;
	m_Glare_pupil = "";
	m_Glare_lashes = "";
}

void LensEffectsWidget::resetFromFilm (bool useDefaults)
{
	double t;

	m_bloomradius = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_BLOOMRADIUS);
	m_bloomweight = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_BLOOMWEIGHT);

	t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_VIGNETTING_ENABLED);
	m_Vignetting_Enabled = t != 0.0;
	m_Vignetting_Scale = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_VIGNETTING_SCALE);

	t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_ABERRATION_ENABLED);
	m_Aberration_enabled = t != 0.0;
	m_Aberration_amount = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_ABERRATION_AMOUNT);

	m_Glare_amount = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_GLARE_AMOUNT);
	m_Glare_radius = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_GLARE_RADIUS);
	m_Glare_blades = (int)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_GLARE_BLADES);
	m_Glare_map = retrieveParam(useDefaults, LUX_FILM, LUX_FILM_GLARE_MAP) != 0;
	m_Glare_pupil = retrieveStringParam(useDefaults, LUX_FILM, LUX_FILM_GLARE_PUPIL);
	m_Glare_lashes = retrieveStringParam(useDefaults, LUX_FILM, LUX_FILM_GLARE_LASHES);

	luxSetParameterValue(LUX_FILM, LUX_FILM_BLOOMRADIUS, m_bloomradius);
	luxSetParameterValue(LUX_FILM, LUX_FILM_BLOOMWEIGHT, m_bloomweight);

	luxSetParameterValue(LUX_FILM, LUX_FILM_VIGNETTING_ENABLED, m_Vignetting_Enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_VIGNETTING_SCALE, m_Vignetting_Scale);

	luxSetParameterValue(LUX_FILM, LUX_FILM_ABERRATION_ENABLED, m_Aberration_enabled);
	luxSetParameterValue(LUX_FILM, LUX_FILM_ABERRATION_AMOUNT, m_Aberration_amount);

	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_AMOUNT, m_Glare_amount);
	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_RADIUS, m_Glare_radius);
	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_BLADES, m_Glare_blades);
	luxSetParameterValue(LUX_FILM, LUX_FILM_GLARE_MAP, m_Glare_map);
	luxSetStringParameterValue(LUX_FILM, LUX_FILM_GLARE_PUPIL, m_Glare_pupil.toAscii().data());
	luxSetStringParameterValue(LUX_FILM, LUX_FILM_GLARE_LASHES, m_Glare_lashes.toAscii().data());
}

void LensEffectsWidget::gaussianAmountChanged (int value)
{
	gaussianAmountChanged ( (double)value / ( FLOAT_SLIDER_RES / BLOOMWEIGHT_RANGE ) );
}

void LensEffectsWidget::gaussianAmountChanged (double value)
{
	m_bloomweight = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / BLOOMWEIGHT_RANGE) * m_bloomweight);

	updateWidgetValue(ui->slider_gaussianAmount, sliderval);
	updateWidgetValue(ui->spinBox_gaussianAmount, m_bloomweight);

	updateParam (LUX_FILM, LUX_FILM_BLOOMWEIGHT, m_bloomweight);

	emit valuesChanged ();
}

void LensEffectsWidget::gaussianRadiusChanged (int value)
{
	gaussianRadiusChanged ( (double)value / ( FLOAT_SLIDER_RES / BLOOMRADIUS_RANGE ) );
}

void LensEffectsWidget::gaussianRadiusChanged (double value)
{
	m_bloomradius = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / BLOOMRADIUS_RANGE) * m_bloomradius);

	updateWidgetValue(ui->slider_gaussianRadius, sliderval);
	updateWidgetValue(ui->spinBox_gaussianRadius, m_bloomradius);

	updateParam (LUX_FILM, LUX_FILM_BLOOMRADIUS, m_bloomradius);
}

void LensEffectsWidget::computeBloomLayer()
{
	// Signal film to update bloom layer at next tonemap
	updateParam(LUX_FILM, LUX_FILM_UPDATEBLOOMLAYER, 1.0f);
	ui->button_gaussianDeleteLayer->setEnabled (true);
	ui->slider_gaussianAmount->setEnabled(true);
	ui->spinBox_gaussianAmount->setEnabled(true);

	emit forceUpdate ();
}

void LensEffectsWidget::deleteBloomLayer()
{
	// Signal film to delete bloom layer
	updateParam(LUX_FILM, LUX_FILM_DELETEBLOOMLAYER, 1.0f);
	ui->button_gaussianDeleteLayer->setEnabled (false);
	ui->slider_gaussianAmount->setEnabled(false);
	ui->spinBox_gaussianAmount->setEnabled(false);

	emit forceUpdate ();
}

void LensEffectsWidget::vignettingAmountChanged(int value)
{
	double dvalue = -1.0f + (2.0f * (double)value / FLOAT_SLIDER_RES);
	vignettingAmountChanged ( dvalue );
}

void LensEffectsWidget::vignettingAmountChanged(double value)
{
	m_Vignetting_Scale = value;
	
	int sliderval; 
	sliderval = (int)(0.5f * (value + 1.0f) * FLOAT_SLIDER_RES);

	updateWidgetValue(ui->slider_vignettingAmount, sliderval);
	updateWidgetValue(ui->spinBox_vignettingAmount, m_Vignetting_Scale);

	updateParam (LUX_FILM, LUX_FILM_VIGNETTING_SCALE, m_Vignetting_Scale);

	if (m_Vignetting_Enabled)
		emit valuesChanged();
}

void LensEffectsWidget::vignettingEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_Vignetting_Enabled = true;
	else
		m_Vignetting_Enabled = false;

	updateParam (LUX_FILM, LUX_FILM_VIGNETTING_ENABLED, m_Vignetting_Enabled);
	
	emit valuesChanged();
}

void LensEffectsWidget::caAmountChanged (int value)
{
	caAmountChanged ( (double)value / ( FLOAT_SLIDER_RES / ABERRATION_AMOUNT_RANGE ) );
}

void LensEffectsWidget::caAmountChanged (double value)
{
	m_Aberration_amount = value;

	if (m_Aberration_amount > ABERRATION_AMOUNT_RANGE) 
		m_Aberration_amount = ABERRATION_AMOUNT_RANGE;
	else if (m_Aberration_amount < 0.0f) 
		m_Aberration_amount = 0.f;

	int sliderval = (int) (( FLOAT_SLIDER_RES / ABERRATION_AMOUNT_RANGE ) * (m_Aberration_amount));

	updateWidgetValue(ui->slider_caAmount, sliderval);
	updateWidgetValue(ui->spinBox_caAmount, m_Aberration_amount);

	updateParam(LUX_FILM, LUX_FILM_ABERRATION_AMOUNT, m_Aberration_amount);
	
	if (m_Aberration_enabled)
		emit valuesChanged();
}

void LensEffectsWidget::caEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_Aberration_enabled = true;
	else
		m_Aberration_enabled = false;

	updateParam (LUX_FILM, LUX_FILM_ABERRATION_ENABLED, m_Aberration_enabled);
	
	emit valuesChanged();
}

void LensEffectsWidget::glareAmountChanged (int value)
{
	glareAmountChanged ( (double)value / ( FLOAT_SLIDER_RES / GLARE_AMOUNT_RANGE ) );
}

void LensEffectsWidget::glareAmountChanged (double value)
{
	m_Glare_amount = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GLARE_AMOUNT_RANGE) * m_Glare_amount);

	updateWidgetValue(ui->slider_glareAmount, sliderval);
	updateWidgetValue(ui->spinBox_glareAmount, m_Glare_amount);

	updateParam (LUX_FILM, LUX_FILM_GLARE_AMOUNT, m_Glare_amount);

	emit valuesChanged();
}

void LensEffectsWidget::glareRadiusChanged (int value)
{
	glareRadiusChanged ( (double)value / ( FLOAT_SLIDER_RES / GLARE_RADIUS_RANGE ) );
}

void LensEffectsWidget::glareRadiusChanged (double value)
{
	m_Glare_radius = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / GLARE_RADIUS_RANGE) * m_Glare_radius);

	updateWidgetValue(ui->slider_glareRadius, sliderval);
	updateWidgetValue(ui->spinBox_glareRadius, m_Glare_radius);

	updateParam (LUX_FILM, LUX_FILM_GLARE_RADIUS, m_Glare_radius);
}

void LensEffectsWidget::glareBladesChanged(int value)
{
	m_Glare_blades = value;

	if (m_Glare_blades > GLARE_BLADES_MAX) 
		m_Glare_blades = GLARE_BLADES_MAX;
	else if (m_Glare_blades < GLARE_BLADES_MIN)
		m_Glare_blades = GLARE_BLADES_MIN;

	updateParam (LUX_FILM, LUX_FILM_GLARE_BLADES, m_Glare_blades);

	emit valuesChanged();
}

void LensEffectsWidget::glareThresholdSliderChanged(int value)
{
	glareThresholdSpinBoxChanged( (double)value / FLOAT_SLIDER_RES * GLARE_THRESHOLD_RANGE );
}

void LensEffectsWidget::glareThresholdSpinBoxChanged(double value)
{
	m_Glare_threshold = value;
	
	int sliderval = (int)(FLOAT_SLIDER_RES / GLARE_THRESHOLD_RANGE * m_Glare_threshold);
	
	updateWidgetValue(ui->slider_glareThreshold, sliderval);
	updateWidgetValue(ui->spinBox_glareThreshold, m_Glare_threshold);
	
	updateParam (LUX_FILM, LUX_FILM_GLARE_THRESHOLD, m_Glare_threshold);
}

void LensEffectsWidget::computeGlareLayer()
{
	// Signal film to update glare layer at next tonemap
	updateParam(LUX_FILM, LUX_FILM_UPDATEGLARELAYER, 1.0f);
	ui->button_glareDeleteLayer->setEnabled (true);
	ui->slider_glareAmount->setEnabled(true);
	ui->spinBox_glareAmount->setEnabled(true);

	emit forceUpdate ();
}

void LensEffectsWidget::deleteGlareLayer()
{
	// Signal film to delete glare layer
	updateParam(LUX_FILM, LUX_FILM_DELETEGLARELAYER, 1.0f);
	ui->button_glareDeleteLayer->setEnabled (false);
	ui->slider_glareAmount->setEnabled(false);
	ui->spinBox_glareAmount->setEnabled(false);

	emit forceUpdate ();
}

void LensEffectsWidget::glareMapChanged(int value)
{
	if (value == Qt::Checked)
		m_Glare_map = true;
	else
		m_Glare_map = false;

	updateParam (LUX_FILM, LUX_FILM_GLARE_MAP, m_Glare_map);
}

void LensEffectsWidget::glareBrowsePupilMap()
{
	m_Glare_pupil = QFileDialog::getOpenFileName(this, tr("Choose a pupil/aperture map"), m_lastOpendir, tr("Image files (*.png *.jpg)"));
	if (m_Glare_pupil.isEmpty())
		return;

	ui->lineEdit_pupilMap->setText(m_Glare_pupil);
	updateParam(LUX_FILM, LUX_FILM_GLARE_PUPIL, m_Glare_pupil.toAscii().data());

	QFileInfo info(m_Glare_pupil);
	m_lastOpendir = info.absolutePath();
}

void LensEffectsWidget::glareBrowseLashesMap()
{
	m_Glare_lashes = QFileDialog::getOpenFileName(this, tr("Choose an eyelashes/obstacle map"), m_lastOpendir, tr("Image files (*.png *.jpg)"));

	if (m_Glare_lashes.isEmpty())
		return;

	ui->lineEdit_lashesMap->setText(m_Glare_lashes);
	updateParam(LUX_FILM, LUX_FILM_GLARE_LASHES, m_Glare_lashes.toAscii().data());

	QFileInfo info(m_Glare_lashes);
	m_lastOpendir = info.absolutePath();
}

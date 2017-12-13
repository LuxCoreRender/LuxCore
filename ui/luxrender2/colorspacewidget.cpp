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
#include "colorspacewidget.hxx"
#include "mainwindow.hxx"

// colorspacepresets and colorspace whitepoints
static double colorspace_presets[8][NUM_COLORSPACE_PRESETS] = {
	{0.314275f, 0.31271f, 0.3460f, 0.313f, 0.313f, 0.310f, 0.313f, 0.313f, 0.33333f}, //xwhite
	{0.329411f, 0.32902f, 0.3590f, 0.329f, 0.329f, 0.316f, 0.329f, 0.329f, 0.33333f}, //ywhite
	{0.630000f, 0.6400f, 0.7347f, 0.640f, 0.625f, 0.670f, 0.630f, 0.640f, 0.7347f}, //xred
	{0.340000f, 0.3300f, 0.2653f, 0.340f, 0.340f, 0.330f, 0.340f, 0.330f, 0.2653f}, //yred
	{0.310000f, 0.3000f, 0.1596f, 0.210f, 0.280f, 0.210f, 0.310f, 0.290f, 0.2738f}, //xgreen
	{0.595000f, 0.6000f, 0.8404f, 0.710f, 0.595f, 0.710f, 0.595f, 0.600f, 0.7174f}, //ygreen
	{0.155000f, 0.1500f, 0.0366f, 0.150f, 0.155f, 0.140f, 0.155f, 0.150f, 0.1666f}, //xblue
	{0.070000f, 0.0600f, 0.0001f, 0.060f, 0.070f, 0.080f, 0.070f, 0.060f, 0.0089f}  //yblue
};
	
// standard whitepoints
static double whitepoint_presets[2][NUM_WHITEPOINT_PRESETS] = {
//   A         B         C         D50       D55       D65       D75       E         F2        F7        F11       9300K     D65 (SMPTE)
	{0.44757f, 0.34842f, 0.31006f, 0.34567f, 0.33242f, 0.31271f, 0.29902f, 0.33333f, 0.37208f, 0.31292f, 0.38052f, 0.28480f, 0.314275f}, //xwhite
	{0.40745f, 0.35161f, 0.31616f, 0.35850f, 0.34743f, 0.32902f, 0.31485f, 0.33333f, 0.37529f, 0.32933f, 0.37713f, 0.29320f, 0.329411f}  //ywhite
};

#define DEFAULT_EPSILON_MIN 0.000005f
static bool EqualDouble(const double a, const double b)
{
	return (std::fabs(a-b) < DEFAULT_EPSILON_MIN);
}

/* 
 * Converts from xy chromaticity coordinates to correlated color temperature
 */
static double XyToTemperature(double x, double y)
{
	// based "Calculating Correlated Color Temperatures Across the Entire Gamut of Daylight and Skylight Chromaticities"
	// see http://www.nadn.navy.mil/Users/oceano/raylee/papers/RLee_AO_CCTpaper.pdf
	const double xe = 0.3366;
	const double ye = 0.1735;
	const double n = (x - xe) / (y - ye);

	const double A0 = -949.86315;
	const double A1 = 6253.80338;
	const double t1 = 0.92159;
	const double A2 = 28.70599;
	const double t2 = 0.20039;
	const double A3 = 0.00004;
	const double t3 = 0.07125;

	return A0 + A1*exp(-n/t1) + A2*exp(-n/t2) + A3*exp(-n/t3);
}

/* 
 * Converts from correlated color temperature to xy chromaticity coordinates
 */
static void TemperatureToXy(double temperature, double *x, double *y)
{
	const double T = temperature;
	const double T2 = T * T;
	const double T3 = T2 * T;

	// from http://en.wikipedia.org/wiki/Planckian_locus
	// based on method in "Design of Advanced Color Temperature Control System for HDTV Applications"
	if (T < 4000) { // T >= 1667
		*x = -0.2661239e9 / T3 - 0.2343589e6 / T2 + 0.8776956e3 / T + 0.179910;
	} else { // T <= 25000
		*x = -3.0258469e9 / T3 + 2.1070379e6 / T2 + 0.2226347e3 / T + 0.24039;
	}
	
	const double x1 = *x;
	const double x2 = x1 * x1;
	const double x3 = x2 * x1;

	if (T < 2222) { // T >= 1667
		*y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x1 - 0.20219683;
	} else if (T < 4000) {
		*y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x1 - 0.16748867;
	} else { // T <= 25000
		*y = 3.0817580 * x3 - 5.8733867 * x2 + 3.75112997 * x1 - 0.37001483;
	}

/*
	// alternative method, only valid for T >= 4000 K
	// see http://www.brucelindbloom.com/index.html?Eqn_DIlluminant.html
	if (T <= 7000) { // T >= 4000
		*x = -4.6070e9 / T3 + 2.9678e6 / T2 + 0.09911e3 / T + 0.244063;
	} else { // T <= 25000
		*x = -2.0064e9 / T3 + 1.9018e6 / T2 + 0.24748e3 / T + 0.237040;
	}

	const double x1 = *x;
	const double x2 = x1 * x1;

	*y = -3.00 * x2 + 2.870 * x1 - 0.275;
*/
}

using namespace std;

ColorSpaceWidget::ColorSpaceWidget(QWidget *parent) : QWidget(parent), ui(new Ui::ColorSpaceWidget)
{
	ui->setupUi(this);
	
	connect(ui->comboBox_colorSpacePreset, SIGNAL(currentIndexChanged(int)), this, SLOT(setColorSpacePreset(int)));
	connect(ui->comboBox_whitePointPreset, SIGNAL(currentIndexChanged(int)), this, SLOT(setWhitepointPreset(int)));
	connect(ui->slider_whitePointX, SIGNAL(valueChanged(int)), this, SLOT(whitePointXChanged(int)));
	connect(ui->spinBox_whitePointX, SIGNAL(valueChanged(double)), this, SLOT(whitePointXChanged(double)));
	connect(ui->slider_whitePointY, SIGNAL(valueChanged(int)), this, SLOT(whitePointYChanged(int)));
	connect(ui->spinBox_whitePointY, SIGNAL(valueChanged(double)), this, SLOT(whitePointYChanged(double)));
	connect(ui->checkbox_precisionEdit, SIGNAL(stateChanged(int)), this, SLOT(precisionChanged(int)));
	
	connect(ui->slider_redX, SIGNAL(valueChanged(int)), this, SLOT(redXChanged(int)));
	connect(ui->spinBox_redX, SIGNAL(valueChanged(double)), this, SLOT(redXChanged(double)));
	connect(ui->slider_redY, SIGNAL(valueChanged(int)), this, SLOT(redYChanged(int)));
	connect(ui->spinBox_redY, SIGNAL(valueChanged(double)), this, SLOT(redYChanged(double)));
	connect(ui->slider_blueX, SIGNAL(valueChanged(int)), this, SLOT(blueXChanged(int)));
	connect(ui->spinBox_blueX, SIGNAL(valueChanged(double)), this, SLOT(blueXChanged(double)));
	connect(ui->slider_blueY, SIGNAL(valueChanged(int)), this, SLOT(blueYChanged(int)));
	connect(ui->spinBox_blueY, SIGNAL(valueChanged(double)), this, SLOT(blueYChanged(double)));
	connect(ui->slider_greenX, SIGNAL(valueChanged(int)), this, SLOT(greenXChanged(int)));
	connect(ui->spinBox_greenX, SIGNAL(valueChanged(double)), this, SLOT(greenXChanged(double)));
	connect(ui->slider_greenY, SIGNAL(valueChanged(int)), this, SLOT(greenYChanged(int)));
	connect(ui->spinBox_greenY, SIGNAL(valueChanged(double)), this, SLOT(greenYChanged(double)));

	connect(ui->slider_temperature, SIGNAL(valueChanged(int)), this, SLOT(temperatureChanged(int)));
	connect(ui->spinBox_temperature, SIGNAL(valueChanged(double)), this, SLOT(temperatureChanged(double)));

#if defined(__APPLE__)
	ui->tab_whitepoint->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_rgb->setFont(QFont  ("Lucida Grande", 11));
	ui->tab_temperature->setFont(QFont  ("Lucida Grande", 11));
#endif

}

ColorSpaceWidget::~ColorSpaceWidget()
{
}

void ColorSpaceWidget::precisionChanged(int value)
{
	if (value == Qt::Checked) {
		ui->spinBox_whitePointX->setDecimals(5);ui->spinBox_whitePointX->setSingleStep(0.0001);
		ui->spinBox_whitePointY->setDecimals(5);ui->spinBox_whitePointY->setSingleStep(0.0001);
		ui->spinBox_redX->setDecimals(5);ui->spinBox_redX->setSingleStep(0.0001);
		ui->spinBox_redY->setDecimals(5);ui->spinBox_redY->setSingleStep(0.0001);
		ui->spinBox_greenX->setDecimals(5);ui->spinBox_greenX->setSingleStep(0.0001);
		ui->spinBox_greenY->setDecimals(5);ui->spinBox_greenY->setSingleStep(0.0001);
		ui->spinBox_blueX->setDecimals(5);ui->spinBox_blueX->setSingleStep(0.0001);
		ui->spinBox_blueY->setDecimals(5);ui->spinBox_blueY->setSingleStep(0.0001);
		ui->spinBox_temperature->setDecimals(1);ui->spinBox_temperature->setSingleStep(1);
	} else {
		ui->spinBox_whitePointX->setDecimals(3);ui->spinBox_whitePointX->setSingleStep(0.010);
		ui->spinBox_whitePointY->setDecimals(3);ui->spinBox_whitePointY->setSingleStep(0.010);
		ui->spinBox_redX->setDecimals(3);ui->spinBox_redX->setSingleStep(0.010);
		ui->spinBox_redY->setDecimals(3);ui->spinBox_redY->setSingleStep(0.010);
		ui->spinBox_greenX->setDecimals(3);ui->spinBox_greenX->setSingleStep(0.010);
		ui->spinBox_greenY->setDecimals(3);ui->spinBox_greenY->setSingleStep(0.010);
		ui->spinBox_blueX->setDecimals(3);ui->spinBox_blueX->setSingleStep(0.010);
		ui->spinBox_blueY->setDecimals(3);ui->spinBox_blueY->setSingleStep(0.010);
		ui->spinBox_temperature->setDecimals(0);ui->spinBox_temperature->setSingleStep(100);
	}
}

void ColorSpaceWidget::updateWidgetValues()
{
	// Colorspace widgets
	updateWidgetValue(ui->slider_whitePointX, (int)((FLOAT_SLIDER_RES / TORGB_XWHITE_RANGE) * m_TORGB_xwhite));
	updateWidgetValue(ui->spinBox_whitePointX, m_TORGB_xwhite);
	updateWidgetValue(ui->slider_whitePointY, (int)((FLOAT_SLIDER_RES / TORGB_YWHITE_RANGE) * m_TORGB_ywhite));
	updateWidgetValue(ui->spinBox_whitePointY, m_TORGB_ywhite);

	updateWidgetValue(ui->slider_redX, (int)((FLOAT_SLIDER_RES / TORGB_XRED_RANGE) * m_TORGB_xred));
	updateWidgetValue(ui->spinBox_redX, m_TORGB_xred);
	updateWidgetValue(ui->slider_redY, (int)((FLOAT_SLIDER_RES / TORGB_YRED_RANGE) * m_TORGB_yred));
	updateWidgetValue(ui->spinBox_redY, m_TORGB_yred);

	updateWidgetValue(ui->slider_greenX, (int)((FLOAT_SLIDER_RES / TORGB_XGREEN_RANGE) * m_TORGB_xgreen));
	updateWidgetValue(ui->spinBox_greenX, m_TORGB_xgreen);
	updateWidgetValue(ui->slider_greenY, (int)((FLOAT_SLIDER_RES / TORGB_YGREEN_RANGE) * m_TORGB_ygreen));
	updateWidgetValue(ui->spinBox_greenY, m_TORGB_ygreen);

	updateWidgetValue(ui->slider_blueX, (int)((FLOAT_SLIDER_RES / TORGB_XBLUE_RANGE) * m_TORGB_xblue));
	updateWidgetValue(ui->spinBox_blueX, m_TORGB_xblue);
	updateWidgetValue(ui->slider_blueY, (int)((FLOAT_SLIDER_RES / TORGB_YBLUE_RANGE) * m_TORGB_yblue));
	updateWidgetValue(ui->spinBox_blueY, m_TORGB_yblue);

	updateWidgetValue(ui->slider_temperature, (int)m_TORGB_temperature);
	updateWidgetValue(ui->spinBox_temperature, m_TORGB_temperature);
}

void ColorSpaceWidget::resetValues()
{
	m_TORGB_xwhite = colorspace_presets[0][0];
	m_TORGB_ywhite = colorspace_presets[1][0];
	m_TORGB_xred = colorspace_presets[2][0];
	m_TORGB_yred = colorspace_presets[3][0];
	m_TORGB_xgreen = colorspace_presets[4][0];
	m_TORGB_ygreen = colorspace_presets[5][0];
	m_TORGB_xblue = colorspace_presets[6][0];
	m_TORGB_yblue = colorspace_presets[7][0];
	m_TORGB_temperature = XyToTemperature(m_TORGB_xwhite, m_TORGB_ywhite);
}

int ColorSpaceWidget::colorspaceToPreset()
{
	for (int i = 0; i < NUM_COLORSPACE_PRESETS; i++) {
		if (EqualDouble(m_TORGB_xwhite, colorspace_presets[0][i])
				&& EqualDouble(m_TORGB_ywhite, colorspace_presets[1][i])
				&& EqualDouble(m_TORGB_xred, colorspace_presets[2][i])
				&& EqualDouble(m_TORGB_yred, colorspace_presets[3][i])
				&& EqualDouble(m_TORGB_xgreen, colorspace_presets[4][i])
				&& EqualDouble(m_TORGB_ygreen, colorspace_presets[5][i])
				&& EqualDouble(m_TORGB_xblue, colorspace_presets[6][i])
				&& EqualDouble(m_TORGB_yblue, colorspace_presets[7][i]))
			return i+1;
	}
	return 0;
}

int ColorSpaceWidget::whitepointToPreset()
{
	for (int i = 0; i < NUM_WHITEPOINT_PRESETS; i++) {
		if (EqualDouble(m_TORGB_xwhite, whitepoint_presets[0][i]) && EqualDouble(m_TORGB_ywhite, whitepoint_presets[1][i]))
			return i+1;
	}
	return 0;
}

void ColorSpaceWidget::resetFromFilm (bool useDefaults)
{
	m_TORGB_xwhite = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_WHITE);
	m_TORGB_ywhite = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_WHITE);
	m_TORGB_xred = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_RED);
	m_TORGB_yred = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_RED);
	m_TORGB_xgreen = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_GREEN);
	m_TORGB_ygreen = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_GREEN);
	m_TORGB_xblue = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_X_BLUE);
	m_TORGB_yblue = (double)retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_Y_BLUE);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);
	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);
	
	// this will trigger the changed signal, which will update whitepoint preset as well
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
}

void ColorSpaceWidget::setColorSpacePreset(int choice)
{
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(choice);
	ui->comboBox_colorSpacePreset->blockSignals(false);
		
	// first choice is "User-defined"
	if (choice < 1) {
		ui->comboBox_colorSpacePreset->blockSignals(true);
		ui->comboBox_whitePointPreset->setCurrentIndex(0);
		ui->comboBox_colorSpacePreset->blockSignals(false);
		return;
	}
	
	m_TORGB_xwhite = colorspace_presets[0][choice-1];
	m_TORGB_ywhite = colorspace_presets[1][choice-1];
	m_TORGB_xred = colorspace_presets[2][choice-1];
	m_TORGB_yred = colorspace_presets[3][choice-1];
	m_TORGB_xgreen = colorspace_presets[4][choice-1];
	m_TORGB_ygreen = colorspace_presets[5][choice-1];
	m_TORGB_xblue = colorspace_presets[6][choice-1];
	m_TORGB_yblue = colorspace_presets[7][choice-1];
	m_TORGB_temperature = XyToTemperature(m_TORGB_xwhite, m_TORGB_ywhite);
	
	// update whitepoint preset dropdown as well
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_whitePointPreset->setCurrentIndex(whitepointToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);

	// Update values in film trough API
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);

	updateWidgetValues ();

	emit valuesChanged();
}

void ColorSpaceWidget::setWhitepointPreset(int choice)
{
	ui->comboBox_whitePointPreset->blockSignals(true);
	ui->comboBox_whitePointPreset->setCurrentIndex(choice);
	ui->comboBox_whitePointPreset->blockSignals(false);
	
	// first choice is "User-defined"
	if (choice < 1)
		return;

	m_TORGB_xwhite = whitepoint_presets[0][choice-1];
	m_TORGB_ywhite = whitepoint_presets[1][choice-1];
	m_TORGB_temperature = XyToTemperature(m_TORGB_xwhite, m_TORGB_ywhite);

	// Update values in film trough API
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);

	updateWidgetValues ();

	emit valuesChanged();
}

void ColorSpaceWidget::whitePointXChanged(int value)
{
	whitePointXChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_XWHITE_RANGE ) );
}

void ColorSpaceWidget::whitePointXChanged(double value)
{
	m_TORGB_xwhite = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_XWHITE_RANGE) * m_TORGB_xwhite);

	updateWidgetValue(ui->slider_whitePointX, sliderval);
	updateWidgetValue(ui->spinBox_whitePointX, m_TORGB_xwhite);

	// update whitepoint temperature
	m_TORGB_temperature = XyToTemperature(m_TORGB_xwhite, m_TORGB_ywhite);
	updateWidgetValue(ui->slider_temperature, (int)m_TORGB_temperature);
	updateWidgetValue(ui->spinBox_temperature, m_TORGB_temperature);

	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);
	
	// Don't trigger yet another event
	ui->comboBox_whitePointPreset->blockSignals(true);
	ui->comboBox_whitePointPreset->setCurrentIndex(whitepointToPreset());
	ui->comboBox_whitePointPreset->blockSignals(false);

	updateParam (LUX_FILM, LUX_FILM_TORGB_X_WHITE, m_TORGB_xwhite);

	emit valuesChanged();
}

void ColorSpaceWidget::whitePointYChanged (int value)
{
	whitePointYChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_YWHITE_RANGE ) );
}

void ColorSpaceWidget::whitePointYChanged (double value)
{
	m_TORGB_ywhite = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_YWHITE_RANGE) * m_TORGB_ywhite);

	updateWidgetValue(ui->slider_whitePointY, sliderval);
	updateWidgetValue(ui->spinBox_whitePointY, m_TORGB_ywhite);
	
	// update whitepoint temperature
	m_TORGB_temperature = XyToTemperature(m_TORGB_xwhite, m_TORGB_ywhite);
	updateWidgetValue(ui->slider_temperature, (int)m_TORGB_temperature);
	updateWidgetValue(ui->spinBox_temperature, m_TORGB_temperature);

	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);

	// Don't trigger yet another event
	ui->comboBox_whitePointPreset->blockSignals(true);
	ui->comboBox_whitePointPreset->setCurrentIndex(whitepointToPreset());
	ui->comboBox_whitePointPreset->blockSignals(false);

	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_WHITE, m_TORGB_ywhite);

	emit valuesChanged();
}

void ColorSpaceWidget::redYChanged (int value)
{
	redYChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_YRED_RANGE ) );
}

void ColorSpaceWidget::redYChanged (double value)
{
	m_TORGB_yred = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_YRED_RANGE) * m_TORGB_yred);

	updateWidgetValue(ui->slider_redY, sliderval);
	updateWidgetValue(ui->spinBox_redY, m_TORGB_yred);

	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);
	
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_RED, m_TORGB_yred);

	emit valuesChanged();
}

void ColorSpaceWidget::redXChanged (int value)
{
	redXChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_XRED_RANGE ) );
}

void ColorSpaceWidget::redXChanged (double value)
{
	m_TORGB_xred = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_XRED_RANGE) * m_TORGB_xred);

	updateWidgetValue(ui->slider_redX, sliderval);
	updateWidgetValue(ui->spinBox_redX, m_TORGB_xred);
	
	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);
	
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_RED, m_TORGB_xred);

	emit valuesChanged();
}

void ColorSpaceWidget::blueYChanged (int value)
{
	blueYChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_YBLUE_RANGE ) );
}

void ColorSpaceWidget::blueYChanged (double value)
{
	m_TORGB_yblue = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_YBLUE_RANGE) * m_TORGB_yblue);

	updateWidgetValue(ui->slider_blueY, sliderval);
	updateWidgetValue(ui->spinBox_blueY, m_TORGB_yblue);
	
	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);
	
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_BLUE, m_TORGB_yblue);

	emit valuesChanged();
}

void ColorSpaceWidget::blueXChanged (int value)
{
	blueXChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_XBLUE_RANGE ) );
}

void ColorSpaceWidget::blueXChanged (double value)
{
	m_TORGB_xblue = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_XBLUE_RANGE) * m_TORGB_xblue);

	updateWidgetValue(ui->slider_blueX, sliderval);
	updateWidgetValue(ui->spinBox_blueX, m_TORGB_xblue);
	
	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);
	
	updateParam (LUX_FILM, LUX_FILM_TORGB_X_BLUE, m_TORGB_xblue);

	emit valuesChanged();
}

void ColorSpaceWidget::greenYChanged (int value)
{
	greenYChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_YGREEN_RANGE ) );
}

void ColorSpaceWidget::greenYChanged (double value)
{
	m_TORGB_ygreen = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_YGREEN_RANGE) * m_TORGB_ygreen);

	updateWidgetValue(ui->slider_greenY, sliderval);
	updateWidgetValue(ui->spinBox_greenY, m_TORGB_ygreen);
	
	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);
	
	updateParam (LUX_FILM, LUX_FILM_TORGB_Y_GREEN, m_TORGB_ygreen);

	emit valuesChanged();
}

void ColorSpaceWidget::greenXChanged (int value)
{
	greenXChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_XGREEN_RANGE ) );
}

void ColorSpaceWidget::greenXChanged (double value)
{
	m_TORGB_xgreen = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_XGREEN_RANGE) * m_TORGB_xgreen);

	updateWidgetValue(ui->slider_greenX, sliderval);
	updateWidgetValue(ui->spinBox_greenX, m_TORGB_xgreen);
	
	// Don't trigger yet another event
	ui->comboBox_colorSpacePreset->blockSignals(true);
	ui->comboBox_colorSpacePreset->setCurrentIndex(colorspaceToPreset());
	ui->comboBox_colorSpacePreset->blockSignals(false);

	updateParam (LUX_FILM, LUX_FILM_TORGB_X_GREEN, m_TORGB_xgreen);

	emit valuesChanged();
}

void ColorSpaceWidget::temperatureChanged(int value)
{
	temperatureChanged ( (double)value );
}

void ColorSpaceWidget::temperatureChanged(double value)
{
	TemperatureToXy(value, &m_TORGB_xwhite, &m_TORGB_ywhite);

	// these will modify m_TORGB_temperature
	whitePointXChanged(m_TORGB_xwhite);
	whitePointYChanged(m_TORGB_ywhite);

	// so update to the real value afterwards
	m_TORGB_temperature = value;
	updateWidgetValue(ui->slider_temperature, (int)m_TORGB_temperature);
	updateWidgetValue(ui->spinBox_temperature, m_TORGB_temperature);

	emit valuesChanged();
}

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

#include <QColorDialog>
#include <QFont>
#include <QPalette>
#include <QSettings>

//#include "api.h"


#include "lightgroupwidget.hxx"
#include "guiutil.h"
#include "mainwindow.hxx"
#include "ui_lightgroup.h"

LightGroupWidget::LightGroupWidget(QWidget *parent) : QWidget(parent), ui(new Ui::LightGroupWidget)
{
	ui->setupUi(this);
	
	connect(ui->checkBox_enableRGB, SIGNAL(stateChanged(int)), this, SLOT(rgbEnabledChanged(int)));
	connect(ui->checkBox_enableBB, SIGNAL(stateChanged(int)), this, SLOT(bbEnabledChanged(int)));
	connect(ui->slider_gain, SIGNAL(valueChanged(int)), this, SLOT(gainChanged(int)));
	connect(ui->spinBox_gain, SIGNAL(valueChanged(double)), this, SLOT(gainChanged(double)));
	connect(ui->slider_colortemp, SIGNAL(valueChanged(int)), this, SLOT(colortempChanged(int)));
	connect(ui->spinBox_colortemp, SIGNAL(valueChanged(double)), this, SLOT(colortempChanged(double)));
	connect(ui->toolButton_colorpicker, SIGNAL(clicked()), this, SLOT(colorPicker()));
	
#if defined(__APPLE__)
	setFont(QFont  ("Lucida Grande", 11));
#endif

	m_Index = -1;
	ResetValues();
}

LightGroupWidget::~LightGroupWidget()
{
}

void LightGroupWidget::changeEvent(QEvent *event)
{
	m_LG_enable = this->isEnabled();
	if (event->type() == QEvent::EnabledChange) {
		luxSetParameterValue(LUX_FILM, LUX_FILM_LG_ENABLE, this->isEnabled(), m_Index);
		SetWidgetsEnabled(m_LG_enable);
		emit valuesChanged ();
	}
}

void LightGroupWidget::rgbEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_LG_rgb_enabled = true;
	else
		m_LG_rgb_enabled = false;

	updateParam(LUX_FILM, LUX_FILM_LG_SCALE_RED, (m_LG_rgb_enabled ? m_LG_scaleRed : 1.0), m_Index);
	updateParam(LUX_FILM, LUX_FILM_LG_SCALE_GREEN, (m_LG_rgb_enabled ? m_LG_scaleGreen : 1.0), m_Index);
	updateParam(LUX_FILM, LUX_FILM_LG_SCALE_BLUE, (m_LG_rgb_enabled ? m_LG_scaleBlue : 1.0), m_Index);

	ui->toolButton_colorpicker->setEnabled(m_LG_rgb_enabled);

	emit valuesChanged();
}

void LightGroupWidget::bbEnabledChanged(int value)
{
	if (value == Qt::Checked)
		m_LG_temperature_enabled = true;
	else
		m_LG_temperature_enabled = false;

	ui->slider_colortemp->setEnabled(m_LG_temperature_enabled);
	ui->spinBox_colortemp->setEnabled(m_LG_temperature_enabled);

	updateParam(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);

	emit valuesChanged();
}

float LightGroupWidget::SliderValToScale(int sliderval)
{
	float logscale = (float)sliderval * (LG_SCALE_LOG_MAX - LG_SCALE_LOG_MIN) / FLOAT_SLIDER_RES + LG_SCALE_LOG_MIN;
	return std::pow(10.f, logscale);
}

int LightGroupWidget::ScaleToSliderVal(float scale)
{
	if (scale <= 0)
		return 0;

	float logscale = Clamp<float>(log10f(scale), LG_SCALE_LOG_MIN, LG_SCALE_LOG_MAX);
	const int val = static_cast<int>((logscale - LG_SCALE_LOG_MIN) / (LG_SCALE_LOG_MAX - LG_SCALE_LOG_MIN) * FLOAT_SLIDER_RES);

	return val;
}

void LightGroupWidget::gainChanged(int value)
{
	gainChanged((double)SliderValToScale(value));
}

void LightGroupWidget::gainChanged(double value)
{
	m_LG_scale = value;
	
	if (m_LG_scale > std::pow(10.f, LG_SCALE_LOG_MAX))
		m_LG_scale = std::pow(10.f, LG_SCALE_LOG_MAX);
	else if (m_LG_scale < 0.f)
		m_LG_scale = 0.f;
	
	int sliderval = ScaleToSliderVal(m_LG_scale);

	updateWidgetValue(ui->slider_gain, sliderval);
	updateWidgetValue(ui->spinBox_gain, m_LG_scale);

	updateParam(LUX_FILM, LUX_FILM_LG_SCALE, m_LG_scale, m_Index);

	emit valuesChanged();
}

void LightGroupWidget::colortempChanged(int value)
{
	colortempChanged(((((double) value) / FLOAT_SLIDER_RES) * (LG_TEMPERATURE_MAX - LG_TEMPERATURE_MIN)) + LG_TEMPERATURE_MIN);
}

void LightGroupWidget::colortempChanged(double value)
{
	m_LG_temperature = value;
	
	if (m_LG_temperature > LG_TEMPERATURE_MAX)
		m_LG_temperature = LG_TEMPERATURE_MAX;
	else if (m_LG_temperature < LG_TEMPERATURE_MIN)
		m_LG_temperature = LG_TEMPERATURE_MIN;

	int sliderval = (int)(((m_LG_temperature - LG_TEMPERATURE_MIN) / (LG_TEMPERATURE_MAX - LG_TEMPERATURE_MIN)) * FLOAT_SLIDER_RES);

	updateWidgetValue(ui->slider_colortemp, sliderval);
	updateWidgetValue(ui->spinBox_colortemp, m_LG_temperature);

	updateParam(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);

	if (m_LG_temperature_enabled)
		emit valuesChanged();
}

void LightGroupWidget::colorSelected(const QColor & color){
	m_LG_scaleRed = color.red() / 255.0;
	m_LG_scaleGreen = color.green() / 255.0;
	m_LG_scaleBlue = color.blue() / 255.0;

	updateParam(LUX_FILM, LUX_FILM_LG_SCALE_RED, (m_LG_rgb_enabled ? m_LG_scaleRed : 1.0), m_Index);
	updateParam(LUX_FILM, LUX_FILM_LG_SCALE_GREEN, (m_LG_rgb_enabled ? m_LG_scaleGreen : 1.0), m_Index);
	updateParam(LUX_FILM, LUX_FILM_LG_SCALE_BLUE, (m_LG_rgb_enabled ? m_LG_scaleBlue : 1.0), m_Index);

	ui->toolButton_colorfield->setPalette(QPalette(color));
	ui->toolButton_colorfield->setAutoFillBackground(true);
	if (m_LG_rgb_enabled)
		emit valuesChanged();
}

void LightGroupWidget::colorPicker()
{
	
	QColor dcolor((int)(m_LG_scaleRed * 255.0),
                  (int)(m_LG_scaleGreen * 255.0),
                  (int)(m_LG_scaleBlue * 255.0));
	QColorDialog colorDlg(dcolor,this);
#if defined(__APPLE__)
		colorDlg.setOptions( QColorDialog::NoButtons );
#endif
	connect(&colorDlg, SIGNAL(colorSelected(const QColor &)), this, SLOT(colorSelected(const QColor &)));
	connect(&colorDlg, SIGNAL(currentColorChanged(const QColor &)), this, SLOT(colorSelected(const QColor &)));
	colorDlg.exec();
	disconnect(&colorDlg, SIGNAL(colorSelected(const QColor &)));
	disconnect(&colorDlg, SIGNAL(currentColorChanged(const QColor &)));
}

QString LightGroupWidget::GetTitle()
{
	return title;
}

int LightGroupWidget::GetIndex()
{
	return m_Index;
}

void LightGroupWidget::SetIndex(int index)
{
	m_Index = index;
}

void LightGroupWidget::UpdateWidgetValues()
{
	SetWidgetsEnabled(m_LG_enable);

	updateWidgetValue(ui->slider_gain, ScaleToSliderVal(m_LG_scale));
	updateWidgetValue(ui->spinBox_gain, m_LG_scale);

	updateWidgetValue(ui->checkBox_enableBB, m_LG_temperature_enabled);
	updateWidgetValue(ui->slider_colortemp, (int)(((m_LG_temperature - LG_TEMPERATURE_MIN) / (LG_TEMPERATURE_MAX - LG_TEMPERATURE_MIN)) * FLOAT_SLIDER_RES));
	updateWidgetValue(ui->spinBox_colortemp, m_LG_temperature);

	updateWidgetValue(ui->checkBox_enableRGB, m_LG_rgb_enabled);

	ui->toolButton_colorfield->setPalette(QPalette(QColor(
											(int)(m_LG_scaleRed * 255.0),
											(int)(m_LG_scaleGreen * 255.0),
											(int)(m_LG_scaleBlue * 255.0))));
	ui->toolButton_colorfield->setAutoFillBackground(true);

	/*wxString st;
	wxColour colour(Clamp(int(m_LG_scaleRed * 255.0), 0, 255),
					Clamp(int(m_LG_scaleGreen * 255.0), 0, 255),
					Clamp(int(m_LG_scaleBlue * 255.0), 0, 255));
	m_LG_rgbPicker->SetColour(colour);*/
}

void LightGroupWidget::ResetValues()
{
	title = QString("lightgroup");
	m_LG_enable = true;
	m_LG_scale = 1.f;
	m_LG_temperature_enabled = false;
	m_LG_temperature = 6500.f;
	m_LG_rgb_enabled = false;
	m_LG_scaleRed = 1.f;
	m_LG_scaleGreen = 1.f;
	m_LG_scaleBlue = 1.f;
	m_LG_scaleX = 1.f;
	m_LG_scaleY = 1.f;
}

void LightGroupWidget::ResetValuesFromFilm(bool useDefaults)
{
	char tmpStr[256];
	luxGetStringParameterValue(LUX_FILM, LUX_FILM_LG_NAME, &tmpStr[0], 256, m_Index);
	
	title = QString(tmpStr);
	
	//m_LG_name->SetLabel(wxString::FromAscii(tmpStr));
	m_LG_enable = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_ENABLE, m_Index) != 0.f;

	// set enabled here so pane can pick it up when this widget is added to it
	this->setEnabled(m_LG_enable);

	m_LG_scale = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE, m_Index);
	double t = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_TEMPERATURE, m_Index);
	m_LG_temperature_enabled = t != 0.0;
	m_LG_temperature = m_LG_temperature_enabled ? t : m_LG_temperature;
	double r = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE_RED, m_Index);
	double g = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE_GREEN, m_Index);
	double b = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_LG_SCALE_BLUE, m_Index);

	m_LG_rgb_enabled = (r != 1.0) || (g != 1.0) || (b != 1.0);
	
	if (m_LG_rgb_enabled) {
		m_LG_scaleRed = r;
		m_LG_scaleGreen = g;
		m_LG_scaleBlue = b;
	}

	SetFromValues();
}

void LightGroupWidget::SetFromValues()
{
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_ENABLE, m_LG_enable, m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE, m_LG_scale, m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE_RED, (m_LG_rgb_enabled ? m_LG_scaleRed : 1.0), m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE_GREEN, (m_LG_rgb_enabled ? m_LG_scaleGreen : 1.0), m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_SCALE_BLUE, (m_LG_rgb_enabled ? m_LG_scaleBlue : 1.0), m_Index);
	luxSetParameterValue(LUX_FILM, LUX_FILM_LG_TEMPERATURE, (m_LG_temperature_enabled ? m_LG_temperature : 0.0), m_Index);
}

void LightGroupWidget::SetWidgetsEnabled(bool enabled)
{
	
	ui->slider_gain->setEnabled(enabled);
	ui->label_gain->setEnabled(enabled);
	ui->spinBox_gain->setEnabled(enabled);
	ui->checkBox_enableRGB->setEnabled(enabled);
	ui->toolButton_colorpicker->setEnabled(enabled && m_LG_rgb_enabled);
	ui->checkBox_enableBB->setEnabled(enabled);
	ui->slider_colortemp->setEnabled(enabled && m_LG_temperature_enabled);
	ui->spinBox_colortemp->setEnabled(enabled && m_LG_temperature_enabled);
}

////////////////////////////////////////////////////////////////////
// Save and Load params using .ini files

void LightGroupWidget::SaveSettings( QString fName )
{
	QSettings settings( fName, QSettings::IniFormat );

	settings.beginGroup( QString("lightgroup_") + title );
	if ( settings.status() ) return;

	settings.setValue( "LG_title", title );
	settings.setValue( "LG_enable", m_LG_enable );
	settings.setValue( "LG_scale", m_LG_scale );
	settings.setValue( "LG_temperature_enabled", m_LG_temperature_enabled );
	settings.setValue( "LG_temperature", m_LG_temperature );
	settings.setValue( "LG_rgb_enabled", m_LG_rgb_enabled );
	settings.setValue( "LG_scaleRed", m_LG_scaleRed );
	settings.setValue( "LG_scaleGreen", m_LG_scaleGreen );
	settings.setValue( "LG_scaleBlue", m_LG_scaleBlue );
	settings.setValue( "LG_scaleX", m_LG_scaleX );
	settings.setValue( "LG_scaleY", m_LG_scaleY );

	settings.endGroup();
}

void LightGroupWidget::LoadSettings( QString fName )
{
	QSettings settings( fName, QSettings::IniFormat );

	settings.beginGroup( QString("lightgroup_") + title );
	if ( settings.status() ) return;

	title = settings.value("LG_title", "lightgroup" ).toString();
	m_LG_enable = settings.value("LG_enable", true ).toBool();
	m_LG_scale = settings.value("LG_scale", 1.0 ).toDouble();
	m_LG_temperature_enabled = settings.value("LG_temperature_enabled", false ).toBool();
	m_LG_temperature = settings.value("LG_temperature", 6500.0 ).toDouble();
	m_LG_rgb_enabled = settings.value("LG_rgb_enabled", false ).toBool();
	m_LG_scaleRed = settings.value("LG_scaleRed", 1.0 ).toDouble();
	m_LG_scaleGreen = settings.value("LG_scaleGreen", 1.0 ).toDouble();
	m_LG_scaleBlue = settings.value("LG_scaleBlue", 1.0 ).toDouble();
	m_LG_scaleX = settings.value("LG_scaleX", 1.0 ).toDouble();
	m_LG_scaleY = settings.value("LG_scaleY", 1.0 ).toDouble();

	settings.endGroup();

	SetFromValues();
	UpdateWidgetValues();

	emit valuesChanged();
}

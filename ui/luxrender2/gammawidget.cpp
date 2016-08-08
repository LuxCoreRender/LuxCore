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



//#include "api.h"

#include "gammawidget.hxx"


GammaWidget::GammaWidget(QWidget *parent) : QWidget(parent), ui(new Ui::GammaWidget)
{
	ui->setupUi(this);
	
	connect(ui->slider_gamma, SIGNAL(valueChanged(int)), this, SLOT(gammaChanged(int)));
	connect(ui->spinBox_gamma, SIGNAL(valueChanged(double)), this, SLOT(gammaChanged(double)));
	connect(ui->checkBox_CRF, SIGNAL(stateChanged(int)), this, SLOT(CRFChanged(int)));
	connect(ui->combo_CRF_List, SIGNAL(activated(QString)), this, SLOT(SetCRFPreset(QString)));

	ui->combo_CRF_List->addItem(tr("External..."));

	addPreset("Advantix 100", "Advantix_100CD");
	addPreset("Advantix 200", "Advantix_200CD");
	addPreset("Advantix 400", "Advantix_400CD");
	addPreset("Agfachrome CTPrecisa 200", "Agfachrome_ctpecisa_200CD");
	addPreset("Agfachrome CTPrecisa 100", "Agfachrome_ctprecisa_100CD");
	addPreset("Agfachrome rsx2 050", "Agfachrome_rsx2_050CD");
	addPreset("Agfachrome rsx2 100", "Agfachrome_rsx2_100CD");
	addPreset("Agfachrome rsx2 200", "Agfachrome_rsx2_200CD");
	addPreset("Agfacolor Futura 100", "Agfacolor_futura_100CD");
	addPreset("Agfacolor Futura 200", "Agfacolor_futura_200CD");
	addPreset("Agfacolor Futura 400", "Agfacolor_futura_400CD");
	addPreset("Agfacolor Futura II 100", "Agfacolor_futuraII_100CD");
	addPreset("Agfacolor Futura II 200", "Agfacolor_futuraII_200CD");
	addPreset("Agfacolor Futura II 400", "Agfacolor_futuraII_400CD");
	addPreset("Agfacolor HDC 100 Plus", "Agfacolor_hdc_100_plusCD");
	addPreset("Agfacolor HDC 200 Plus", "Agfacolor_hdc_200_plusCD");
	addPreset("Agfacolor HDC 400 Plus", "Agfacolor_hdc_400_plusCD");
	addPreset("Agfacolor Optima II 100", "Agfacolor_optimaII_100CD");
	addPreset("Agfacolor Optima II 200", "Agfacolor_optimaII_200CD");
	addPreset("Agfacolor Ultra 050", "Agfacolor_ultra_050_CD");
	addPreset("Agfacolor Vista 100", "Agfacolor_vista_100CD");
	addPreset("Agfacolor Vista 200", "Agfacolor_vista_200CD");
	addPreset("Agfacolor Vista 400", "Agfacolor_vista_400CD");
	addPreset("Agfacolor Vista 800", "Agfacolor_vista_800CD");
	addPreset("Agfapan APX 025 (B&W)", "Agfapan_apx_025CD");
	addPreset("Agfapan APX 100 (B&W)", "Agfapan_apx_100CD");
	addPreset("Agfapan APX 400 (B&W)", "Agfapan_apx_400CD");
	addPreset("Ektachrome 100 Plus (Color Rev.)", "Ektachrome_100_plusCD");
	addPreset("Ektachrome 100 (Color Rev.)", "Ektachrome_100CD");
	addPreset("Ektachrome 320T (Color Rev.)", "Ektachrome_320TCD");
	addPreset("Ektachrome 400X (Color Rev.)", "Ektachrome_400XCD");
	addPreset("Ektachrome 64 (Color Rev.)", "Ektachrome_64CD");
	addPreset("Ektachrome 64T (Color Rev.)", "Ektachrome_64TCD");
	addPreset("Ektachrome E100S", "Ektachrome_E100SCD");
	addPreset("Fujifilm Cine F-125", "F125CD");
	addPreset("Fujifilm Cine F-250", "F250CD");
	addPreset("Fujifilm Cine F-400", "F400CD");
	addPreset("Fujifilm Cine FCI", "FCICD");
	addPreset("Kodak Gold 100", "Gold_100CD");
	addPreset("Kodak Gold 200", "Gold_200CD");
	addPreset("Kodachrome 200", "Kodachrome_200CD");
	addPreset("Kodachrome 25", "Kodachrome_25CD");
	addPreset("Kodachrome 64", "Kodachrome_64CD");
	addPreset("Kodak Max Zoom 800", "Max_Zoom_800CD");
	addPreset("Kodak Portra 100T", "Portra_100TCD");
	addPreset("Kodak Portra 160NC", "Portra_160NCCD");
	addPreset("Kodak Portra 160VC", "Portra_160VCCD");
	addPreset("Kodak Portra 400NC", "Portra_400NCCD");
	addPreset("Kodak Portra 400VC", "Portra_400VCCD");
	addPreset("Kodak Portra 800", "Portra_800CD");
}

GammaWidget::~GammaWidget()
{
}

void GammaWidget::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::EnabledChange) {
		//// Reset from film when enabling in case values were not properly initialized
		//if (retrieveParam(false, LUX_FILM, LUX_FILM_TORGB_GAMMA) == m_TORGB_gamma)
		//	resetFromFilm(false);
		
		updateParam(LUX_FILM, LUX_FILM_TORGB_GAMMA, (this->isEnabled() ? m_TORGB_gamma : 1.0));
		updateParam(LUX_FILM, LUX_FILM_CAMERA_RESPONSE_ENABLED, this->isEnabled() && m_CRF_enabled);
		emit valuesChanged ();

	}
}

void GammaWidget::updateWidgetValues()
{
	// Gamma widgets
	updateWidgetValue(ui->slider_gamma, (int)((FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE) * m_TORGB_gamma));
	updateWidgetValue(ui->spinBox_gamma, m_TORGB_gamma);
	updateWidgetValue(ui->checkBox_CRF, m_CRF_enabled);
}

void GammaWidget::resetValues()
{
	m_TORGB_gamma = 2.2f;
	m_CRF_enabled = false;
	m_CRF_file = "";
}

void GammaWidget::resetFromFilm (bool useDefaults)
{
	m_CRF_enabled = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_CAMERA_RESPONSE_ENABLED) != 0.0;

	//ui->checkBox_CRF->setChecked(m_CRF_enabled);

	m_CRF_file = getStringAttribute("film", "CameraResponse");

	// not ideal as this will reset gamma
	// which wont happen if CRF was defined in the scene file
	// and eg luxconsole is used
	if (m_CRF_enabled)
		activateCRF();
	else
		deactivateCRF();

	// gamma must be handled after CRF
	// as (de)activateCRF modifies the gamma values
	m_TORGB_gamma = retrieveParam( useDefaults, LUX_FILM, LUX_FILM_TORGB_GAMMA);

	luxSetParameterValue(LUX_FILM, LUX_FILM_TORGB_GAMMA, m_TORGB_gamma);
}

void GammaWidget::gammaChanged (int value)
{
	gammaChanged ( (double)value / ( FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE ) );
}

void GammaWidget::gammaChanged (double value)
{
	m_TORGB_gamma = value;

	int sliderval = (int)((FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE) * m_TORGB_gamma);

	updateWidgetValue(ui->slider_gamma, sliderval);
	updateWidgetValue(ui->spinBox_gamma, m_TORGB_gamma);

	updateParam (LUX_FILM, LUX_FILM_TORGB_GAMMA, m_TORGB_gamma);

	emit valuesChanged ();
}
 
void GammaWidget::CRFChanged(int value)
{
	if (value == Qt::Checked)
		activateCRF();
	else
		deactivateCRF();
	
	emit valuesChanged ();
}

void GammaWidget::activateCRF()
{
	if(!m_CRF_file.isEmpty()) {
		m_CRF_enabled = true;

		QFileInfo fi(m_CRF_file);

		// Validate current selection...
		if ( m_CRF_file != ui->combo_CRF_List->itemData( ui->combo_CRF_List->currentIndex() ).toString() )
		{
			if ( fi.suffix().toLower() == "crf" || fi.suffix().toLower() == "txt" || fi.exists() )
			{
				ui->combo_CRF_List->insertItem( 1, fi.fileName(), QVariant( m_CRF_file ) );
				ui->combo_CRF_List->setCurrentIndex( 1 );
			}
			else // look for existing preset.
			{
				int lIndex = ui->combo_CRF_List->findData( QVariant( m_CRF_file ) );

				if ( lIndex != -1 ) ui->combo_CRF_List->setCurrentIndex( lIndex );
				else // Invalid preset...
				{
					m_CRF_enabled = false;
				}				
			}
		}

		updateWidgetValue(ui->checkBox_CRF, m_CRF_enabled);
		
		updateParam(LUX_FILM, LUX_FILM_CAMERA_RESPONSE_ENABLED, m_CRF_enabled);

		if ( m_CRF_enabled )
		{
#if defined(__APPLE__) // OSX locale is UTF-8, TODO check this for other OS
			luxSetStringAttribute("film", "CameraResponse", m_CRF_file.toUtf8().data());
#else
			luxSetStringAttribute("film", "CameraResponse", m_CRF_file.toAscii().data());
#endif
		}
	}
}

void GammaWidget::deactivateCRF()
{
	m_CRF_enabled = false;

	ui->gamma_label->setText("Gamma");
	updateWidgetValue(ui->slider_gamma, (int)((FLOAT_SLIDER_RES / TORGB_GAMMA_RANGE) * m_TORGB_gamma) );
	updateWidgetValue(ui->spinBox_gamma, m_TORGB_gamma);
	updateWidgetValue(ui->checkBox_CRF, m_CRF_enabled);
	updateParam (LUX_FILM, LUX_FILM_TORGB_GAMMA, m_TORGB_gamma);
	
	updateParam(LUX_FILM, LUX_FILM_CAMERA_RESPONSE_ENABLED, m_CRF_enabled);
}

void GammaWidget::loadCRF()
{
	m_CRF_file = QFileDialog::getOpenFileName(this, tr("Choose a CRF file to open"), m_lastOpendir, tr("Camera Response Files (*.crf *.txt)"));

	if(!m_CRF_file.isEmpty()) {
		QFileInfo info(m_CRF_file);
		m_lastOpendir = info.absolutePath();
	}
}

void GammaWidget::SetCRFPreset( QString sOption )
{
	if ( ui->combo_CRF_List->currentIndex() == 0 ) // Load from file.
	{
		loadCRF();

		if( !m_CRF_file.isEmpty() )
		{
			QFileInfo fTemp = m_CRF_file;

			ui->combo_CRF_List->insertItem( 1, fTemp.fileName(), QVariant( m_CRF_file ) );
			ui->combo_CRF_List->setCurrentIndex( 1 );
		}
	}
	else // Select existing preset.
	{
		QVariant vTemp = ui->combo_CRF_List->itemData( ui->combo_CRF_List->currentIndex() );

		if ( vTemp != QVariant::Invalid )
		{
			m_CRF_file = vTemp.toString();
		}
		else
		{
			m_CRF_file = sOption;
		}		
	}
	
	if( !m_CRF_file.isEmpty() )
	{
		activateCRF();
	}

	emit valuesChanged ();
}

void GammaWidget::addPreset( QString listName, QString realName )
{
	ui->combo_CRF_List->addItem( listName, QVariant( realName ) );
}

///////////////////////////////////////////
// Save and Load settings from a ini file.

void GammaWidget::SaveSettings( QString fName )
{
	QSettings settings( fName, QSettings::IniFormat );

	settings.beginGroup("gamma");
	if ( settings.status() ) return;

	settings.setValue( "TORGB_gamma", m_TORGB_gamma );
	settings.setValue( "CRF_enabled", m_CRF_enabled );
	settings.setValue( "CRF_file", m_CRF_file );

	settings.endGroup();
}

void GammaWidget::LoadSettings( QString fName )
{
	QSettings settings( fName, QSettings::IniFormat );

	settings.beginGroup("gamma");
	if ( settings.status() ) return;

	m_TORGB_gamma = settings.value("TORGB_gamma", 2.2 ).toDouble();
	m_CRF_enabled = settings.value("CRF_enabled", false ).toBool();
	m_CRF_file = settings.value("CRF_file", "" ).toString();

	settings.endGroup();

	updateWidgetValues();
	updateParam(LUX_FILM, LUX_FILM_TORGB_GAMMA, (this->isEnabled() ? m_TORGB_gamma : 1.0));

	if ( m_CRF_enabled )
	{
		activateCRF();
	}

	emit valuesChanged();
}

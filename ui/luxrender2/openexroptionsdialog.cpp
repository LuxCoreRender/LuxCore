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

#include "ui_openexroptionsdialog.h"
#include "openexroptionsdialog.hxx"

OpenEXROptionsDialog::OpenEXROptionsDialog(QWidget *parent, const bool halfFloats, const bool depthBuffer,
                                           const int compressionType) :
    QDialog(parent),
    ui(new Ui::OpenEXROptionsDialog)
{
    ui->setupUi(this);

    if(!halfFloats) { ui->halfFloatRadioButton->setChecked(false); ui->singleFloatRadioButton->setChecked(true); }
    if(depthBuffer) { ui->depthChannelCheckBox->setChecked(true); }
    if(compressionType != 1) ui->compressionTypeComboBox->setCurrentIndex(compressionType);
}

OpenEXROptionsDialog::~OpenEXROptionsDialog()
{
    delete ui;
}

void OpenEXROptionsDialog::disableZBufferCheckbox()
{
    // Uncheck Zbuf and disable
    ui->depthChannelCheckBox->setChecked(false);
    ui->depthChannelCheckBox->setEnabled(false);
}

bool OpenEXROptionsDialog::useHalfFloats() const { return ui->halfFloatRadioButton->isChecked(); }
bool OpenEXROptionsDialog::includeZBuffer() const { return ui->depthChannelCheckBox->isChecked(); }
int OpenEXROptionsDialog::getCompressionType() const { return ui->compressionTypeComboBox->currentIndex(); }

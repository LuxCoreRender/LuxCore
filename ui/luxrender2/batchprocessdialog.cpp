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

#include <QFileDialog>

#include "batchprocessdialog.hxx"
#include "ui_batchprocessdialog.h"

BatchProcessDialog::BatchProcessDialog(const QString &startingDir, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BatchProcessDialog)
{
    ui->setupUi(this);
	m_startDir = startingDir;
}

BatchProcessDialog::~BatchProcessDialog()
{
    delete ui;
}

bool BatchProcessDialog::individualLightGroups() { return ui->allLightGroupsModeRadioButton->isChecked(); }
QString BatchProcessDialog::inputDir() { return ui->inputDirectoryLineEdit->text(); }
QString BatchProcessDialog::outputDir() { return ui->outputDirectoryLineEdit->text(); }
bool BatchProcessDialog::applyTonemapping() { return ui->tonemapCheckBox->isChecked(); }
int BatchProcessDialog::format() { return ui->imageFormatComboBox->currentIndex(); }

void BatchProcessDialog::on_browseForInputDirectoryButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Input Directory"), m_startDir,
                                     QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isNull() || dir.isEmpty()) return;

    ui->inputDirectoryLineEdit->setText(dir);
}

void BatchProcessDialog::on_browseForOutputDirectoryButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"), m_startDir,
                                     QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(dir.isNull() || dir.isEmpty()) return;

    ui->outputDirectoryLineEdit->setText(dir);
}

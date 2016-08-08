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

#ifndef BATCHPROCESSDIALOG_H
#define BATCHPROCESSDIALOG_H

#include <QDialog>
#include <QString>
#include <QWidget>

namespace Ui {
    class BatchProcessDialog;
}

class BatchProcessDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchProcessDialog(const QString &startDir, QWidget *parent = 0);
    ~BatchProcessDialog();

    // Retrieve dialog specifics
    bool individualLightGroups();
    QString inputDir();
    QString outputDir();
    bool applyTonemapping();
    int format();

private slots:
    void on_browseForInputDirectoryButton_clicked();
    void on_browseForOutputDirectoryButton_clicked();

private:
    Ui::BatchProcessDialog *ui;
	QString m_startDir;
};

#endif // BATCHPROCESSDIALOG_H

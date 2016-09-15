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

#ifndef OPENEXROPTIONSDIALOG_H
#define OPENEXROPTIONSDIALOG_H

#include <QDialog>
#include <QWidget>

namespace Ui {
    class OpenEXROptionsDialog;
}

class OpenEXROptionsDialog : public QDialog
{
    Q_OBJECT

public:
    OpenEXROptionsDialog(QWidget *parent = 0, const bool halfFloats = true, const bool depthBuffer = false,
                         const int compressionType = 1);
    ~OpenEXROptionsDialog();

    void disableZBufferCheckbox();

    bool useHalfFloats() const;
    bool includeZBuffer() const;
    int getCompressionType() const;

private:
    Ui::OpenEXROptionsDialog *ui;
};

#endif // OPENEXROPTIONSDIALOG_H

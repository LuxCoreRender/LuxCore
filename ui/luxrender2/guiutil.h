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

#ifndef GUIUTIL_H
#define GUIUTIL_H

#include <sstream>

#include <cmath>
#include <vector>

#include <QFontMetrics>
#include <QImage>
#include <QList>
#include <QMessageBox>
#include <QPair>
#include <QString>
#include <QWidget>

#include <QAbstractButton>
#include <QChar>
#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QPushButton>
#include <QRectF>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>


#include "luxcorePlaceholder.h"

template<class T> inline T Clamp(T val, T low, T high) {
	return val > low ? (val < high ? val : high) : low;
}

int ValueToLogSliderVal(float value, const float logLowerBound, const float logUpperBound, const float slider_resolution);
float LogSliderValToValue(int sliderval, const float logLowerBound, const float logUpperBound, const float slider_resolution);

QString pathElidedText(const QFontMetrics &fm, const QString &text, int width, int flags = 0);

QString getStringAttribute(const char *objectName, const char *attributeName);
QString getAttributeDescription(const char *objectName, const char *attributeName);

void overlayStatistics(QImage *image);
QImage getFramebufferImage(bool overlayStats = false, bool outputAlpha = false);
bool saveCurrentImageTonemapped(const QString &outFile, bool overlayStats = false, bool outputAlpha = false);

// A list of button titles and associated button roles, stored as a QPair
typedef QList<QPair<QString, QMessageBox::ButtonRole> > CustomButtonsList;
/**
 * Displays a custom message box.
 * @param buttons A QList of QPairs, each containing a button title and the associated button role
 * @param defaultButton Index of the default button in the buttons list
 * @return Index of the clicked button in the buttons list, or -1 if user exited the dialog without click a button
 */
int customMessageBox(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text, 
	const CustomButtonsList &buttons, int defaultButton = 0);

class StrBufDialogBox : public std::stringbuf
{
public:
	StrBufDialogBox(QMessageBox::Icon icon) : icon(icon) {}
	virtual ~StrBufDialogBox() { if (str().size() > 0) sync(); }
	virtual int sync()
	{
		QMessageBox msgBox;
		msgBox.setIcon(QMessageBox::Information);
		msgBox.setText(str().c_str());
		msgBox.exec();
		return 0;
	}

private:
	QMessageBox::Icon icon;
};

#endif //GUIUTIL_H

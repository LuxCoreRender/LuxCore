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

#include "advancedinfowidget.hxx"

using namespace std;

AdvancedInfoWidget::AdvancedInfoWidget(QWidget *parent) : QWidget(parent), ui(new Ui::AdvancedInfoWidget)
{
	ui->setupUi(this);		
}

AdvancedInfoWidget::~AdvancedInfoWidget()
{
}

void AdvancedInfoWidget::updateWidgetValues()
{
	QStringList info;
	info.append("<html>");
	info.append("<head>");
	info.append("<style type=\"text/css\">");
	info.append("td { border-style: solid; padding-right: 0.5em; }");
	info.append("</style>");
	info.append("</head");
	info.append("<body>");

	info.append("<h3>Renderer</h3>");
	info.append("<p><table>");
	info.append("<tr>");
	info.append(QString("<td>Type:</td><td>%1</td>").arg(
		getStringAttribute("renderer", "name")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Surface integrator:</td><td>%1</td>").arg(
		getStringAttribute("surfaceintegrator", "name")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Volume integrator:</td><td>%1</td>").arg(
		getStringAttribute("volumeintegrator", "name")));
	info.append("</tr>");
	info.append("</table></p>");

	info.append("<h3>Film</h3>");
	info.append("<p><table>");
	info.append("<tr>");
	info.append(QString("<td>Resolution:</td><td>%1x%2</td>").arg(
		luxGetIntAttribute("film", "xResolution")).arg(
		luxGetIntAttribute("film", "yResolution")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Effective resolution:</td><td>%1x%2</td>").arg(
		luxGetIntAttribute("film", "xPixelCount")).arg(
		luxGetIntAttribute("film", "yPixelCount")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Premult. alpha:</td><td>%1</td>").arg(
		luxGetBoolAttribute("film", "premultiplyAlpha") ? tr("Yes") : tr("No")));
	info.append("</tr>");	
	info.append("<tr>");
	info.append(QString("<td>Output:</td><td>\"%1\"</td>").arg(
		getStringAttribute("film", "filename")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Write EXR:</td><td>%1</td>").arg(
		luxGetBoolAttribute("film", "write_EXR") ? tr("Yes") : tr("No")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Write PNG:</td><td>%1, %2</td>").arg(
		luxGetBoolAttribute("film", "write_PNG") ? tr("Yes") : tr("No")).arg(
		luxGetBoolAttribute("film", "write_PNG_16bit") ? tr("16bit") : tr("8bit")));
	info.append("</tr>");
	info.append("<tr>");
	info.append(QString("<td>Write FLM:</td><td>%1</td>").arg(
		luxGetBoolAttribute("film", "writeResumeFlm") ? tr("Yes") : tr("No")));
	info.append("</tr>");
	info.append("</table></p>");

	info.append("</body></html>");

	ui->textAdvancedInfo->setHtml(info.join("\n"));
}

void AdvancedInfoWidget::showEvent(QShowEvent *event)
{
	updateWidgetValues();

	QWidget::showEvent(event);
}

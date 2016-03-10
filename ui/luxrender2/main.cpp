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

#include <clocale>

#include <QTranslator>

#include "luxapp.hxx"

int main(int argc, char *argv[])
{
//	Q_INIT_RESOURCE(icons);
#if defined(__APPLE__) // for since OSX 10.9 changed default font
	QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
	QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Lucida Grande");
	QFont::insertSubstitution(".SF NS Text", "Lucida Grande");
#endif
	luxcore::Init();

	LuxGuiApp application(argc, argv);

/*	QString locale = QLocale::system().name();

	QTranslator translator;
	if (translator.load(QString("luxrender_") + locale))
		application.installTranslator(&translator);
*/

	// force C locale
	setlocale(LC_NUMERIC,"C");

	if (application.mainwin)
		return application.exec();
	else
		return 1;
}

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <cstdio>
#include <cstdlib>

#include "smalllux.h"
#include "mainwindow.h"
#include "luxmarkapp.h"

static void PrintCmdLineHelp(const QString &cmd) {
	cerr << "Usage: " << cmd.toAscii().data() << " [options]" << endl <<
			" --help <display this help and exit>" << endl <<
			" --scene=<LUXBALL_HDR|SALA|ROOM>" << endl;
}

int main(int argc, char **argv) {
	LuxMarkApp app(argc, argv);

	// Get the arguments into a list
	bool exit = false;

	QStringList argsList = app.arguments();
	QRegExp argHelp("--help");
	QRegExp argScene("--scene=(LUXBALL_HDR|SALA|ROOM)");

	LuxMarkAppMode mode = BENCHMARK_OCL_GPU;
	const char *scnName = SCENE_SALA;
    for (int i = 1; i < argsList.size(); ++i) {
        if (argHelp.indexIn(argsList.at(i)) != -1 ) {   
			PrintCmdLineHelp(argsList.at(0));
            exit = true;
			break;
		} else if (argScene.indexIn(argsList.at(i)) != -1 ) {   
            QString scene = argScene.cap(1);
			if (scene.compare("LUXBALL_HDR", Qt::CaseInsensitive) == 0)
				scnName = SCENE_LUXBALL_HDR;
			else if (scene.compare("SALA", Qt::CaseInsensitive) == 0)
				scnName = SCENE_SALA;
			else if (scene.compare("ROOM", Qt::CaseInsensitive) == 0)
				scnName = SCENE_ROOM;
			else {
				cerr << "Unknown scene name: " << argScene.cap(1).toAscii().data() << endl;
				PrintCmdLineHelp(argsList.at(0));
				exit = true;
				break;
			}	
        } else {
            cerr << "Unknown argument: " << argsList.at(i).toAscii().data() << endl;
			PrintCmdLineHelp(argsList.at(0));
			exit = true;
			break;
        }
    }

	if (exit)
		return EXIT_SUCCESS;
	else {
		app.Init(mode, scnName);

		// Force C locale
		setlocale(LC_NUMERIC,"C");

		if (app.mainWin != NULL)
			return app.exec();
		else
			return EXIT_FAILURE;
	}
}

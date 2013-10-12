/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include <iostream>

#include "luxcore/luxcore.h"

using namespace std;
using namespace luxcore;

//static void FreeImageErrorHandler(FREE_IMAGE_FORMAT fif, const char *message) {
//	printf("\n*** ");
//	if(fif != FIF_UNKNOWN)
//		printf("%s Format\n", FreeImage_GetFormatFromFIF(fif));
//
//	printf("%s", message);
//	printf(" ***\n");
//}

int main(int argc, char *argv[]) {
	try {
//		// Initialize FreeImage Library
//		FreeImage_Initialise(TRUE);
//		FreeImage_SetOutputMessage(FreeImageErrorHandler);

		//----------------------------------------------------------------------
		// Properties example
		//----------------------------------------------------------------------

		cout << "Properties example...\n";
		Properties props(
				(Property("test1.prop1") = 1, 2, 3) %
				(Property("test2.prop2") = "test"));
		cout << props;
	} catch (runtime_error err) {
		cerr << "RUNTIME ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	} catch (exception err) {
		cerr << "ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
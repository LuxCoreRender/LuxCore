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
#include <boost/foreach.hpp>

#include <luxcore/luxcore.h>

using namespace std;
using namespace luxrays;
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

		cout << "LuxCore " << LUXCORE_VERSION_MAJOR << "." << LUXCORE_VERSION_MINOR << "\n\n" ;

		{
			//------------------------------------------------------------------
			cout << "Property examples...\n";
			//------------------------------------------------------------------

			Property prop("test1.prop1", "aa");
			cout << "test1.prop1[0] => " << prop.GetValue<string>(0) << "\n\n";

			prop.Clear().Add(0).Add(2).Add(3);
			prop.Set(0, 1);
			cout << prop.ToString() << "\n\n";

			prop.Clear()(1.f, 2.f, 3.f);
			cout << prop.Get<luxrays::Vector>() << "\n\n";

			cout << prop.AddedNamePrefix("prefix.test.") << "\n\n";
			
			cout << prop.Renamed("rename.test") << "\n\n";

			prop = prop.Renamed("field0.field1.field2.field3")(0, 1, 2);
			cout << Property::ExtractField(prop.GetName(), 1) << "\n";
			cout << Property::ExtractPrefix(prop.GetName(), 1) << "\n";
			cout << Property::ExtractPrefix(prop.GetName(), 2) << "\n";
			cout << Property::ExtractPrefix(prop.GetName(), 3) << "\n";
			cout << Property::ExtractPrefix(prop.GetName(), 4) << "\n";
			cout << Property::ExtractPrefix(prop.GetName(), 5) << "\n";

			//------------------------------------------------------------------
			cout << "Properties examples...\n";
			//------------------------------------------------------------------

			Properties props(
					Property("test1.prop1")(1.f, 2.f, 3.f) <<
					Property("test2.prop2")("test"));
			cout << props << "\n";

			cout << "test1.prop1[0] => " << props.Get("test1.prop1").GetValue<int>(0) << "\n";
			cout << "test1.prop1[1] => " << props.Get("test1.prop1").GetValue<string>(1) << "\n";
			cout << "test1.prop1[2] => " << props.Get("test1.prop1").GetValue<float>(2) << "\n\n";

			props.Set(Property("test2.prop2")("overwrite"));
			cout << props.Get("test2.prop2") << "\n\n";
			
			props.Clear().SetFromString("test1.prop1 = 1.0, 2.0, 3.0 \"quoted\"\ntest2.prop2 = aa 'quoted' bb");
			cout << props;
			cout << "Size: " << props.Get("test1.prop1").GetSize() << "\n";
			cout << "Size: " << props.Get("test2.prop2").GetSize() << "\n\n";

			Properties props0(
					Property("test1.prop1.sub0.a")(1.f, 2.f, 3.f) <<
					Property("test1.prop1.sub0.b")("test1") <<
					Property("test1.prop2.sub1")("test2"));
			Properties props1(
					Property("test2.prop1")(1.f, 2.f, 3.f) <<
					Property("test2.prop2")("test"));
			props0.Set(props1);
			cout << props0 << "\n";

			vector<string> names0 = props0.GetAllUniqueSubNames("test1");
			BOOST_FOREACH(const string &n, names0)
				cout << "[" << n << "]";
			cout << "\n";
			vector<string> names1 = props0.GetAllUniqueSubNames("test1.prop1");
			BOOST_FOREACH(const string &n, names1)
				cout << "[" << n << "]";
			cout << "\n\n";

			cout << props0.Get("doesnt.exist.test", Property("default.property.test")("aaa")) << "\n";
		}
	} catch (runtime_error err) {
		cerr << "RUNTIME ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	} catch (exception err) {
		cerr << "ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
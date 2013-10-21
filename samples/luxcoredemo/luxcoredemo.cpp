/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#include <iostream>

#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <boost/format.hpp>

#include <luxcore/luxcore.h>

using namespace std;
using namespace luxrays;
using namespace luxcore;

int main(int argc, char *argv[]) {
	try {
		luxcore::Init();

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

			cout << props0.Get("doesnt.exist.test", MakePropertyValues(0, 1, 2)) << "\n\n";
		}
		
		//----------------------------------------------------------------------
		cout << "RenderConfig and RenderSession examples (requires scenes directory)...\n";
		//----------------------------------------------------------------------

		{
			//------------------------------------------------------------------
			cout << "A simple rendering...\n";
			//------------------------------------------------------------------

			// Load the configuration from file
			Properties props("scenes/luxball/luxball-hdr.cfg");

			// Change the render engine to PATHCPU
			props.Set(Property("renderengine.type")("PATHCPU"));

			RenderConfig *config = new RenderConfig(props);
			RenderSession *session = new RenderSession(config);

			session->Start();

			const double startTime = WallClockTime();
			for (;;) {
				boost::this_thread::sleep(boost::posix_time::millisec(1000));

				const double elapsedTime = WallClockTime() - startTime;

				// Print some information about the rendering progress

				// Update statistics
				session->UpdateStats();

				// Print all statistics
				//cout << "[Elapsed time: " << (int)elapsedTime << "/5]\n";
				//cout << session->GetStats();

				const Properties &stats = session->GetStats();
				cout << boost::format("[Elapsed time: %3d/5sec][Samples %4d][Avg. samples/sec % 3.2fM on %.1fK tris]\n") %
						(int)stats.Get("stats.renderengine.time").Get<double>() %
						stats.Get("stats.renderengine.pass").Get<u_int>() %
						(stats.Get("stats.renderengine.total.samplesec").Get<double>()  / 1000000.0) %
						(stats.Get("stats.dataset.trianglecount").Get<float>() / 1000.0);

				if (elapsedTime > 5.0) {
					// Time to stop the rendering
					break;
				}
			}

			session->Stop();

			// Save the rendered image
			session->SaveFilm();

			delete session;
			delete config;

			cout << "Done.\n";
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
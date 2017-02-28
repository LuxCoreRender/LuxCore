/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

		cout << "LuxCore v" << LUXCORE_VERSION_MAJOR << "." << LUXCORE_VERSION_MINOR << "\n\n" ;

		{
			//------------------------------------------------------------------
			cout << "Property examples...\n";
			//------------------------------------------------------------------

			Property prop("test1.prop1", "aa");
			cout << "test1.prop1[0] => " << prop.Get<string>(0) << "\n\n";

			prop.Clear().Add(0).Add(2).Add(3);
			prop.Set(0, 1);
			cout << prop.ToString() << "\n\n";

			prop.Clear()(1.f)(2.f)(3.f);
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

			prop = Property("test.matrix");
			Matrix4x4 m0;
			m0.m[3][0] = 1.f;
			prop.Add(m0);
			cout << prop << "\n";

			Matrix4x4 m1 = prop.Get<Matrix4x4>();
			for (u_int i = 0; i < 4; ++i)
				for (u_int j = 0; j < 4; ++j)
					if (m1.m[j][i] != m0.m[j][i])
						cout << "ERROR in Matrix4x4 test !\n";

			prop = Property("test.matrix")(m0);
			cout << prop << "\n\n";

			// String tests
			const string testStr("TEST123");
			Property p0("string.test", testStr);
			prop = p0;
			//prop = Property("string.test")(testStr);
			cout << "String:";
			if (prop.Get<string>() != testStr)
				cout << "\nERROR in string test !\n";
			cout << "String: " << prop << "\n\n";

			// Blob tests
			const size_t blobSize = 100;
			char blobData[blobSize];
			for (u_int i = 0; i < blobSize; ++i)
				blobData[i] = (char)i;

			Blob blob(blobData, blobSize);

			prop = Property("blob.test")(blob);

			cout << "Blob:";
			for (u_int i = 0; i < blobSize; ++i) {
				const Blob &b = prop.Get<const Blob &>();
			
				cout << " " << (int)b.GetData()[i];
				if (b.GetData()[i] != blobData[i])
					cout << "\nERROR in Blob test !\n";
			}
			cout << "\n";

			cout << "Blob ToString:\n========\n" << prop.Get<const Blob &>() << "\n========\n";

			Blob blob2(prop.Get<const Blob &>().ToString());
			cout << "Blob2 FromString:\n========\n" << blob2 << "\n========\n";

			cout << "Property Blob ToString: " << prop << "\n\n";

			//------------------------------------------------------------------
			cout << "Properties examples...\n";
			//------------------------------------------------------------------

			Properties props(
					Property("test1.prop1")(Vector(1.f, 2.f, 3.f)) <<
					Property("test2.prop2")("test"));
			cout << props << "\n";

			cout << "test1.prop1[0] => " << props.Get("test1.prop1").Get<int>(0) << "\n";
			cout << "test1.prop1[1] => " << props.Get("test1.prop1").Get<string>(1) << "\n";
			cout << "test1.prop1[2] => " << props.Get("test1.prop1").Get<float>(2) << "\n\n";

			props.Set(Property("test2.prop2")("overwrite"));
			cout << props.Get("test2.prop2") << "\n\n";
			
			props.Clear().SetFromString("test1.prop1 = 1.0 2.0 3.0 \"quoted\"\ntest2.prop2 = aa 'quoted' bb\n");
			cout << props;
			cout << "Size: " << props.Get("test1.prop1").GetSize() << "\n";
			cout << "Size: " << props.Get("test2.prop2").GetSize() << "\n\n";

			Properties props0(
					Property("test1.prop1.sub0.a")(Vector(1.f, 2.f, 3.f)) <<
					Property("test1.prop1.sub0.b")("test1") <<
					Property("test1.prop2.sub1")("test2"));
			Properties props1(
					Property("test2.prop1")(Vector(1.f, 2.f, 3.f)) <<
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

			cout << props0.Get(Property("doesnt.exist.test")(0, 1, 2)) << "\n\n";

			names0 = props0.GetAllNamesRE(".*\\.prop1\\..*");
			BOOST_FOREACH(const string &n, names0)
				cout << "[" << n << "]";
			cout << "\n\n";
		}

		//----------------------------------------------------------------------
		cout << "LuxRays device information example...\n";
		//----------------------------------------------------------------------

		{
			luxrays::Context ctx;
			const vector<luxrays::DeviceDescription *> &deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

			// Print device info
			for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
				luxrays::DeviceDescription *desc = deviceDescriptions[i];
				cout << "Device " << i << " name: " << desc->GetName() << "\n";
				cout << "Device " << i << " type: " << luxrays::DeviceDescription::GetDeviceType(desc->GetType()) << "\n";
				cout << "Device " << i << " compute units: " << desc->GetComputeUnits() << "\n";
				cout << "Device " << i << " preferred float vector width: " << desc->GetNativeVectorWidthFloat() << "\n";
				cout << "Device " << i << " max allocable memory: " << desc->GetMaxMemory() / (1024 * 1024) << "MBytes" << "\n";
				cout << "Device " << i << " max allocable memory block size: " << desc->GetMaxMemoryAllocSize() / (1024 * 1024) << "MBytes" << "\n";
			}
		}
		cout << "\n";

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
						(stats.Get("stats.dataset.trianglecount").Get<double>() / 1000.0);

				if (elapsedTime > 5.0) {
					// Time to stop the rendering
					break;
				}
			}

			session->Stop();

			// Save the rendered image
			session->GetFilm().SaveOutputs();

			delete session;
			delete config;

			cout << "Done.\n";
		}
	} catch (runtime_error &err) {
		cerr << "RUNTIME ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	} catch (exception &err) {
		cerr << "ERROR: " << err.what() << "\n";
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

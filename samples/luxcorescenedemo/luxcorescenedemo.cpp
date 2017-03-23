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

#include <limits>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/filesystem/operations.hpp>

#include <luxcore/luxcore.h>

using namespace std;
using namespace luxrays;
using namespace luxcore;

class UV {
public:
	UV(float _u = 0.f, float _v = 0.f)
	: u(_u), v(_v) {
	}

	UV(const float v[2]) : u(v[0]), v(v[1]) {
	}

	float u, v;
};

class Point {
public:
	Point(float _x = 0.f, float _y = 0.f, float _z = 0.f)
	: x(_x), y(_y), z(_z) {
	}

	Point(const float v[3]) : x(v[0]), y(v[1]), z(v[2]) {
	}

	float x, y, z;
};

class Triangle {
public:
	Triangle() { }
	Triangle(const unsigned int v0, const unsigned int v1, const unsigned int v2) {
		v[0] = v0;
		v[1] = v1;
		v[2] = v2;
	}

	unsigned int v[3];
};

class BBox {
public:
	// BBox Public Methods

	BBox() {
		pMin = Point(numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity(),
				numeric_limits<float>::infinity());
		pMax = Point(-numeric_limits<float>::infinity(),
				-numeric_limits<float>::infinity(),
				-numeric_limits<float>::infinity());
	}

	BBox(const Point &p1, const Point &p2) {
		pMin = p1;
		pMax = p2;
	}

	Point pMin, pMax;
};

static void CreateBox(Scene *scene, const string &objName, const string &meshName,
		const string &matName, const bool enableUV, const BBox &bbox) {
	Point *p = (Point *)Scene::AllocVerticesBuffer(24);
	// Bottom face
	p[0] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	p[1] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);
	p[2] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);
	p[3] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	// Top face
	p[4] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	p[5] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	p[6] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	p[7] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);
	// Side left
	p[8] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	p[9] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	p[10] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);
	p[11] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);
	// Side right
	p[12] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	p[13] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);
	p[14] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	p[15] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	// Side back
	p[16] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMin.z);
	p[17] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMin.z);
	p[18] = Point(bbox.pMax.x, bbox.pMin.y, bbox.pMax.z);
	p[19] = Point(bbox.pMin.x, bbox.pMin.y, bbox.pMax.z);
	// Side front
	p[20] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMin.z);
	p[21] = Point(bbox.pMin.x, bbox.pMax.y, bbox.pMax.z);
	p[22] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMax.z);
	p[23] = Point(bbox.pMax.x, bbox.pMax.y, bbox.pMin.z);

	Triangle *vi = (Triangle *)Scene::AllocTrianglesBuffer(12);
	// Bottom face
	vi[0] = Triangle(0, 1, 2);
	vi[1] = Triangle(2, 3, 0);
	// Top face
	vi[2] = Triangle(4, 5, 6);
	vi[3] = Triangle(6, 7, 4);
	// Side left
	vi[4] = Triangle(8, 9, 10);
	vi[5] = Triangle(10, 11, 8);
	// Side right
	vi[6] = Triangle(12, 13, 14);
	vi[7] = Triangle(14, 15, 12);
	// Side back
	vi[8] = Triangle(16, 17, 18);
	vi[9] = Triangle(18, 19, 16);
	// Side back
	vi[10] = Triangle(20, 21, 22);
	vi[11] = Triangle(22, 23, 20);

	// Define the Mesh
	if (!enableUV) {
		// Define the object
		scene->DefineMesh(meshName, 24, 12, (float *)p, (unsigned int *)vi, NULL, NULL, NULL, NULL);
	} else {
		UV *uv = new UV[24];
		// Bottom face
		uv[0] = UV(0.f, 0.f);
		uv[1] = UV(1.f, 0.f);
		uv[2] = UV(1.f, 1.f);
		uv[3] = UV(0.f, 1.f);
		// Top face
		uv[4] = UV(0.f, 0.f);
		uv[5] = UV(1.f, 0.f);
		uv[6] = UV(1.f, 1.f);
		uv[7] = UV(0.f, 1.f);
		// Side left
		uv[8] = UV(0.f, 0.f);
		uv[9] = UV(1.f, 0.f);
		uv[10] = UV(1.f, 1.f);
		uv[11] = UV(0.f, 1.f);
		// Side right
		uv[12] = UV(0.f, 0.f);
		uv[13] = UV(1.f, 0.f);
		uv[14] = UV(1.f, 1.f);
		uv[15] = UV(0.f, 1.f);
		// Side back
		uv[16] = UV(0.f, 0.f);
		uv[17] = UV(1.f, 0.f);
		uv[18] = UV(1.f, 1.f);
		uv[19] = UV(0.f, 1.f);
		// Side front
		uv[20] = UV(0.f, 0.f);
		uv[21] = UV(1.f, 0.f);
		uv[22] = UV(1.f, 1.f);
		uv[23] = UV(0.f, 1.f);

		// Define the object
		scene->DefineMesh(meshName, 24, 12, (float *)p, (unsigned int *)vi, NULL, (float *)uv, NULL, NULL);
	}

	// Add the object to the scene
	Properties props;
	props.SetFromString(
		"scene.objects." + objName + ".shape = " + meshName + "\n"
		"scene.objects." + objName + ".material = " + matName + "\n"
		);
	scene->Parse(props);
}

static void DoRendering(RenderSession *session) {
	const unsigned int haltTime = session->GetRenderConfig().GetProperties().Get(Property("batch.halttime")(0)).Get<unsigned int>();
	const unsigned int haltSpp = session->GetRenderConfig().GetProperties().Get(Property("batch.haltspp")(0)).Get<unsigned int>();
	const float haltThreshold = session->GetRenderConfig().GetProperties().Get(Property("batch.haltthreshold")(-1.f)).Get<float>();

	char buf[512];
	const Properties &stats = session->GetStats();
	for (;;) {
		boost::this_thread::sleep(boost::posix_time::millisec(1000));

		session->UpdateStats();
		const double elapsedTime = stats.Get("stats.renderengine.time").Get<double>();
		if ((haltTime > 0) && (elapsedTime >= haltTime))
			break;

		const unsigned int pass = stats.Get("stats.renderengine.pass").Get<unsigned int>();
		if ((haltSpp > 0) && (pass >= haltSpp))
			break;

		// Convergence test is update inside UpdateFilm()
		const float convergence = stats.Get("stats.renderengine.convergence").Get<unsigned int>();
		if ((haltThreshold >= 0.f) && (1.f - convergence <= haltThreshold))
			break;

		// Print some information about the rendering progress
		sprintf(buf, "[Elapsed time: %3d/%dsec][Samples %4d/%d][Convergence %f%%][Avg. samples/sec % 3.2fM on %.1fK tris]",
				int(elapsedTime), int(haltTime), pass, haltSpp, 100.f * convergence,
				stats.Get("stats.renderengine.total.samplesec").Get<double>() / 1000000.0,
				stats.Get("stats.dataset.trianglecount").Get<double>() / 1000.0);

		LC_LOG(buf);
	}

	// Save the rendered image
	session->GetFilm().SaveOutputs();
}

int main(int argc, char *argv[]) {
	try {
		luxcore::Init();

		cout << "LuxCore " << LUXCORE_VERSION_MAJOR << "." << LUXCORE_VERSION_MINOR << "\n" ;

		//----------------------------------------------------------------------
		// Build the scene to render
		//----------------------------------------------------------------------

		Scene *scene = Scene::Create();

		// Setup the camera
		scene->Parse(
				Property("scene.camera.lookat.orig")(1.f , 6.f , 3.f) <<
				Property("scene.camera.lookat.target")(0.f , 0.f , .5f) <<
				Property("scene.camera.fieldofview")(60.f));

		// Define texture maps
		const unsigned int size = 500;
		vector<unsigned char> img(size * size * 3);
		unsigned char *ptr = &img[0];
		for (unsigned int y = 0; y < size; ++y) {
			for (unsigned int x = 0; x < size; ++x) {
				if ((x % 50 < 25) ^ (y % 50 < 25)) {
					*ptr++ = 255;
					*ptr++ = 0;
					*ptr++ = 0;
				} else {
					*ptr++ = 255;
					*ptr++ = 255;
					*ptr++ = 0;
				}
			}
		}

		scene->DefineImageMap<unsigned char>("check_texmap", &img[0], 1.f, 3, size, size, Scene::DEFAULT);
		scene->Parse(
			Property("scene.textures.map.type")("imagemap") <<
			Property("scene.textures.map.file")("check_texmap") <<
			Property("scene.textures.map.gamma")(1.f)
			);

		// Setup materials
		scene->Parse(
			Property("scene.materials.whitelight.type")("matte") <<
			Property("scene.materials.whitelight.emission")(1000000.f, 1000000.f, 1000000.f) <<
			Property("scene.materials.mat_white.type")("matte") <<
			Property("scene.materials.mat_white.kd")("map") <<
			Property("scene.materials.mat_red.type")("matte") <<
			Property("scene.materials.mat_red.kd")(0.75f, 0.f, 0.f) <<
			Property("scene.materials.mat_glass.type")("glass") <<
			Property("scene.materials.mat_glass.kr")(0.9f, 0.9f, 0.9f) <<
			Property("scene.materials.mat_glass.kt")(0.9f, 0.9f, 0.9f) <<
			Property("scene.materials.mat_glass.exteriorior")(1.f) <<
			Property("scene.materials.mat_glass.interiorior")(1.4f) <<
			Property("scene.materials.mat_gold.type")("metal2") <<
			Property("scene.materials.mat_gold.preset")("gold")
			);

		// Create the ground
		CreateBox(scene, "ground", "mesh-ground", "mat_white", true, BBox(Point(-3.f, -3.f, -.1f), Point(3.f, 3.f, 0.f)));
		// Create the red box
		CreateBox(scene, "box01", "mesh-box01", "mat_red", false, BBox(Point(-.5f, -.5f, .2f), Point(.5f, .5f, .7f)));
		// Create the glass box
		CreateBox(scene, "box02", "mesh-box02", "mat_glass", false, BBox(Point(1.5f, 1.5f, .3f), Point(2.f, 1.75f, 1.5f)));
		// Create the light
		CreateBox(scene, "box03", "mesh-box03", "whitelight", false, BBox(Point(-1.75f, 1.5f, .75f), Point(-1.5f, 1.75f, .5f)));
		// Create a monkey from ply-file
		Properties props;
		props.SetFromString(
			"scene.shapes.monkey.type = mesh\n"
			"scene.shapes.monkey.ply = samples/luxcorescenedemo/suzanne.ply\n"	// load the ply-file
			"scene.objects.monkey.shape = monkey\n"		// set shape
			"scene.objects.monkey.material = mat_gold\n"		// set material
			"scene.objects.monkey.transformation = \
						0.4 0.0 0.0 0.0 \
						0.0 0.4 0.0 0.0 \
						0.0 0.0 0.4 0.0 \
					    0.0 2.0 0.3 1.0\n"						//scale and translate
			);
		scene->Parse(props);

		// Create a SkyLight & SunLight
		scene->Parse(
				Property("scene.lights.skyl.type")("sky") <<
				Property("scene.lights.skyl.dir")(0.166974f, 0.59908f, 0.783085f) <<
				Property("scene.lights.skyl.turbidity")(2.2f) <<
				Property("scene.lights.skyl.gain")(0.8f, 0.8f, 0.8f) <<
				Property("scene.lights.sunl.type")("sun") <<
				Property("scene.lights.sunl.dir")(0.166974f, 0.59908f, 0.783085f) <<
				Property("scene.lights.sunl.turbidity")(2.2f) <<
				Property("scene.lights.sunl.gain")(0.8f, 0.8f, 0.8f)
				);

		//----------------------------------------------------------------------
		// Do the render
		//----------------------------------------------------------------------

		RenderConfig *config = RenderConfig::Create(
				Property("renderengine.type")("PATHCPU") <<
				Property("sampler.type")("RANDOM") <<
				Property("opencl.platform.index")(-1) <<
				Property("opencl.cpu.use")(false) <<
				Property("opencl.gpu.use")(true) <<
				Property("batch.halttime")(10) <<
				Property("film.outputs.1.type")("RGB_IMAGEPIPELINE") <<
				Property("film.outputs.1.filename")("image.png"),
				scene);
		RenderSession *session = RenderSession::Create(config);

		//----------------------------------------------------------------------
		// Start the rendering
		//----------------------------------------------------------------------

		session->Start();

		DoRendering(session);
		boost::filesystem::rename("image.png", "image0.png");

		//----------------------------------------------------------------------
		// Edit a texture
		//----------------------------------------------------------------------

		LC_LOG("Editing a texture...");
		session->BeginSceneEdit();
		scene->Parse(
			Property("scene.textures.map.type")("constfloat3") <<
			Property("scene.textures.map.value")(0.f, 0.f, 1.f));
		session->EndSceneEdit();

		// And redo the rendering
		DoRendering(session);
		boost::filesystem::rename("image.png", "image1.png");

		//----------------------------------------------------------------------
		// Edit a material
		//----------------------------------------------------------------------

		LC_LOG("Editing a material...");
		session->BeginSceneEdit();
		scene->Parse(
			Property("scene.materials.mat_white.type")("mirror") <<
			Property("scene.materials.mat_white.kr")(.9f, .9f, .9f));
		session->EndSceneEdit();

		// And redo the rendering
		DoRendering(session);
		boost::filesystem::rename("image.png", "image2.png");

		//----------------------------------------------------------------------
		// Edit an object
		//----------------------------------------------------------------------

		LC_LOG("Editing a material and an object...");
		session->BeginSceneEdit();
		
		scene->Parse(
			Property("scene.materials.mat_white.type")("matte") <<
			Property("scene.materials.mat_white.kr")(.7f, .7f, .7f));
		CreateBox(scene, "box03", "mesh-box03", "mat_red", false, BBox(Point(-2.75f, 1.5f, .75f), Point(-.5f, 1.75f, .5f)));

		// Translate the monkey
		const float mat[16] = {
			.4f, 0.f, 0.f, 0.f,
			0.f, .4f, 0.f, 0.f,
			0.f, 0.f, .4f, 0.f,
			1.f, 2.f, .3f, 1.f
		};
		scene->UpdateObjectTransformation("monkey", mat);

		session->EndSceneEdit();

		// And redo the rendering
		DoRendering(session);
		boost::filesystem::rename("image.png", "image3.png");

		//----------------------------------------------------------------------
		// Add a strands object
		//----------------------------------------------------------------------

		LC_LOG("Adding a strands object...");
		session->BeginSceneEdit();

		{
			const string fileName = "scenes/strands/straight.hair";
			luxrays::cyHairFile strandsFile;
			const int strandsCount = strandsFile.LoadFromFile(fileName.c_str());
			if (strandsCount <= 0)
				throw runtime_error("Unable to read strands file: " + fileName);
			scene->DefineStrands("hairs_shape", strandsFile, Scene::TESSEL_RIBBON,
					0, 0.f, 0, false, false, true);

			// Add the object to the scene
			Properties props;
			props.SetFromString(
				"scene.objects.hairs_obj.shape = hairs_shape\n"
				"scene.objects.hairs_obj.material = mat_white\n"
				"scene.objects.hairs_obj.transformation = 0.01 0.0 0.0 0.0  0.0 0.01 0.0 0.0  0.0 0.0 0.01 0.0  -1.5 0.0 0.3 1.0\n"
				);
			scene->Parse(props);
		}

		session->EndSceneEdit();
			

		// And redo the rendering
		DoRendering(session);
		boost::filesystem::rename("image.png", "image4.png");

		//----------------------------------------------------------------------

		// Stop the rendering
		session->Stop();

		delete session;
		delete config;
		delete scene;

		LC_LOG("Done.");
	} catch (runtime_error &err) {
		LC_LOG("RUNTIME ERROR: " << err.what());
		return EXIT_FAILURE;
	} catch (exception &err) {
		LC_LOG("ERROR: " << err.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

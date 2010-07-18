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

#include <iostream>
#include <string>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>

#include "smalllux.h"
#include "telnet.h"
#include "renderconfig.h"

#include "luxrays/utils/properties.h"

using boost::asio::ip::tcp;

TelnetServer::TelnetServer(const unsigned int serverPort, RenderingConfig *renderConfig) : port(serverPort) {
	config = renderConfig;
	serverThread = new boost::thread(boost::bind(TelnetServer::ServerThreadImpl, this));
}

TelnetServer::~TelnetServer() {
	serverThread->interrupt();
	serverThread->join();
}

void TelnetServer::ServerThreadImpl(TelnetServer *telnetServer) {
	cerr << "[Telnet server] Thread started" << endl;

	try {
		ServerState state = RUN;

		boost::asio::io_service ioService;
		tcp::acceptor acceptor(ioService, tcp::endpoint(tcp::v4(), telnetServer->port));
		cerr << "[Telnet server] Server started on port: " << telnetServer->port << endl;

		for (;;) {
			tcp::socket socket(ioService);
			acceptor.accept(socket);

			// Handle the connection
			try {
				boost::asio::streambuf response;
				std::ostream respStream(&response);
				respStream << "SmallLuxGPU Telnet Server Interface\n";
				boost::asio::write(socket, response);

				for (bool exit = false; !exit;) {
					// Print prompt
					boost::asio::write(socket, boost::asio::buffer("> ", 2));

					// Read the command
					boost::asio::streambuf commandBuf;
					boost::asio::read_until(socket, commandBuf, "\n");

					std::istream commandStream(&commandBuf);
					string command;
					commandStream >> command;
					cerr << "[Telnet server] Received command: " << command << endl;

					if (command == "exit")
						exit = true;
					else if (command == "help") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);
						respStream << "exit - close the connection\n";
						respStream << "get <property name> - return the value of a (supported) property\n";
						respStream << "help - this help\n";
						respStream << "help.get - print the list of get supported properties\n";
						respStream << "help.set - print the list of set supported properties\n";
						respStream << "material.list - print the list of \n";
						respStream << "image.reset - reset the rendering image (requires render.stop)\n";
						respStream << "image.save - save the rendering image\n";
						respStream << "render.start - start the rendering\n";
						respStream << "render.stop - stop the rendering\n";
						respStream << "set <property name> = <values> - set the value of a (supported) property\n";
						respStream << "OK\n";
					    boost::asio::write(socket, response);
					} else if (command == "get") {
						//------------------------------------------------------
						// Get property
						//------------------------------------------------------

						// Get the name of the property to recover
						string property;
						commandStream >> property;

						// Check if is one of the supported properties
						if (property == "film.tonemap.linear.scale") {
							if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								LinearToneMapParams *params = (LinearToneMapParams *)telnetServer->config->film->GetToneMapParams();
								respStream << params->scale << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] Not using TONEMAP_LINEAR" << endl;
							}
						} else if (property == "film.tonemap.reinhard02.burn") {
							if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)telnetServer->config->film->GetToneMapParams();
								respStream << params->burn << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
							}
						} else if (property == "film.tonemap.reinhard02.postscale") {
							if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)telnetServer->config->film->GetToneMapParams();
								respStream << params->postScale << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
							}
						} else if (property == "film.tonemap.reinhard02.prescale") {
							if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)telnetServer->config->film->GetToneMapParams();
								respStream << params->preScale << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
							}
						} else if (property == "film.tonemap.type") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << telnetServer->config->film->GetToneMapParams()->GetType() << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.lookat") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Point &o = telnetServer->config->scene->camera->orig;
							const Point &t = telnetServer->config->scene->camera->target;
							respStream << o.x << " " << o.y << " " << o.z << " "
									<< t.x << " " << t.y << " " << t.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.motionblur.lookat") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Point &o = telnetServer->config->scene->camera->mbOrig;
							const Point &t = telnetServer->config->scene->camera->mbTarget;
							respStream << o.x << " " << o.y << " " << o.z << " "
									<< t.x << " " << t.y << " " << t.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.up") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Vector &up = telnetServer->config->scene->camera->up;
							respStream << up.x << " " << up.y << " " << up.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.motionblur.up") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Vector &up = telnetServer->config->scene->camera->mbUp;
							respStream << up.x << " " << up.y << " " << up.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.lensradius") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << telnetServer->config->scene->camera->lensRadius << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.fieldofview") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << telnetServer->config->scene->camera->fieldOfView << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.focaldistance") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << telnetServer->config->scene->camera->focalDistance << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.motionblur.enable") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << (telnetServer->config->scene->camera->motionBlur ? 1 : 0) << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "image.filename") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << telnetServer->config->cfg.GetString("image.filename", "image.png") << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.infinitelight.gain") {
							if (telnetServer->config->scene->infiniteLight &&
									(telnetServer->config->scene->infiniteLight->GetType() != TYPE_IL_SKY)) {
								std::ostream respStream(&response);
								const Spectrum gain = telnetServer->config->scene->infiniteLight->GetGain();
								respStream << gain.r << " " << gain.g << " " << gain.b << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No InfiniteLight defined: " << property << endl;
							}
						} else if (property == "scene.infinitelight.shift") {
							if (telnetServer->config->scene->infiniteLight &&
									(telnetServer->config->scene->infiniteLight->GetType() != TYPE_IL_SKY)) {
								std::ostream respStream(&response);
								const Spectrum gain = telnetServer->config->scene->infiniteLight->GetGain();
								respStream << telnetServer->config->scene->infiniteLight->GetShiftU() << " " <<
										telnetServer->config->scene->infiniteLight->GetShiftV() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No InfiniteLight defined: " << property << endl;
							}
						} else if (property == "scene.skylight.gain") {
							if (telnetServer->config->scene->infiniteLight &&
									(telnetServer->config->scene->infiniteLight->GetType() == TYPE_IL_SKY)) {
								std::ostream respStream(&response);
								const Spectrum gain = telnetServer->config->scene->infiniteLight->GetGain();
								respStream << gain.r << " " << gain.g << " " << gain.b << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No SkyLight defined: " << property << endl;
							}
						} else if (property == "scene.skylight.turbidity") {
							if (telnetServer->config->scene->infiniteLight &&
									(telnetServer->config->scene->infiniteLight->GetType() == TYPE_IL_SKY)) {
								SkyLight *sl = (SkyLight *)telnetServer->config->scene->infiniteLight;

								std::ostream respStream(&response);
								respStream << sl->GetTubidity() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No SkyLight defined: " << property << endl;
							}
						} else if (property == "scene.sunlight.turbidity") {
							// Look for the SunLight
							SunLight *sl = telnetServer->config->scene->GetSunLight();

							if (sl) {
								std::ostream respStream(&response);
								respStream << sl->GetTubidity() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No SunLight defined: " << property << endl;
							}
						} else if (property == "scene.sunlight.relsize") {
							// Look for the SunLight
							SunLight *sl = telnetServer->config->scene->GetSunLight();

							if (sl) {
								std::ostream respStream(&response);
								respStream << sl->GetRelSize() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No SunLight defined: " << property << endl;
							}
						} else if (property == "scene.sunlight.dir") {
							// Look for the SunLight
							SunLight *sl = telnetServer->config->scene->GetSunLight();

							if (sl) {
								std::ostream respStream(&response);
								const Vector &dir = sl->GetDir();
								respStream << dir.x << " " << dir.y << " " << dir.x << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] No SunLight defined: " << property << endl;
							}
						} else {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							cerr << "[Telnet server] Unknown property: " << property << endl;
						}
					} else if (command == "help.get") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);
						respStream << "film.tonemap.linear.scale\n";
						respStream << "film.tonemap.reinhard02.burn\n";
						respStream << "film.tonemap.reinhard02.postscale\n";
						respStream << "film.tonemap.reinhard02.prescale\n";
						respStream << "film.tonemap.type\n";
						respStream << "image.filename\n";
						respStream << "scene.camera.fieldofview\n";
						respStream << "scene.camera.focaldistance\n";
						respStream << "scene.camera.lensradius\n";
						respStream << "scene.camera.lookat\n";
						respStream << "scene.camera.motionblur.enable\n";
						respStream << "scene.camera.motionblur.lookat\n";
						respStream << "scene.camera.motionblur.up\n";
						respStream << "scene.camera.up\n";
						respStream << "scene.infinitelight.gain\n";
						respStream << "scene.infinitelight.shift\n";
						respStream << "scene.skylight.gain\n";
						respStream << "scene.skylight.turbidity\n";
						respStream << "scene.sunlight.dir\n";
						respStream << "scene.sunlight.relsize\n";
						respStream << "scene.sunlight.turbidity\n";
						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "help.set") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);
						respStream << "film.tonemap.linear.scale\n";
						respStream << "film.tonemap.reinhard02.burn\n";
						respStream << "film.tonemap.reinhard02.postscale\n";
						respStream << "film.tonemap.reinhard02.prescale\n";
						respStream << "film.tonemap.type\n";
						respStream << "image.filename\n";
						respStream << "scene.camera.fieldofview (requires render.stop)\n";
						respStream << "scene.camera.focaldistance (requires render.stop)\n";
						respStream << "scene.camera.lensradius (requires render.stop)\n";
						respStream << "scene.camera.lookat (requires render.stop)\n";
						respStream << "scene.camera.motionblur.enable (requires render.stop)\n";
						respStream << "scene.camera.motionblur.lookat (requires render.stop)\n";
						respStream << "scene.camera.motionblur.up (requires render.stop)\n";
						respStream << "scene.camera.up (requires render.stop)\n";
						respStream << "scene.materials.*.* (requires render.stop)\n";
						respStream << "scene.infinitelight.gain (requires render.stop)\n";
						respStream << "scene.infinitelight.shift (requires render.stop)\n";
						respStream << "scene.skylight.gain (requires render.stop)\n";
						respStream << "scene.skylight.turbidity (requires render.stop)\n";
						respStream << "scene.sunlight.dir (requires render.stop)\n";
						respStream << "scene.sunlight.relsize (requires render.stop)\n";
						respStream << "scene.sunlight.turbidity (requires render.stop)\n";
						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "image.reset") {
						if (state == STOP) {
							telnetServer->config->film->Reset();
							boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
						} else {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							cerr << "[Telnet server] Wrong state" << endl;
						}
					} else if (command == "material.list") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);

						std::map<std::string, size_t>::const_iterator iter =
								telnetServer->config->scene->materialIndices.begin();
						while (iter != telnetServer->config->scene->materialIndices.end()) {
							respStream << (*iter).first << "\n";
							iter++;
						}

						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "image.save") {
						std::string fileName = telnetServer->config->cfg.GetString("image.filename", "image.png");
						telnetServer->config->film->Save(fileName);
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if (command == "render.start") {
						if (state == STOP)
							telnetServer->config->StartAllRenderThreads();
						state = RUN;
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if (command == "render.stop") {
						if (state == RUN)
							telnetServer->config->StopAllRenderThreads();
						state = STOP;
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if (command == "set") {
						//------------------------------------------------------
						// Set property
						//------------------------------------------------------

						try {
							// Get the name of the property to set
							string property;
							getline(commandStream, property, '\n');
							Properties prop;
							const string propertyName = prop.SetString(property);
							cerr << "[Telnet server] Set: " << property << endl;

							// Check if is one of the supported properties
							if (propertyName == "film.tonemap.linear.scale") {
								const float k = prop.GetFloat(propertyName, 1.f);

								if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									LinearToneMapParams *params = (LinearToneMapParams *)telnetServer->config->film->GetToneMapParams()->Copy();
									params->scale = k;
									telnetServer->config->film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
								}
							} else if (propertyName == "film.tonemap.reinhard02.burn") {
								const float k = prop.GetFloat(propertyName, 3.75f);

								if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)telnetServer->config->film->GetToneMapParams()->Copy();
									params->burn = k;
									telnetServer->config->film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
								}
							} else if (propertyName == "film.tonemap.reinhard02.postscale") {
								const float k = prop.GetFloat(propertyName, 1.2f);

								if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)telnetServer->config->film->GetToneMapParams()->Copy();
									params->postScale = k;
									telnetServer->config->film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
								}
							} else if (propertyName == "film.tonemap.reinhard02.prescale") {
								const float k = prop.GetFloat(propertyName, 1.f);

								if (telnetServer->config->film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)telnetServer->config->film->GetToneMapParams()->Copy();
									params->preScale = k;
									telnetServer->config->film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Not using TONEMAP_REINHARD02" << endl;
								}
							} else if (propertyName == "film.tonemap.type") {
								const int type = prop.GetInt(propertyName, 0);

								if (type == 0) {
									LinearToneMapParams params;
									telnetServer->config->film->SetToneMapParams(params);
								} else {
									Reinhard02ToneMapParams params;
									telnetServer->config->film->SetToneMapParams(params);
								}

								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else if (propertyName == "image.filename") {
								// Get the image file name
								const string fileName = prop.GetString(propertyName, "image.png");

								telnetServer->config->cfg.SetString("image.filename", fileName);
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else if (propertyName == "scene.infinitelight.gain") {
								// Check if we are in the right state
								if (state == STOP) {
									if (telnetServer->config->scene->infiniteLight &&
											(telnetServer->config->scene->infiniteLight->GetType() != TYPE_IL_SKY)) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "1.0 1.0 1.0");
										Spectrum gain(vf.at(0), vf.at(1), vf.at(2));
										telnetServer->config->scene->infiniteLight->SetGain(gain);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										cerr << "[Telnet server] No InifinteLight defined: " << property << endl;
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.infinitelight.shift") {
								// Check if we are in the right state
								if (state == STOP) {
									if (telnetServer->config->scene->infiniteLight &&
											(telnetServer->config->scene->infiniteLight->GetType() != TYPE_IL_SKY)) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0");
										telnetServer->config->scene->infiniteLight->SetShift(vf.at(0), vf.at(1));
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										cerr << "[Telnet server] No InifinteLight defined: " << property << endl;
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.skylight.gain") {
								// Check if we are in the right state
								if (state == STOP) {
									if (telnetServer->config->scene->infiniteLight &&
											(telnetServer->config->scene->infiniteLight->GetType() == TYPE_IL_SKY)) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "1.0 1.0 1.0");
										Spectrum gain(vf.at(0), vf.at(1), vf.at(2));
										telnetServer->config->scene->infiniteLight->SetGain(gain);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										cerr << "[Telnet server] No SkyLight defined: " << property << endl;
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.skylight.turbidity") {
								// Check if we are in the right state
								if (state == STOP) {
									if (telnetServer->config->scene->infiniteLight &&
											(telnetServer->config->scene->infiniteLight->GetType() == TYPE_IL_SKY)) {
										SkyLight *sl = (SkyLight *)telnetServer->config->scene->infiniteLight;
										sl->SetTurbidity(prop.GetFloat(propertyName, 2.2f));
										sl->Init();
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										cerr << "[Telnet server] No SkyLight defined: " << property << endl;
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.sunlight.turbidity") {
								// Look for the SunLight
								SunLight *sl = telnetServer->config->scene->GetSunLight();

								if (sl) {
									sl->SetTurbidity(prop.GetFloat(propertyName, 2.2f));
									sl->Init();
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] No SunLight defined: " << property << endl;
								}
							} else if (propertyName == "scene.sunlight.relsize") {
								// Look for the SunLight
								SunLight *sl = telnetServer->config->scene->GetSunLight();

								if (sl) {
									sl->SetRelSize(prop.GetFloat(propertyName, 1.f));
									sl->Init();
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] No SunLight defined: " << property << endl;
								}
							} else if (propertyName == "scene.sunlight.dir") {
								// Look for the SunLight
								SunLight *sl = telnetServer->config->scene->GetSunLight();

								if (sl) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "1.0 1.0 1.0");
									Vector dir(vf.at(0), vf.at(1), vf.at(2));
									sl->SetDir(dir);
									sl->Init();
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] No SunLight defined: " << property << endl;
								}
							} else if (propertyName.find("scene.materials.") == 0) {
								if (state == STOP) {
									Scene *scene = telnetServer->config->scene;

									// Check if it is the name of a known material
									const std::string matType = Properties::ExtractField(propertyName, 2);
									if (matType == "")
										throw std::runtime_error("Syntax error in " + propertyName);
									const std::string matName = Properties::ExtractField(propertyName, 3);
									if (matName == "")
										throw std::runtime_error("Syntax error in " + propertyName);

									// Look for old material
									std::map<std::string, size_t>::const_iterator iter = scene->materialIndices.find(matName);
									if (iter == scene->materialIndices.end())
										throw std::runtime_error("Unknown material name: " + matName);

									Material *oldMat = scene->materials[iter->second];

									// Create new material
									Material *newMat = Scene::CreateMaterial(propertyName, prop);

									// Check if both are light sources
									if (oldMat->IsLightSource()) {
										if (!newMat->IsLightSource())
											throw std::runtime_error("New material must be a light source too");

										// Replace old light material with new one
										scene->materials[iter->second] = newMat;
										for (unsigned int i = 0; i < scene->triangleMaterials.size(); ++i) {
											if (scene->triangleMaterials[i] == oldMat)
												scene->triangleMaterials[i] = newMat;
										}
										for (unsigned int i = 0; i < scene->lights.size(); ++i) {
											if (scene->lights[i]->IsAreaLight()) {
												TriangleLight *tl = (TriangleLight *)scene->lights[i];
												if (tl->GetMaterial() == (LightMaterial *)oldMat)
													tl->SetMaterial((AreaLightMaterial *)newMat);
											}
										}
									} else {
										if (newMat->IsLightSource())
											throw std::runtime_error("New material must not be a light source too");

										// Replace old material with new one
										scene->materials[iter->second] = newMat;
										for (unsigned int i = 0; i < scene->triangleMaterials.size(); ++i) {
											if (scene->triangleMaterials[i] == oldMat)
												scene->triangleMaterials[i] = newMat;
										}
									}

									delete oldMat;

									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.lookat") {
								// Check if we are in the right state
								if (state == STOP) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "10.0 0.0 0.0  0.0 0.0 0.0");
									Point o(vf.at(0), vf.at(1), vf.at(2));
									Point t(vf.at(3), vf.at(4), vf.at(5));

									telnetServer->config->scene->camera->orig = o;
									telnetServer->config->scene->camera->target = t;
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.motionblur.lookat") {
								// Check if we are in the right state
								if (state == STOP) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "10.0 1.0 0.0  0.0 1.0 0.0");
									Point o(vf.at(0), vf.at(1), vf.at(2));
									Point t(vf.at(3), vf.at(4), vf.at(5));

									telnetServer->config->scene->camera->mbOrig = o;
									telnetServer->config->scene->camera->mbTarget = t;
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.up") {
								// Check if we are in the right state
								if (state == STOP) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0 0.1");
									Vector up(vf.at(0), vf.at(1), vf.at(2));

									telnetServer->config->scene->camera->up = Normalize(up);
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.motionblur.up") {
								// Check if we are in the right state
								if (state == STOP) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0 0.1");
									Vector up(vf.at(0), vf.at(1), vf.at(2));

									telnetServer->config->scene->camera->mbUp = Normalize(up);
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.lensradius") {
								// Check if we are in the right state
								if (state == STOP) {
									telnetServer->config->scene->camera->lensRadius = prop.GetFloat(propertyName, 0.f);
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.fieldofview") {
								// Check if we are in the right state
								if (state == STOP) {
									telnetServer->config->scene->camera->fieldOfView = prop.GetFloat(propertyName, 0.f);
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.focaldistance") {
								// Check if we are in the right state
								if (state == STOP) {
									telnetServer->config->scene->camera->focalDistance = prop.GetFloat(propertyName, 0.f);
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else if (propertyName == "scene.camera.motionblur.enable") {
								// Check if we are in the right state
								if (state == STOP) {
									telnetServer->config->scene->camera->motionBlur = (prop.GetInt(propertyName, 0) != 0);
									telnetServer->config->scene->camera->Update(telnetServer->config->film->GetWidth(),
											telnetServer->config->film->GetHeight());
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									cerr << "[Telnet server] Wrong state: " << property << endl;
								}
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								cerr << "[Telnet server] Unknown property: " << property << endl;
							}
						} catch (std::exception& e) {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							cerr << "[Telnet server] Error while setting a property: " << e.what() << endl;
						}
					} else {
						boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
						cerr << "[Telnet server] Unknown command" << endl;
					}
				}
			} catch (std::exception& e) {
				cerr << "[Telnet server] Connection error: " << e.what() << endl;
			}
		}
	} catch (std::exception& e) {
		cerr << "Telnet server error: " << e.what() << endl;
	}
}

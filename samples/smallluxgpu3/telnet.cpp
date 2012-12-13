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

#include "telnet.h"
#include "renderconfig.h"

#include "luxrays/utils/properties.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

using boost::asio::ip::tcp;

namespace slg {

TelnetServer::TelnetServer(const unsigned int serverPort, RenderSession *renderSession) : port(serverPort) {
	session = renderSession;
	serverThread = new boost::thread(boost::bind(TelnetServer::ServerThreadImpl, this));
}

TelnetServer::~TelnetServer() {
	serverThread->interrupt();
	serverThread->join();
}

void TelnetServer::ServerThreadImpl(TelnetServer *telnetServer) {
	SLG_LOG("[Telnet server] Thread started");

	try {
		ServerState state = RUN;

		boost::asio::io_service ioService;
		tcp::acceptor acceptor(ioService, tcp::endpoint(tcp::v4(), telnetServer->port));
		SLG_LOG("[Telnet server] Server started on port: " << telnetServer->port);

		for (;;) {
			tcp::socket socket(ioService);
			acceptor.accept(socket);

			// Handle the connection
			try {
				boost::asio::streambuf response;
				std::ostream respStream(&response);
				respStream << "SmallLuxGPU Telnet Server Interface\n";
				boost::asio::write(socket, response);

				bool echoCommandOn = true;
				for (bool exit = false; !exit;) {
					// Print prompt
					boost::asio::write(socket, boost::asio::buffer("> ", 2));

					// Read the command
					boost::asio::streambuf commandBuf;
					boost::asio::read_until(socket, commandBuf, "\n");

					std::istream commandStream(&commandBuf);
					string command;
					commandStream >> command;
					if (echoCommandOn)
						SLG_LOG("[Telnet server] Received command: " << command);

					RenderSession *session = telnetServer->session;
					Scene *scene = session->renderConfig->scene;
					Film *film = session->film;

					if (command == "exit")
						exit = true;
					else if (command == "help") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);
						respStream << "exit - close the connection\n";
						respStream << "get <property name> - return the value of a (supported) property\n";
						respStream << "echocmd.off\n";
						respStream << "echocmd.on\n";
						respStream << "edit.start - start an editing session (alias render.start for SLG 1.x compatibility)\n";
						respStream << "edit.stop - stop an editing session (alias render.stop for SLG 1.x compatibility)\n";
						respStream << "help - this help\n";
						respStream << "help.get - print the list of get supported properties\n";
						respStream << "help.set - print the list of set supported properties\n";
						respStream << "image.reset - reset the rendering image (requires edit.start)\n";
						respStream << "image.save - save the rendering image\n";
						respStream << "material.list - print the list of materials\n";
						respStream << "object.list - print the list of objects\n";
						respStream << "set <property name> = <values> - set the value of a (supported) property\n";
						respStream << "transmit.framebuffer.rgb_float - transmit the current framebuffer (RGB float format)\n";
						respStream << "transmit.framebuffer.rgb_byte - transmit the current framebuffer (RGB byte format)\n";
						respStream << "OK\n";
					    boost::asio::write(socket, response);
					} else if (command == "echocmd.off") {
						echoCommandOn = false;
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if (command == "echocmd.on") {
						echoCommandOn = true;
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if (command == "get") {
						//------------------------------------------------------
						// Get property
						//------------------------------------------------------

						// Get the name of the property to recover
						string property;
						commandStream >> property;

						// Check if is one of the supported properties
						if (property == "film.tonemap.linear.scale") {
							if (film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								LinearToneMapParams *params = (LinearToneMapParams *)film->GetToneMapParams();
								respStream << params->scale << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] Not using TONEMAP_LINEAR");
							}
						} else if (property == "film.tonemap.reinhard02.burn") {
							if (film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)film->GetToneMapParams();
								respStream << params->burn << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
							}
						} else if (property == "film.tonemap.reinhard02.postscale") {
							if (film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)film->GetToneMapParams();
								respStream << params->postScale << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
							}
						} else if (property == "film.tonemap.reinhard02.prescale") {
							if (film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
								boost::asio::streambuf response;
								std::ostream respStream(&response);
								Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)film->GetToneMapParams();
								respStream << params->preScale << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
							}
						} else if (property == "film.tonemap.type") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << film->GetToneMapParams()->GetType() << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.lookat") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Point &o = scene->camera->orig;
							const Point &t = scene->camera->target;
							respStream << o.x << " " << o.y << " " << o.z << " "
									<< t.x << " " << t.y << " " << t.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.up") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Vector &up = scene->camera->up;
							respStream << up.x << " " << up.y << " " << up.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.lensradius") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << scene->camera->lensRadius << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.fieldofview") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << scene->camera->fieldOfView << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.camera.focaldistance") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << scene->camera->focalDistance << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "image.filename") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << session->renderConfig->cfg.GetString("image.filename", "image.png") << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "scene.infinitelight.gain") {
							InfiniteLight *il = (InfiniteLight *)scene->GetLightByType(TYPE_IL);
							if (il) {
								std::ostream respStream(&response);
								const Spectrum gain = il->GetGain();
								respStream << gain.r << " " << gain.g << " " << gain.b << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No InfiniteLight defined: " << property);
							}
						} else if (property == "scene.infinitelight.shift") {
							InfiniteLight *il = (InfiniteLight *)scene->GetLightByType(TYPE_IL);
							if (il) {
								std::ostream respStream(&response);
								respStream << il->GetShiftU() << " " <<
										il->GetShiftV() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No InfiniteLight defined: " << property);
							}
						} else if (property == "scene.skylight.dir") {
							SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
							if (sl) {
								std::ostream respStream(&response);
								const Vector &dir = sl->GetSunDir();
								respStream << dir.x << " " << dir.y << " " << dir.x << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SkyLight defined: " << property);
							}
						} else if (property == "scene.skylight.gain") {
							SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
							if (sl) {
								std::ostream respStream(&response);
								const Spectrum gain = sl->GetGain();
								respStream << gain.r << " " << gain.g << " " << gain.b << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SkyLight defined: " << property);
							}
						} else if (property == "scene.skylight.turbidity") {
							SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
							if (sl) {
								std::ostream respStream(&response);
								respStream << sl->GetTubidity() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SkyLight defined: " << property);
							}
						} else if (property == "scene.sunlight.turbidity") {
							// Look for the SunLight
							SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
							if (sl) {
								std::ostream respStream(&response);
								respStream << sl->GetTubidity() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SunLight defined: " << property);
							}
						} else if (property == "scene.sunlight.relsize") {
							// Look for the SunLight
							SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
							if (sl) {
								std::ostream respStream(&response);
								respStream << sl->GetRelSize() << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SunLight defined: " << property);
							}
						} else if (property == "scene.sunlight.dir") {
							// Look for the SunLight
							SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
							if (sl) {
								std::ostream respStream(&response);
								const Vector &dir = sl->GetDir();
								respStream << dir.x << " " << dir.y << " " << dir.x << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SunLight defined: " << property);
							}
						} else if (property == "scene.sunlight.gain") {
							// Look for the SunLight
							SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
							if (sl) {
								std::ostream respStream(&response);
								const Spectrum &gain = sl->GetGain();
								respStream << gain.r << " " << gain.g << " " << gain.b << "\n";
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] No SunLight defined: " << property);
							}
						} else {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							SLG_LOG("[Telnet server] Unknown property: " << property);
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
						respStream << "scene.camera.up\n";
						respStream << "scene.infinitelight.gain\n";
						respStream << "scene.infinitelight.shift\n";
						respStream << "scene.skylight.dir\n";
						respStream << "scene.skylight.gain\n";
						respStream << "scene.skylight.turbidity\n";
						respStream << "scene.sunlight.dir\n";
						respStream << "scene.sunlight.gain\n";
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
						respStream << "scene.camera.fieldofview (requires edit.start)\n";
						respStream << "scene.camera.focaldistance (requires edit.start)\n";
						respStream << "scene.camera.lensradius (requires edit.start)\n";
						respStream << "scene.camera.lookat (requires edit.start)\n";
						respStream << "scene.camera.up (requires edit.start)\n";
						respStream << "scene.materials.*.* (requires edit.start)\n";
						respStream << "scene.infinitelight.gain (requires edit.start)\n";
						respStream << "scene.infinitelight.shift (requires edit.start)\n";
						respStream << "scene.objects.*.*.transform (requires edit.start)\n";
						respStream << "scene.objects.*.*.transformation (requires edit.start)\n";
						respStream << "scene.skylight.dir (requires edit.start)\n";
						respStream << "scene.skylight.gain (requires edit.start)\n";
						respStream << "scene.skylight.turbidity (requires edit.start)\n";
						respStream << "scene.sunlight.dir (requires edit.start)\n";
						respStream << "scene.sunlight.gain (requires edit.start)\n";
						respStream << "scene.sunlight.relsize (requires edit.start)\n";
						respStream << "scene.sunlight.turbidity (requires edit.start)\n";
						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "image.reset") {
						if (state == EDIT) {
							session->film->Reset();
							boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
						} else {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							SLG_LOG("[Telnet server] Wrong state");
						}
					} else if (command == "material.list") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);

						std::map<std::string, size_t>::const_iterator iter =
								scene->materialIndices.begin();
						while (iter != scene->materialIndices.end()) {
							respStream << (*iter).first << "\n";
							iter++;
						}

						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "object.list") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);

						std::map<std::string, size_t>::const_iterator iter =
								scene->objectIndices.begin();
						while (iter != scene->objectIndices.end()) {
							respStream << (*iter).first << "\n";
							iter++;
						}

						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "image.save") {
						session->SaveFilmImage();
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if ((command == "edit.stop") || (command == "render.start")) {
						if (state == EDIT)
							session->EndEdit();

						state = RUN;
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if ((command == "edit.start") || (command == "render.stop")) {
						if (state == RUN)
							session->BeginEdit();
						state = EDIT;
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
							if (echoCommandOn)
								SLG_LOG("[Telnet server] Set: " << property);

							// Check if is one of the supported properties
							if (propertyName == "film.tonemap.linear.scale") {
								const float k = prop.GetFloat(propertyName, 1.f);

								if (film->GetToneMapParams()->GetType() == TONEMAP_LINEAR) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									LinearToneMapParams *params = (LinearToneMapParams *)film->GetToneMapParams()->Copy();
									params->scale = k;
									film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
								}
							} else if (propertyName == "film.tonemap.reinhard02.burn") {
								const float k = prop.GetFloat(propertyName, 3.75f);

								if (film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)film->GetToneMapParams()->Copy();
									params->burn = k;
									film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
								}
							} else if (propertyName == "film.tonemap.reinhard02.postscale") {
								const float k = prop.GetFloat(propertyName, 1.2f);

								if (film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)film->GetToneMapParams()->Copy();
									params->postScale = k;
									film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
								}
							} else if (propertyName == "film.tonemap.reinhard02.prescale") {
								const float k = prop.GetFloat(propertyName, 1.f);

								if (film->GetToneMapParams()->GetType() == TONEMAP_REINHARD02) {
									boost::asio::streambuf response;
									std::ostream respStream(&response);
									Reinhard02ToneMapParams *params = (Reinhard02ToneMapParams *)film->GetToneMapParams()->Copy();
									params->preScale = k;
									film->SetToneMapParams(*params);
									delete params;
									respStream << "OK\n";
									boost::asio::write(socket, response);
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Not using TONEMAP_REINHARD02");
								}
							} else if (propertyName == "film.tonemap.type") {
								const int type = prop.GetInt(propertyName, 0);

								if (type == 0) {
									LinearToneMapParams params;
									film->SetToneMapParams(params);
								} else {
									Reinhard02ToneMapParams params;
									film->SetToneMapParams(params);
								}

								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else if (propertyName == "image.filename") {
								// Get the image file name
								const string fileName = prop.GetString(propertyName, "image.png");

								session->renderConfig->cfg.SetString("image.filename", fileName);
								respStream << "OK\n";
								boost::asio::write(socket, response);
							} else if (propertyName == "scene.infinitelight.gain") {
								// Check if we are in the right state
								if (state == EDIT) {
									InfiniteLight *il = (InfiniteLight *)scene->GetLightByType(TYPE_IL);
									if (il) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "1.0 1.0 1.0");
										Spectrum gain(vf.at(0), vf.at(1), vf.at(2));
										il->SetGain(gain);
										session->editActions.AddAction(INFINITELIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No InifinteLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.infinitelight.shift") {
								// Check if we are in the right state
								if (state == EDIT) {
									InfiniteLight *il = (InfiniteLight *)scene->GetLightByType(TYPE_IL);
									if (il) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0");
										il->SetShift(vf.at(0), vf.at(1));
										session->editActions.AddAction(INFINITELIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No InifinteLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.skylight.dir") {
								// Check if we are in the right state
								if (state == EDIT) {
									SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
									if (sl) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0 1.0");
										Vector dir(vf.at(0), vf.at(1), vf.at(2));
										sl->SetSunDir(dir);
										sl->Preprocess();
										session->editActions.AddAction(SKYLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SkyLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.skylight.gain") {
								// Check if we are in the right state
								if (state == EDIT) {
									SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
									if (sl) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "1.0 1.0 1.0");
										Spectrum gain(vf.at(0), vf.at(1), vf.at(2));
										sl->SetGain(gain);
										session->editActions.AddAction(SKYLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SkyLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.skylight.turbidity") {
								// Check if we are in the right state
								if (state == EDIT) {
									SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
									if (sl) {
										sl->SetTurbidity(prop.GetFloat(propertyName, 2.2f));
										sl->Preprocess();
										session->editActions.AddAction(SKYLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SkyLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.sunlight.turbidity") {
								if (state == EDIT) {
									// Look for the SunLight
									SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
									if (sl) {
										sl->SetTurbidity(prop.GetFloat(propertyName, 2.2f));
										sl->Preprocess();
										session->editActions.AddAction(SUNLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SunLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.sunlight.relsize") {
								if (state == EDIT) {
									// Look for the SunLight
									SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
									if (sl) {
										sl->SetRelSize(prop.GetFloat(propertyName, 1.f));
										sl->Preprocess();
										session->editActions.AddAction(SUNLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SunLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.sunlight.dir") {
								if (state == EDIT) {
									// Look for the SunLight
									SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
									if (sl) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0 1.0");
										Vector dir(vf.at(0), vf.at(1), vf.at(2));
										sl->SetDir(dir);
										sl->Preprocess();
										session->editActions.AddAction(SUNLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SunLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.sunlight.gain") {
								if (state == EDIT) {
									// Look for the SunLight
									SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
									if (sl) {
										const std::vector<float> vf = prop.GetFloatVector(propertyName, "1.0 1.0 1.0");
										Spectrum gain(vf.at(0), vf.at(1), vf.at(2));
										sl->SetGain(gain);
										sl->Preprocess();
										session->editActions.AddAction(SUNLIGHT_EDIT);
										respStream << "OK\n";
										boost::asio::write(socket, response);
									} else {
										boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
										SLG_LOG("[Telnet server] No SunLight defined: " << property);
									}
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName.find("scene.materials.") == 0) {
								if (state == EDIT) {
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
									// Check if the material type is one of the already enabled
									if (!session->renderEngine->IsMaterialCompiled(newMat->GetType()))
										session->editActions.AddAction(MATERIAL_TYPES_EDIT);
									// Check if both are light sources
									if (oldMat->IsLightSource()) {
										if (!newMat->IsLightSource())
											throw std::runtime_error("New material must be a light source too");

										// Replace old light material with new one
										scene->materials[iter->second] = newMat;
										for (unsigned int i = 0; i < scene->objectMaterials.size(); ++i) {
											if (scene->objectMaterials[i] == oldMat)
												scene->objectMaterials[i] = newMat;
										}
										for (unsigned int i = 0; i < scene->lights.size(); ++i) {
											if (scene->lights[i]->IsAreaLight()) {
												TriangleLight *tl = (TriangleLight *)scene->lights[i];
												if (tl->GetMaterial() == (LightMaterial *)oldMat)
													tl->SetMaterial((AreaLightMaterial *)newMat);
											}
										}

										session->editActions.AddAction(AREALIGHTS_EDIT);
									} else {
										if (newMat->IsLightSource())
											throw std::runtime_error("New material must not be a light source too");

										// Replace old material with new one
										scene->materials[iter->second] = newMat;
										for (unsigned int i = 0; i < scene->objectMaterials.size(); ++i) {
											if (scene->objectMaterials[i] == oldMat)
												scene->objectMaterials[i] = newMat;
										}
									}

									session->editActions.AddAction(MATERIALS_EDIT);
									delete oldMat;

									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if ((propertyName.find("scene.objects.") == 0) && (propertyName.find(".transform") == propertyName.size() - 10)) {
								if (state == EDIT) {
									// Check if it is the name of a known objects
									const std::string matType = Properties::ExtractField(propertyName, 2);
									if (matType == "")
										throw std::runtime_error("Syntax error in " + propertyName);
									const std::string objName = Properties::ExtractField(propertyName, 3);
									if (objName == "")
										throw std::runtime_error("Syntax error in " + propertyName);

									std::map<std::string, size_t>::const_iterator iter = scene->objectIndices.find(objName);
									if (iter == scene->objectIndices.end())
										throw std::runtime_error("Unknown object name: " + objName);

									const unsigned int meshIndex = iter->second;
									ExtMesh *obj = scene->objects[meshIndex];

									// Read the new transformation
									const std::vector<float> vf = prop.GetFloatVector(propertyName,
											"1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
									const Matrix4x4 mat(
											vf.at(0), vf.at(4), vf.at(8), vf.at(12),
											vf.at(1), vf.at(5), vf.at(9), vf.at(13),
											vf.at(2), vf.at(6), vf.at(10), vf.at(14),
											vf.at(3), vf.at(7), vf.at(11), vf.at(15));
									const Transform trans(mat);

									obj->ApplyTransform(trans);

									// Check if it is a light source
									if (scene->objectMaterials[meshIndex]->IsLightSource()) {
										// Have to update all light source using this mesh
										for (unsigned int i = 0; i < scene->lights.size(); ++i) {
											if (scene->lights[i]->GetType() == luxrays::sdl::TYPE_TRIANGLE) {
												TriangleLight *tl = (TriangleLight *)scene->lights[i];

												if (tl->GetMeshIndex() == meshIndex)
													tl->Init(scene->objects);
											}
										}

										// Some render engine requires a complete update when
										// modifing a light source
										session->editActions.AddAction(AREALIGHTS_EDIT);
									}

									// Set the flag to Update the DataSet
									if (obj->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
										if (session->renderConfig->scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH)
											session->editActions.AddAction(INSTANCE_TRANS_EDIT);
										else
											session->editActions.AddAction(GEOMETRY_EDIT);
									} else
										session->editActions.AddAction(GEOMETRY_EDIT);

									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if ((propertyName.find("scene.objects.") == 0) && (propertyName.find(".transformation") == propertyName.size() - 15)) {
								if (state == EDIT) {
									// Check if it is the name of a known objects
									const std::string matType = Properties::ExtractField(propertyName, 2);
									if (matType == "")
										throw std::runtime_error("Syntax error in " + propertyName);
									const std::string objName = Properties::ExtractField(propertyName, 3);
									if (objName == "")
										throw std::runtime_error("Syntax error in " + propertyName);

									std::map<std::string, size_t>::const_iterator iter = scene->objectIndices.find(objName);
									if (iter == scene->objectIndices.end())
										throw std::runtime_error("Unknown object name: " + objName);

									const unsigned int meshIndex = iter->second;
									ExtMesh *obj = scene->objects[meshIndex];

									// Check if the object is an instance
									if (obj->GetType() != TYPE_EXT_TRIANGLE_INSTANCE)
										throw std::runtime_error("Object must be an instance: " + objName);

									// Read the new transformation
									const std::vector<float> vf = prop.GetFloatVector(propertyName,
											"1.0 0.0 0.0 0.0  0.0 1.0 0.0 0.0  0.0 0.0 1.0 0.0  0.0 0.0 0.0 1.0");
									const Matrix4x4 mat(
											vf.at(0), vf.at(4), vf.at(8), vf.at(12),
											vf.at(1), vf.at(5), vf.at(9), vf.at(13),
											vf.at(2), vf.at(6), vf.at(10), vf.at(14),
											vf.at(3), vf.at(7), vf.at(11), vf.at(15));
									const Transform trans(mat);

									ExtInstanceTriangleMesh *iObj = (ExtInstanceTriangleMesh *)obj;
									iObj->SetTransformation(trans);

									// Check if it is a light source
									if (scene->objectMaterials[meshIndex]->IsLightSource()) {
										// Have to update all light source using this mesh
										for (unsigned int i = 0; i < scene->lights.size(); ++i) {
											if (scene->lights[i]->GetType() == luxrays::sdl::TYPE_TRIANGLE) {
												TriangleLight *tl = (TriangleLight *)scene->lights[i];

												if (tl->GetMeshIndex() == meshIndex)
													tl->Init(scene->objects);
											}
										}

										// Some render engine requires a complete update when
										// modifing a light source
										session->editActions.AddAction(AREALIGHTS_EDIT);
									}

									// Set the flag to Update the DataSet
									if (session->renderConfig->scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH)
										session->editActions.AddAction(INSTANCE_TRANS_EDIT);
									else
										session->editActions.AddAction(GEOMETRY_EDIT);

									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.camera.lookat") {
								// Check if we are in the right state
								if (state == EDIT) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "10.0 0.0 0.0  0.0 0.0 0.0");
									Point o(vf.at(0), vf.at(1), vf.at(2));
									Point t(vf.at(3), vf.at(4), vf.at(5));

									scene->camera->orig = o;
									scene->camera->target = t;
									scene->camera->Update(film->GetWidth(),
											film->GetHeight());
									session->editActions.AddAction(CAMERA_EDIT);
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.camera.up") {
								// Check if we are in the right state
								if (state == EDIT) {
									const std::vector<float> vf = prop.GetFloatVector(propertyName, "0.0 0.0 0.1");
									Vector up(vf.at(0), vf.at(1), vf.at(2));

									scene->camera->up = Normalize(up);
									scene->camera->Update(film->GetWidth(),
											film->GetHeight());
									session->editActions.AddAction(CAMERA_EDIT);
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.camera.lensradius") {
								// Check if we are in the right state
								if (state == EDIT) {
									scene->camera->lensRadius = prop.GetFloat(propertyName, 0.f);
									scene->camera->Update(film->GetWidth(),
											film->GetHeight());
									session->editActions.AddAction(CAMERA_EDIT);
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.camera.fieldofview") {
								// Check if we are in the right state
								if (state == EDIT) {
									scene->camera->fieldOfView = prop.GetFloat(propertyName, 0.f);
									scene->camera->Update(film->GetWidth(),
											film->GetHeight());
									session->editActions.AddAction(CAMERA_EDIT);
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else if (propertyName == "scene.camera.focaldistance") {
								// Check if we are in the right state
								if (state == EDIT) {
									scene->camera->focalDistance = prop.GetFloat(propertyName, 0.f);
									scene->camera->Update(film->GetWidth(),
											film->GetHeight());
									session->editActions.AddAction(CAMERA_EDIT);
									boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
								} else {
									boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
									SLG_LOG("[Telnet server] Wrong state: " << property);
								}
							} else {
								boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
								SLG_LOG("[Telnet server] Unknown property: " << property);
							}
						} catch (std::exception& e) {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							SLG_LOG("[Telnet server] Error while setting a property: " << e.what());
						}
					} else if ((command == "transmit.framebuffer.rgb_float")) {
						// Lock the film
						{
							boost::unique_lock<boost::mutex> lock(session->filmMutex);

							// Transmit the image size
							const unsigned int w = film->GetWidth();
							const unsigned int h = film->GetHeight();
							boost::asio::write(socket, boost::asio::buffer(boost::asio::const_buffer(&w, sizeof(unsigned int))));
							boost::asio::write(socket, boost::asio::buffer(boost::asio::const_buffer(&h, sizeof(unsigned int))));

							// Transmit the framebuffer (RGB float)
							boost::asio::write(socket, boost::asio::buffer(boost::asio::const_buffer(session->film->GetScreenBuffer(),
									sizeof(float) * w * h * 3)));
						}
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else if ((command == "transmit.framebuffer.rgb_byte")) {
						// Lock the film
						{
							boost::unique_lock<boost::mutex> lock(session->filmMutex);

							// Transmit the image size
							const unsigned int w = film->GetWidth();
							const unsigned int h = film->GetHeight();
							boost::asio::write(socket, boost::asio::buffer(boost::asio::const_buffer(&w, sizeof(unsigned int))));
							boost::asio::write(socket, boost::asio::buffer(boost::asio::const_buffer(&h, sizeof(unsigned int))));

							// Translate the framebuffer
							unsigned char *fb = new unsigned char[w * h * 3];
							unsigned char *dst = fb;
							const float *src = session->film->GetScreenBuffer();
							for (unsigned int i = 0; i < w * h; ++i) {
								*dst++ = (*src++) * 255.f + .5f;
								*dst++ = (*src++) * 255.f + .5f;
								*dst++ = (*src++) * 255.f + .5f;
							}

							// Transmit the framebuffer (RGB byte)
							boost::asio::write(socket, boost::asio::buffer(boost::asio::const_buffer(fb,
									sizeof(unsigned char) * w * h * 3)));
							delete fb;
						}
						boost::asio::write(socket, boost::asio::buffer("OK\n", 3));
					} else {
						boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
						SLG_LOG("[Telnet server] Unknown command");
					}
				}
			} catch (std::exception& e) {
				SLG_LOG("[Telnet server] Connection error: " << e.what());
			}
		}
	} catch (std::exception& e) {
		SLG_LOG("Telnet server error: " << e.what());
	}
}

}

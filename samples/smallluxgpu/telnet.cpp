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

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>

#include "telnet.h"
#include "renderconfig.h"

using namespace std;
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
						respStream << "image.save - save the renderingimage\n";
						respStream << "render.start - start the rendering\n";
						respStream << "render.stop - stop the rendering\n";
						respStream << "set <property name> <values>- set the value of a (supported) property\n";
						respStream << "OK\n";
					    boost::asio::write(socket, response);
					} else if (command == "get") {
						// Get the name of the property to recover
						string property;
						commandStream >> property;

						// Check if is one of the supported properties
						if (property == "scene.camera.lookat") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							const Point &o = telnetServer->config->scene->camera->orig;
							const Point &t = telnetServer->config->scene->camera->target;
							respStream << o.x << " " << o.y << " " << o.z << " "
									<< t.x << " " << t.y << " " << t.z << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else if (property == "image.filename") {
							boost::asio::streambuf response;
							std::ostream respStream(&response);
							respStream << telnetServer->config->cfg.GetString("image.filename", "image.png") << "\n";
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							cerr << "[Telnet server] Unknown property: " << property << endl;
						}
					} else if (command == "help.get") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);
						respStream << "image.filename\n";
						respStream << "scene.camera.lookat\n";
						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "help.set") {
						boost::asio::streambuf response;
						std::ostream respStream(&response);
						respStream << "image.filename\n";
						respStream << "OK\n";
						boost::asio::write(socket, response);
					} else if (command == "image.save") {
						std::string fileName = telnetServer->config->cfg.GetString("image.filename", "image.png");
						telnetServer->config->scene->camera->film->Save(fileName);
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
						// Get the name of the property to set
						string property;
						commandStream >> property;

						// Check if is one of the supported properties
						if (property == "image.filename") {
							// Get the image file name
							string fileName;
							commandStream >> fileName;

							telnetServer->config->cfg.SetString("image.filename", fileName);
							respStream << "OK\n";
							boost::asio::write(socket, response);
						} else {
							boost::asio::write(socket, boost::asio::buffer("ERROR\n", 6));
							cerr << "[Telnet server] Unknown property: " << property << endl;
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

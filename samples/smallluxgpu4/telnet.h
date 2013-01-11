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

#ifndef _TELNET_H
#define	_TELNET_H

#include "slg.h"
#include "rendersession.h"

namespace slg {

class TelnetServer {
public:
	TelnetServer(const unsigned int serverPort, RenderSession *renderSession);
	~TelnetServer();

private:
	typedef enum {
		RUN, EDIT
	} ServerState;

	static void ServerThreadImpl(TelnetServer *telnetServer);

	const unsigned int port;
	boost::thread *serverThread;

	RenderSession *session;
};

}

#endif	/* _TELNET_H */

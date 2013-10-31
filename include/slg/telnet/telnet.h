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

#ifndef _SLG_TELNET_H
#define	_SLG_TELNET_H

#include "slg/slg.h"
#include "slg/rendersession.h"

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

#endif	/* _SLG_TELNET_H */

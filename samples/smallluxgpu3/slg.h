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

#ifndef _SLG_H
#define	_SLG_H

#include <sstream>

#include "slgcfg.h"

/*! \namespace slg
 *
 * \brief The SLG core classes are defined within this namespace.
 */

namespace slg {
class RenderSession;
class RenderConfig;
class RenderEngine;
class EditActionList;
}

extern void SLGDebugHandler(const char *msg);

#define SLG_LOG(a) { std::stringstream _SLG_LOG_LOCAL_SS; _SLG_LOG_LOCAL_SS << a; SLGDebugHandler(_SLG_LOG_LOCAL_SS.str().c_str()); }

#endif	/* _LUXRAYS_H */

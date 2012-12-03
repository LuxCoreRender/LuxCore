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

#ifndef _LUXRAYS_SDL_EXTMESHCACHE_H
#define	_LUXRAYS_SDL_EXTMESHCACHE_H

#include <string>
#include <vector>
#include <map>

#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/context.h"
#include "luxrays/utils/core/exttrianglemesh.h"

namespace luxrays { namespace sdl {

class ExtMeshCache {
public:
	ExtMeshCache();
	~ExtMeshCache();

	void DefineExtMesh(const std::string &fileName,
		const long plyNbVerts, const long plyNbTris,
		Point *p, Triangle *vi, Normal *n, UV *uv,
		const bool usePlyNormals);

	ExtMesh *GetExtMesh(const std::string &fileName, const bool usePlyNormals);
	ExtMesh *GetExtMesh(const std::string &fileName, const bool usePlyNormals,
		const Transform &trans);

private:
	std::map<std::string, ExtTriangleMesh *> maps;
	std::vector<ExtMesh *> meshes;
};

} }

#endif	/* _LUXRAYS_SDL_EXTMESHCACHE_H */

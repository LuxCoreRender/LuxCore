#line 2 "mapping_types.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

//------------------------------------------------------------------------------
// TextureMapping2D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING2D
} TextureMapping2DType;

typedef struct {
    float uScale, vScale, uDelta, vDelta;
} UVMappingParam;


typedef struct {
	TextureMapping2DType type;
	union {
		UVMappingParam uvMapping2D;
	};
} TextureMapping2D;

//------------------------------------------------------------------------------
// TextureMapping3D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING3D, GLOBALMAPPING3D
} TextureMapping3DType;

typedef struct {
	TextureMapping3DType type;
	Transform worldToLocal;
	//union {
		// UVMapping3D has no parameters
		// GlobalMapping3D has no parameters
	//};
} TextureMapping3D;

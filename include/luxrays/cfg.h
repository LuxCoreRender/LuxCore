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

#ifndef _LUXRAYS_CFG_H
#define	_LUXRAYS_CFG_H

// The configured options and settings for LuxRays

#define LUXRAYS_VERSION_MAJOR "0"
#define LUXRAYS_VERSION_MINOR "1alpha6dev"

/* #undef LUXRAYS_DISABLE_OPENCL */

#if defined(__APPLE__)
#if (__GNUC__ == 3) || (__GNUC__ == 4)
extern "C" {
	int isinf(float);
	int isnan(double);
}
#endif
#endif

#if defined(WIN32)
#define isnan(a) _isnan(a)
#endif

#endif	/* _LUXRAYS_CFG_H */

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

#ifndef _OCL_UTILS_H
#define	_OCL_UTILS_H

#include <string>

#include "luxrays/luxrays.h"

#if !defined(LUXRAYS_DISABLE_OPENCL)

namespace luxrays { namespace utils {

extern std::string oclErrorString(cl_int error);

class oclKernelVolatileCache {
public:
	oclKernelVolatileCache();
	~oclKernelVolatileCache();

	cl::Program *Compile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource, bool *cached = NULL);
	
	static cl::Program *ForcedCompile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource);

private:
	std::map<std::string, cl::Program::Binaries> kernelCache;
};

} }

#endif

#endif	/* _LUXRAYS_FILM_FILM_H */

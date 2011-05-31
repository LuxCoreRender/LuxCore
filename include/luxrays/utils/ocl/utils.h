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

class oclKernelCache {
public:
	oclKernelCache() { }
	virtual ~oclKernelCache() { }

	virtual cl::Program *Compile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource, bool *cached = NULL) = 0;

	static cl::Program *ForcedCompile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource);
};

class oclKernelDummyCache : public oclKernelCache {
public:
	oclKernelDummyCache() { }
	~oclKernelDummyCache() { }

	cl::Program *Compile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource, bool *cached = NULL) {
		if (cached)
			cached = false;

		return ForcedCompile(context, device, kernelsParameters, kernelSource);
	}
};

class oclKernelVolatileCache : public oclKernelCache {
public:
	oclKernelVolatileCache();
	~oclKernelVolatileCache();

	cl::Program *Compile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource, bool *cached = NULL);

private:
	std::map<std::string, cl::Program::Binaries> kernelCache;
};

// WARNING: this class is not thread safe !
class oclKernelPersistentCache : public oclKernelCache {
public:
	oclKernelPersistentCache(const std::string &applicationName);
	~oclKernelPersistentCache();

	cl::Program *Compile(cl::Context &context, cl::Device &device,
		const std::string &kernelsParameters, const std::string &kernelSource, bool *cached = NULL);

private:
	static std::string HashString(const std::string &ss);

	std::string appName;
};

} }

#endif

#endif	/* _LUXRAYS_FILM_FILM_H */

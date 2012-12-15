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

#ifndef _METRIC_H
#define _METRIC_H

#include "luxrays/utils/convtest/pdiff/metric.h"

namespace luxrays { namespace utils {

class ConvergenceTest {
public:
	ConvergenceTest(const unsigned int w, const unsigned int h);
	virtual ~ConvergenceTest();

	void NeedTVI();
	const float *GetTVI() const { return tvi; }
	
	void Reset();
	void Reset(const unsigned int w, const unsigned int h);
	unsigned int Test(const float *image);

private:
	unsigned int width, height;
	
	float *reference;
	float *tvi;
};

} }

#endif


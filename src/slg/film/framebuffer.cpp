/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

#include "slg/film/framebuffer.h"

using namespace std;
using namespace luxrays;
using namespace slg;
OIIO_NAMESPACE_USING

typedef unsigned char BYTE;

template<> void GenericFrameBuffer<4, 1, float>::SaveHDR(const string &fileName) const {
	ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
	ImageBuf buffer(spec);

	for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
		u_int x = it.x();
		u_int y = it.y();
		float *pixel = (float *)buffer.pixeladdr(x, y, 0);
		y = height - y - 1;

		if (pixel == NULL)
			throw runtime_error("Error while unpacking film data, could not address buffer in GenericFrameBuffer<4, 1, float>::SaveHDR()");
		
		GetWeightedPixel(x, y, pixel);
	}

	if (!buffer.write(fileName))
		throw runtime_error("Error while writing an output type in GenericFrameBuffer<4, 1, float>::SaveHDR(): " +
					fileName + " (error = " + buffer.geterror() + ")");
}

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

#include <type_traits>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

#include "slg/film/framebuffer.h"

using namespace std;
using namespace luxrays;
using namespace slg;
OIIO_NAMESPACE_USING

template<> 
void GenericFrameBuffer<4, 1, float>::SaveHDR(const string &fileName,
		const vector<float> &pixels, const u_int width, const u_int height) {
	ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
	ImageBuf buffer(spec);

	for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
		u_int x = it.x();
		u_int y = it.y();
		float *dst = (float *)buffer.pixeladdr(x, y, 0);
		y = height - y - 1;

		if (dst == NULL)
			throw runtime_error("Error while unpacking film data, could not address buffer in GenericFrameBuffer<4, 1, float>::SaveHDR()");
		
		
		const float *src = &pixels[(x + y * width) * 4];
		const float k = 1.f / src[3];
		dst[0] = src[0] * k;
		dst[1] = src[1] * k;
		dst[2] = src[2] * k;
	}

	if (!buffer.write(fileName))
		throw runtime_error("Error while writing an output type in GenericFrameBuffer<4, 1, float>::SaveHDR(): " +
				fileName + " (error = " + buffer.geterror() + ")");
}

template<> 
void GenericFrameBuffer<3, 0, float>::SaveHDR(const string &fileName,
		const vector<float> &pixels, const u_int width, const u_int height) {
	ImageSpec spec(width, height, 3, TypeDesc::FLOAT);
	ImageBuf buffer(spec);

	for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
		u_int x = it.x();
		u_int y = it.y();
		float *dst = (float *)buffer.pixeladdr(x, y, 0);
		y = height - y - 1;

		if (dst == NULL)
			throw runtime_error("Error while unpacking film data, could not address buffer in GenericFrameBuffer<4, 1, float>::SaveHDR()");
		
		
		const float *src = &pixels[(x + y * width) * 3];
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}

	if (!buffer.write(fileName))
		throw runtime_error("Error while writing an output type in GenericFrameBuffer<4, 1, float>::SaveHDR(): " +
				fileName + " (error = " + buffer.geterror() + ")");
}

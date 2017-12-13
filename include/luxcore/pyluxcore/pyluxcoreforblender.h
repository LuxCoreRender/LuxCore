/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXCORE_FOR_BLENER_H
#define	_LUXCORE_FOR_BLENER_H

#include "luxcore/luxcoreimpl.h"

namespace luxcore {
namespace blender {

extern void ConvertFilmChannelOutput_3xFloat_To_4xUChar(const u_int width,
		const u_int height,	boost::python::object &objSrc, boost::python::object &objDst, const bool normalize);
extern boost::python::list ConvertFilmChannelOutput_3xFloat_To_3xFloatList(const u_int width,
		const u_int height,	boost::python::object &objSrc);
extern boost::python::list ConvertFilmChannelOutput_1xFloat_To_4xFloatList(const u_int width,
		const u_int height, boost::python::object &objSrc, const bool normalize);
extern boost::python::list ConvertFilmChannelOutput_2xFloat_To_4xFloatList(const u_int width,
		const u_int height,	boost::python::object &objSrc, const bool normalize);
extern boost::python::list ConvertFilmChannelOutput_3xFloat_To_4xFloatList(const u_int width,
		const u_int height, boost::python::object &objSrc, const bool normalize);
extern boost::python::list ConvertFilmChannelOutput_4xFloat_To_4xFloatList(const u_int width, 
		const u_int height, boost::python::object &objSrc, const bool normalize);

extern boost::python::list Scene_DefineBlenderMesh1(luxcore::detail::SceneImpl *scene, const std::string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr,
		const boost::python::object &transformation);
extern boost::python::list Scene_DefineBlenderMesh2(luxcore::detail::SceneImpl *scene, const std::string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr);
}
}

#endif


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

#ifndef _LUXCORE_FOR_BLENER_H
#define	_LUXCORE_FOR_BLENER_H

#include "luxcore/luxcoreimpl.h"

namespace luxcore {
namespace blender {

extern void ConvertFilmChannelOutput_1xFloat_To_1xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);
		
extern void ConvertFilmChannelOutput_UV_to_Blender_UV(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);
		
extern void ConvertFilmChannelOutput_1xFloat_To_4xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);
		
extern void ConvertFilmChannelOutput_3xFloat_To_3xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);
		
extern void ConvertFilmChannelOutput_3xFloat_To_4xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);
		
extern void ConvertFilmChannelOutput_4xFloat_To_4xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);
		
extern void ConvertFilmChannelOutput_1xUInt_To_1xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width,
		const u_int height, const size_t renderPassPtr, const bool normalize,
		const bool executeImagePipeline);

// Note: This method is used by pyluxcoredemo.py, do not remove.
extern void ConvertFilmChannelOutput_3xFloat_To_4xUChar(const u_int width, const u_int height,
		boost::python::object &objSrc, boost::python::object &objDst, const bool normalize);

extern boost::python::list BlenderMatrix4x4ToList(boost::python::object &blenderMatrix);

extern boost::python::list GetOpenVDBGridNames(const std::string &filePathStr);
extern boost::python::tuple GetOpenVDBGridInfo(const std::string &filePathStr, const std::string &gridName);


extern boost::python::list Scene_DefineBlenderMesh1(luxcore::detail::SceneImpl *scene, const std::string &name,
		const size_t loopTriCount, const size_t loopTriPtr,
		const size_t loopPtr,
		const size_t vertPtr,
		const size_t normalPtr,
		const size_t polyPtr,
		const size_t sharpPtr,
		const bool sharpAttr,
		const boost::python::object &loopUVsPtrList,
		const boost::python::object &loopColsPtrList,
		const size_t meshPtr,
		const u_int materialCount,
		const boost::python::object &transformation,
		const boost::python::tuple &blenderVersion,
		const boost::python::object& material_indices,
		const boost::python::object &loopTriCustomNormals);
		
extern boost::python::list Scene_DefineBlenderMesh2(luxcore::detail::SceneImpl *scene, const std::string &name,
		const size_t loopTriCount, const size_t loopTriPtr,
		const size_t loopPtr,
		const size_t vertPtr,
		const size_t normalPtr,
		const size_t polyPtr,
		const size_t sharpPtr,
		const bool sharpAttr,
		const boost::python::object &loopUVsPtrList,
		const boost::python::object &loopColsPtrList,
		const size_t meshPtr,
		const u_int materialCount,
		const boost::python::tuple &blenderVersion,
		const boost::python::object& material_indices,
		const boost::python::object &loopTriCustomNormals);
	

extern bool Scene_DefineBlenderStrands(luxcore::detail::SceneImpl *scene,
		const std::string &shapeName,
		const u_int pointsPerStrand,
		const boost::python::object &points,
		const boost::python::object &colors,
		const boost::python::object &uvs,
		const std::string &imageFilename,
		const float imageGamma,
		const bool copyUVs,
		const boost::python::object &transformation,
		const float strandDiameter,
		const float rootWidth,
		const float tipWidth,
		const float widthOffset,
		const std::string &tessellationTypeStr,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const boost::python::list &rootColor,
		const boost::python::list &tipColor);

} // namespace blender
} // namespace luxcore

#endif

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

#ifndef _LUXCORE_FOR_BLENDER_H
#define  _LUXCORE_FOR_BLENDER_H

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "luxcore/luxcoreimpl.h"

namespace py = pybind11;

namespace luxcore {
namespace blender {

using blender_ptr = size_t;

extern void ConvertFilmChannelOutput_1xFloat_To_1xFloatList(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

extern void ConvertFilmChannelOutput_UV_to_Blender_UV(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

extern void ConvertFilmChannelOutput_1xFloat_To_4xFloatList(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

extern void ConvertFilmChannelOutput_3xFloat_To_3xFloatList(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

extern void ConvertFilmChannelOutput_3xFloat_To_4xFloatList(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

extern void ConvertFilmChannelOutput_4xFloat_To_4xFloatList(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

extern void ConvertFilmChannelOutput_1xUInt_To_1xFloatList(py::object &filmObj,
    const Film::FilmOutputType outputType, const size_t outputIndex, const size_t width,
    const size_t height, blender_ptr renderPassPtr, const bool normalize,
    const bool executeImagePipeline);

// Note: This method is used by pyluxcoredemo.py, do not remove.
extern void ConvertFilmChannelOutput_3xFloat_To_4xUChar(const size_t width, const size_t height,
    py::object &objSrc, py::object &objDst, const bool normalize);

extern py::list BlenderMatrix4x4ToList(py::object &blenderMatrix);

extern py::list GetOpenVDBGridNames(const std::string &filePathStr);
extern py::tuple GetOpenVDBGridInfo(const std::string &filePathStr, const std::string &gridName);

using mesh_list = std::vector< std::tuple<std::string, size_t> >;

extern mesh_list Scene_DefineBlenderMesh1(
    luxcore::detail::SceneImpl *scene,
    const std::string &name,
    const size_t loopTriCount,
    blender_ptr loopTriPtr,
    blender_ptr loopTriPolyPtr,
    blender_ptr loopPtr,
    blender_ptr vertPtr,
    blender_ptr normalPtr,
    blender_ptr sharpPtr,
    const bool sharpAttr,
    const py::object& loopUVsPtrList,
    const py::object& loopColsPtrList,
    blender_ptr meshPtr,
    const size_t materialCount,
    const py::object& transformation,
    const py::tuple& blenderVersion,
    const py::object& material_indices,
    const py::object& loopTriCustomNormals);

extern mesh_list Scene_DefineBlenderMesh2(
    luxcore::detail::SceneImpl *scene,
    const std::string &name,
    const size_t loopTriCount,
    blender_ptr loopTriPtr,
    blender_ptr loopTriPolyPtr,
    blender_ptr loopPtr,
    blender_ptr vertPtr,
    blender_ptr normalPtr,
    blender_ptr sharpPtr,
    const bool sharpAttr,
    const py::object& loopUVsPtrList,
    const py::object& loopColsPtrList,
    blender_ptr meshPtr,
    const size_t materialCount,
    const py::tuple& blenderVersion,
    const py::object& material_indices,
    const py::object& loopTriCustomNormals);


extern bool Scene_DefineBlenderStrands(luxcore::detail::SceneImpl *scene,
    const std::string &shapeName,
    const size_t pointsPerStrand,
    const py::array_t<float> &points,
    const py::array_t<float> &colors,
    const py::array_t<float> &uvs,
    const std::string &imageFilename,
    const float imageGamma,
    const bool copyUVs,
    const py::object &transformation,
    const float strandDiameter,
    const float rootWidth,
    const float tipWidth,
    const float widthOffset,
    const std::string &tessellationTypeStr,
    const size_t adaptiveMaxDepth, const float adaptiveError,
    const size_t solidSideCount, const bool solidCapBottom, const bool solidCapTop,
    const py::list &rootColor,
    const py::list &tipColor);


extern bool Scene_DefineBlenderCurveStrands(luxcore::detail::SceneImpl* scene,
  const std::string& shapeName,
  const py::array_t<int>& pointsPerStrand,
  const py::array_t<float>& points,
  const py::array_t<float>& colors,
  const py::array_t<float>& uvs,
  const std::string& imageFilename,
  const float imageGamma,
  const bool copyUVs,
  const py::object& transformation,
  const float strandDiameter,
  const float rootWidth,
  const float tipWidth,
  const float widthOffset,
  const std::string& tessellationTypeStr,
  const size_t adaptiveMaxDepth, const float adaptiveError,
  const size_t solidSideCount, const bool solidCapBottom, const bool solidCapTop,
  const py::list& rootColor,
  const py::list& tipColor);

} // namespace blender
} // namespace luxcore

#endif

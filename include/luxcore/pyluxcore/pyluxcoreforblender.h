/***************************************************************************
 * Copyright 1998-2025 by authors (see AUTHORS.txt)                        *
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

namespace py = pybind11;



namespace luxcore {

namespace detail {
class SceneImpl;
}

namespace blender {

extern py::list BlenderMatrix4x4ToList(const py::array_t<float>& blenderMatrix);

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
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *   http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifdef WIN32
// Python 3.8 and older define snprintf as a macro even for VS 2015 and newer,
// where this causes an error - See https://bugs.python.org/issue36020
#if defined(_MSC_VER) && _MSC_VER >= 1900
#define HAVE_SNPRINTF
#endif
#endif

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <memory>
#include <vector>
#include <algorithm>
#include <boost/format.hpp>

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/dassert.h>

#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/pyluxcore/pyluxcoreforblender.h"
#include "luxrays/utils/utils.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;

namespace py = pybind11;
OIIO_NAMESPACE_USING

#define STREQ(a, b) (strcmp(a, b) == 0)

namespace {


static Transform ExtractTransformation(const py::object &transformation) {
  if (transformation.is_none()) {
	return Transform();
  }

  if (py::isinstance<py::list>(transformation)) {
	const py::list &lst = transformation.cast<py::list>();
	const py::ssize_t size = py::len(lst);
	if (size != 16) {
	  const string objType = py::cast<string>((transformation.attr("__class__")).attr("__name__"));
	  throw runtime_error("Wrong number of elements for the list of transformation values: " + objType);
	}

	luxrays::Matrix4x4 mat;
	py::ssize_t index = 0;
	for (u_int j = 0; j < 4; ++j)
	  for (u_int i = 0; i < 4; ++i)
		mat.m[i][j] = py::cast<float>(lst[index++]);

	return Transform(mat);
  }
  else {
	const string objType = py::cast<string>((transformation.attr("__class__")).attr("__name__"));
	throw runtime_error("Wrong data type for the list of transformation values: " + objType);
  }
}


//------------------------------------------------------------------------------
// Hair/strands conversion functions
//------------------------------------------------------------------------------

static bool nearlyEqual(const float a, const float b, const float epsilon) {
  return fabs(a - b) < epsilon;
}

static Spectrum getColorFromImage(const vector<float> &imageData, const float gamma,
               const size_t width, const size_t height, const size_t channelCount,
               const float u, const float v) {
  assert (width > 0);
  assert (height > 0);
  
   const size_t x = u * (width - 1);
  // The pixels coming from OIIO are flipped in y direction, so we flip v
  const size_t y = (1.f - v) * (height - 1);
  assert (x >= 0);
  assert (x < width);
  assert (y >= 0);
  assert (y < height);

  const size_t index = (width * y + x) * channelCount;

  if (channelCount == 1) {
    return Spectrum(powf(imageData[index], gamma));
  } else {
    // In case of channelCount == 4, we just ignore the alpha channel
    return Spectrum(powf(imageData[index], gamma),
            powf(imageData[index + 1], gamma),
            powf(imageData[index + 2], gamma));
  }
}


}

namespace luxcore {
namespace blender {




// Returns true if the shape could be defined successfully, false otherwise.
// root_width, tip_width and width_offset are percentages (range 0..1).
bool Scene_DefineBlenderStrands(
  luxcore::detail::SceneImpl* scene,
  const string& shapeName,
  const size_t pointsPerStrand,
  const py::array_t<float>& points,
  const py::array_t<float>& colors,
  const py::array_t<float>& uvs,
  const string& imageFilename,
  const float imageGamma,
  const bool copyUVs,
  const py::object& transformation,
  const float strandDiameter,
  const float rootWidth,
  const float tipWidth,
  const float widthOffset,
  const string& tessellationTypeStr,
  const size_t adaptiveMaxDepth, const float adaptiveError,
  const size_t solidSideCount, const bool solidCapBottom, const bool solidCapTop,
  const py::list& rootColor,
  const py::list& tipColor) {
  //--------------------------------------------------------------------------
  // Extract arguments (e.g. numpy arrays)
  //--------------------------------------------------------------------------

  if (pointsPerStrand == 0)
    throw runtime_error("pointsPerStrand needs to be greater than 0");

  // Points
  const auto& arrPoints = points;

  if (arrPoints.ndim() != 1)
    throw runtime_error("Points: Wrong number of dimensions (required: 1)");

  const float* const pointsStartPtr = reinterpret_cast<const float*>(arrPoints.data());
  const int pointArraySize = arrPoints.shape(0);
  const int pointStride = 3;
  const size_t inputPointCount = pointArraySize / pointStride;

  // Colors
  const auto& arrColors = colors;
  if (arrColors.ndim() != 1)
    throw runtime_error("Colors: Wrong number of dimensions (required: 1)");

  const float* const colorsStartPtr = reinterpret_cast<const float*>(arrColors.data());
  const int colorArraySize = arrColors.shape(0);
  const int colorStride = 3;
  // const size_t inputColorCount = colorArraySize / colorStride;
  const bool useVertexCols = colorArraySize > 0;

  // Root/tip colors
  if (len(rootColor) != 3)
    throw runtime_error("rootColor list has wrong length (required: 3)");
  if (len(tipColor) != 3)
    throw runtime_error("tipColor list has wrong length (required: 3)");
  const float rootColorR = py::cast<float>(rootColor[0]);
  const float rootColorG = py::cast<float>(rootColor[1]);
  const float rootColorB = py::cast<float>(rootColor[2]);
  const float tipColorR = py::cast<float>(tipColor[0]);
  const float tipColorG = py::cast<float>(tipColor[1]);
  const float tipColorB = py::cast<float>(tipColor[2]);
  const Spectrum rootCol(rootColorR, rootColorG, rootColorB);
  const Spectrum tipCol(tipColorR, tipColorG, tipColorB);
  const Spectrum white(1.f);
  // Since root and tip colors are multipliers, we don't need them if both are white
  const bool useRootTipColors = rootCol != white || tipCol != white;

  // UVs
  const auto& arrUVs = uvs;
  if (arrUVs.ndim() != 1)
    throw runtime_error("UVs: Wrong number of dimensions (required: 1)");

  const float* const uvsStartPtr = reinterpret_cast<const float*>(arrUVs.data());
  const int uvArraySize = arrUVs.shape(0);
  const int uvStride = 2;

  // If UVs are used, we expect one UV coord per strand (not per point)
  const int inputUVCount = uvArraySize / uvStride;
  const int inputStrandCount = inputPointCount / pointsPerStrand;
  if (uvArraySize > 0 && inputUVCount != inputStrandCount)
    throw runtime_error("UV array size is " + to_string(inputUVCount)
      + " (expected: " + to_string(inputStrandCount) + ")");

  if (copyUVs && uvArraySize == 0)
    throw runtime_error("Can not copy UVs without UV array");

  // Tessellation type
  Scene::StrandsTessellationType tessellationType;
  if (tessellationTypeStr == "ribbon")
    tessellationType = Scene::TESSEL_RIBBON;
  else if (tessellationTypeStr == "ribbonadaptive")
    tessellationType = Scene::TESSEL_RIBBON_ADAPTIVE;
  else if (tessellationTypeStr == "solid")
    tessellationType = Scene::TESSEL_SOLID;
  else if (tessellationTypeStr == "solidadaptive")
    tessellationType = Scene::TESSEL_SOLID_ADAPTIVE;
  else
    throw runtime_error("Unknown tessellation type: " + tessellationTypeStr);

  // Transformation
  bool hasTransformation = false;
  Transform trans;
  if (!transformation.is_none()) {
    trans = ExtractTransformation(transformation);
    hasTransformation = true;
  }

  //--------------------------------------------------------------------------
  // Load image if required
  //--------------------------------------------------------------------------

  vector<float> imageData;
  u_int width = 0;
  u_int height = 0;
  u_int channelCount = 0;

  if (uvArraySize > 0 && !imageFilename.empty()) {
    ImageSpec config;
    config.attribute("oiio:UnassociatedAlpha", 1);
    unique_ptr<ImageInput> in(ImageInput::open(imageFilename, &config));

    if (!in.get()) {
      throw runtime_error("Error opening image file : " + imageFilename +
        "\n" + geterror());
    }

    const ImageSpec& spec = in->spec();

    width = spec.width;
    height = spec.height;
    channelCount = spec.nchannels;

    if (channelCount != 1 && channelCount != 3 && channelCount != 4) {
      throw runtime_error("Unsupported number of channels (" + to_string(channelCount)
        + ") in image file: " + imageFilename
        + " (supported: 1, 3, or 4 channels)");
    }

    imageData.resize(width * height * channelCount);
    in->read_image(TypeDesc::FLOAT, &imageData[0]);
    in->close();
    in.reset();
  }

  if (!imageFilename.empty() && uvArraySize == 0)
    throw runtime_error("Image provided, but no UV data");

  const bool colorsFromImage = uvArraySize > 0 && !imageFilename.empty();
  if (useVertexCols && colorsFromImage)
    throw runtime_error("Can't copy colors from both image and color array");

  //--------------------------------------------------------------------------
  // Remove invalid points, create other arrays (segments, thickness etc.)
  //--------------------------------------------------------------------------

  // There can be invalid points, so we have to filter them
  constexpr float epsilon = 0.000000001f;
  const Point invalidPoint(0.f, 0.f, 0.f);

  vector<u_short> segments;
  segments.reserve(inputPointCount / pointsPerStrand);

  // We save the filtered points as raw floats so we can easily move later
  vector<float> filteredPoints;
  filteredPoints.reserve(pointArraySize);

  // We only need the thickness array if rootWidth and tipWidth are not equal.
  // Also, if the widthOffset is 1, there is no thickness variation.
  const bool useThicknessArray = !nearlyEqual(rootWidth, tipWidth, epsilon)
    && !nearlyEqual(widthOffset, 1.f, epsilon);
  vector<float> thickness;
  if (useThicknessArray) {
    thickness.reserve(inputPointCount);
  }
  else {
    thickness.push_back(strandDiameter * rootWidth);
  }

  const bool useColorsArray = colorsFromImage || useVertexCols || useRootTipColors;
  vector<float> filteredColors;
  if (useColorsArray) {
    filteredColors.reserve(inputPointCount * colorStride);
  }

  const bool useUVsArray = inputUVCount > 0 && copyUVs;
  vector<float> filteredUVs;
  if (useUVsArray) {
    filteredUVs.reserve(inputPointCount * uvStride);
  }

  const float* pointPtr = pointsStartPtr;
  const float* uvPtr = uvsStartPtr;
  const float* colorPtr = colorsStartPtr;

  while (pointPtr < (pointsStartPtr + pointArraySize)) {
    u_short validPointCount = 0;

    // We only have uv and color information for the first point of each strand
    float u = 0.f, v = 0.f, r = 1.f, g = 1.f, b = 1.f;
    if (useUVsArray || colorsFromImage) {
      u = *uvPtr++;
      v = *uvPtr++;
      // Bring u and v into range 0..1
      u -= floor(u);
      v -= floor(v);
    }
    if (useVertexCols) {
      r = *colorPtr++;
      g = *colorPtr++;
      b = *colorPtr++;
    }

    Point currPoint = Point(pointPtr);
    if (hasTransformation)
      currPoint *= trans;
    pointPtr += pointStride;
    Point lastPoint;

    // Iterate over the strand. We can skip step == 0.
    for (u_int step = 1; step < pointsPerStrand; ++step) {
      lastPoint = currPoint;
      currPoint = Point(pointPtr);
      if (hasTransformation)
        currPoint *= trans;
      pointPtr += pointStride;

      if (lastPoint == invalidPoint || currPoint == invalidPoint) {
        // Blender sometimes creates points that are all zeros, e.g. if
        // hair length is textured and an area is black (length == 0)
        continue;
      }

      const float segmentLengthSqr = DistanceSquared(currPoint, lastPoint);
      if (segmentLengthSqr < epsilon) {
        continue;
      }

      if (step == 1) {
        filteredPoints.push_back(lastPoint.x);
        filteredPoints.push_back(lastPoint.y);
        filteredPoints.push_back(lastPoint.z);
        validPointCount++;

        // The root point of a strand always uses the rootWidth
        if (useThicknessArray) {
          thickness.push_back(rootWidth * strandDiameter);
        }

        if (useUVsArray) {
          filteredUVs.push_back(u);
          filteredUVs.push_back(v);
        }

        Spectrum colPoint(1.f);

        if (colorsFromImage) {
          colPoint = getColorFromImage(imageData, imageGamma,
            width, height,
            channelCount, u, v);
        }

        if (useVertexCols) {
          colPoint = Spectrum(r, g, b);
        }

        if (useColorsArray) {
          if (useRootTipColors) {
            // We are in the root, no need to interpolate
            colPoint *= rootCol;
          }

          filteredColors.push_back(colPoint.c[0]);
          filteredColors.push_back(colPoint.c[1]);
          filteredColors.push_back(colPoint.c[2]);
        }
      }

      filteredPoints.push_back(currPoint.x);
      filteredPoints.push_back(currPoint.y);
      filteredPoints.push_back(currPoint.z);
      validPointCount++;

      if (useThicknessArray) {
        const float widthOffsetSteps = widthOffset * (pointsPerStrand - 1);

        if (step < widthOffsetSteps) {
          // We are still in the root part
          thickness.push_back(rootWidth * strandDiameter);
        }
        else {
          // We are above the root, interpolate thickness
          const float normalizedPosition = ((float)step - widthOffsetSteps)
            / (pointsPerStrand - 1 - widthOffsetSteps);
          const float relativeThick = Lerp(normalizedPosition, rootWidth, tipWidth);
          thickness.push_back(relativeThick * strandDiameter);
        }
      }

      if (useUVsArray) {
        filteredUVs.push_back(u);
        filteredUVs.push_back(v);
      }

      Spectrum colPoint(1.f);

      if (colorsFromImage) {
        colPoint = getColorFromImage(imageData, imageGamma,
          width, height,
          channelCount, u, v);
      }

      if (useVertexCols) {
        colPoint = Spectrum(r, g, b);
      }

      if (useColorsArray) {
        if (useRootTipColors) {
          if (step == pointsPerStrand - 1) {
            // We are in the root, no need to interpolate
            colPoint *= tipCol;
          }
          else {
            const float normalizedPosition = (float)step / (pointsPerStrand - 1);
            colPoint *= Lerp(normalizedPosition, rootCol, tipCol);
          }
        }

        filteredColors.push_back(colPoint.c[0]);
        filteredColors.push_back(colPoint.c[1]);
        filteredColors.push_back(colPoint.c[2]);
      }
    }

    if (validPointCount == 1) {
      // Can't make a segment with only one point, rollback
      for (int i = 0; i < pointStride; ++i)
        filteredPoints.pop_back();

      if (useThicknessArray)
        thickness.pop_back();

      if (useColorsArray) {
        for (int i = 0; i < colorStride; ++i)
          filteredColors.pop_back();
      }

      if (useUVsArray) {
        for (int i = 0; i < uvStride; ++i)
          filteredUVs.pop_back();
      }
    }
    else if (validPointCount > 1) {
      segments.push_back(validPointCount - 1);
    }
  }

  if (segments.empty()) {
    SLG_LOG("Aborting strand definition: Could not find valid segments!");
    return false;
  }

  const size_t pointCount = filteredPoints.size() / pointStride;

  if (pointCount != inputPointCount) {
    SLG_LOG("Removed " << (inputPointCount - pointCount) << " invalid points");
  }

  const bool allSegmentsEqual = std::adjacent_find(segments.begin(), segments.end(),
    std::not_equal_to<u_short>()) == segments.end();

  //--------------------------------------------------------------------------
  // Create hair file header
  //--------------------------------------------------------------------------

  luxrays::cyHairFile strands;
  strands.SetHairCount(segments.size());
  strands.SetPointCount(pointCount);

  int flags = CY_HAIR_FILE_POINTS_BIT;

  if (allSegmentsEqual) {
    strands.SetDefaultSegmentCount(segments.at(0));
  }
  else {
    flags |= CY_HAIR_FILE_SEGMENTS_BIT;
  }

  if (useThicknessArray)
    flags |= CY_HAIR_FILE_THICKNESS_BIT;
  else
    strands.SetDefaultThickness(thickness.at(0));

  // We don't need/support vertex alpha at the moment
  strands.SetDefaultTransparency(0.f);

  if (useColorsArray)
    flags |= CY_HAIR_FILE_COLORS_BIT;
  else
    strands.SetDefaultColor(1.f, 1.f, 1.f);

  if (useUVsArray)
    flags |= CY_HAIR_FILE_UVS_BIT;

  strands.SetArrays(flags);

  //--------------------------------------------------------------------------
  // Copy/move data into hair file
  //--------------------------------------------------------------------------

  if (!allSegmentsEqual) {
    move(segments.begin(), segments.end(), strands.GetSegmentsArray());
  }

  if (useThicknessArray) {
    move(thickness.begin(), thickness.end(), strands.GetThicknessArray());
  }

  if (useColorsArray) {
    move(filteredColors.begin(), filteredColors.end(), strands.GetColorsArray());
  }

  if (useUVsArray) {
    move(filteredUVs.begin(), filteredUVs.end(), strands.GetUVsArray());
  }

  move(filteredPoints.begin(), filteredPoints.end(), strands.GetPointsArray());

  const bool useCameraPosition = true;
  scene->DefineStrands(shapeName, strands,
    tessellationType, adaptiveMaxDepth, adaptiveError,
    solidSideCount, solidCapBottom, solidCapTop,
    useCameraPosition);

  return true;
}

// Returns true if the shape could be defined successfully, false otherwise.
// root_width, tip_width and width_offset are percentages (range 0..1).
bool Scene_DefineBlenderCurveStrands(luxcore::detail::SceneImpl* scene,
  const string& shapeName,
  const py::array_t<int>& pointsPerStrand,
  const py::array_t<float>& points,
  const py::array_t<float>& colors,
  const py::array_t<float>& uvs,
  const string& imageFilename,
  const float imageGamma,
  const bool copyUVs,
  const py::object& transformation,
  const float strandDiameter,
  const float rootWidth,
  const float tipWidth,
  const float widthOffset,
  const string& tessellationTypeStr,
  const size_t adaptiveMaxDepth,
  const float adaptiveError,
  const size_t solidSideCount,
  const bool solidCapBottom,
  const bool solidCapTop,
  const py::list& rootColor,
  const py::list& tipColor) {
  //--------------------------------------------------------------------------
  // Extract arguments (e.g. numpy arrays)
  //--------------------------------------------------------------------------

  //Points per strand
  const auto& arrPointsperStrand = pointsPerStrand;
  const auto& arrPoints = points;

  if (arrPointsperStrand.ndim() != 1)
    throw runtime_error("PointsperStrand: Wrong number of dimensions (required: 1)");


  if (arrPoints.ndim() != 1)
    throw runtime_error("Points: Wrong number of dimensions (required: 1)");

  const int* const pointsperStrand = reinterpret_cast<const int*>(arrPointsperStrand.data());
  const float* const pointsStartPtr = reinterpret_cast<const float*>(arrPoints.data());
  const int pointStride = 3;

  const size_t strandCount = arrPointsperStrand.shape(0);
  const size_t pointArraySize = arrPoints.shape(0);
  const size_t inputPointCount = pointArraySize / pointStride;

  cout << "strandCount: " << strandCount << endl;
  cout << "pointArraySize: " << pointArraySize << endl;

  // Colors
  const auto& arrColors = colors;
  if (arrColors.ndim() != 1)
    throw runtime_error("Colors: Wrong number of dimensions (required: 1)");


  const float* const colorsStartPtr = reinterpret_cast<const float*>(arrColors.data());
  const int colorArraySize = arrColors.shape(0);
  const int colorStride = 3;
  const size_t inputColorCount = colorArraySize / colorStride;
  const bool useVertexCols = colorArraySize > 0;

  // Root/tip colors
  if (len(rootColor) != 3)
    throw runtime_error("rootColor list has wrong length (required: 3)");
  if (len(tipColor) != 3)
    throw runtime_error("tipColor list has wrong length (required: 3)");
  const float rootColorR = py::cast<float>(rootColor[0]);
  const float rootColorG = py::cast<float>(rootColor[1]);
  const float rootColorB = py::cast<float>(rootColor[2]);
  const float tipColorR = py::cast<float>(tipColor[0]);
  const float tipColorG = py::cast<float>(tipColor[1]);
  const float tipColorB = py::cast<float>(tipColor[2]);
  const Spectrum rootCol(rootColorR, rootColorG, rootColorB);
  const Spectrum tipCol(tipColorR, tipColorG, tipColorB);
  const Spectrum white(1.f);

  // Since root and tip colors are multipliers, we don't need them if both are white
  const bool useRootTipColors = rootCol != white || tipCol != white;

  // UVs
  const auto& arrUVs = uvs;
  if (arrUVs.ndim() != 1)
    throw runtime_error("UVs: Wrong number of dimensions (required: 1)");

  const float* const uvsStartPtr = reinterpret_cast<const float*>(arrUVs.data());
  const int uvArraySize = arrUVs.shape(0);
  const int uvStride = 2;

  // If UVs are used, we expect one UV coord per strand (not per point)
  const int inputUVCount = uvArraySize / uvStride;

  if (uvArraySize > 0 && inputUVCount != strandCount)
    throw runtime_error("UV array size is " + to_string(inputUVCount)
      + " (expected: " + to_string(strandCount) + ")");

  if (copyUVs && uvArraySize == 0)
    throw runtime_error("Can not copy UVs without UV array");


  // Tessellation type
  Scene::StrandsTessellationType tessellationType;
  if (tessellationTypeStr == "ribbon")
    tessellationType = Scene::TESSEL_RIBBON;
  else if (tessellationTypeStr == "ribbonadaptive")
    tessellationType = Scene::TESSEL_RIBBON_ADAPTIVE;
  else if (tessellationTypeStr == "solid")
    tessellationType = Scene::TESSEL_SOLID;
  else if (tessellationTypeStr == "solidadaptive")
    tessellationType = Scene::TESSEL_SOLID_ADAPTIVE;
  else
    throw runtime_error("Unknown tessellation type: " + tessellationTypeStr);

  // Transformation
  bool hasTransformation = false;
  Transform trans;
  if (!transformation.is_none()) {
    trans = ExtractTransformation(transformation);
    hasTransformation = true;
  }

  //--------------------------------------------------------------------------
  // Load image if required
  //--------------------------------------------------------------------------

  vector<float> imageData;
  u_int width = 0;
  u_int height = 0;
  u_int channelCount = 0;

  if (uvArraySize > 0 && !imageFilename.empty()) {
    ImageSpec config;
    config.attribute("oiio:UnassociatedAlpha", 1);
    unique_ptr<ImageInput> in(ImageInput::open(imageFilename, &config));

    if (!in.get()) {
      throw runtime_error("Error opening image file : " + imageFilename +
        "\n" + geterror());
    }

    const ImageSpec& spec = in->spec();

    width = spec.width;
    height = spec.height;
    channelCount = spec.nchannels;

    if (channelCount != 1 && channelCount != 3 && channelCount != 4) {
      throw runtime_error("Unsupported number of channels (" + to_string(channelCount)
        + ") in image file: " + imageFilename
        + " (supported: 1, 3, or 4 channels)");
    }

    imageData.resize(width * height * channelCount);
    in->read_image(TypeDesc::FLOAT, &imageData[0]);
    in->close();
    in.reset();
  }

  if (!imageFilename.empty() && uvArraySize == 0)
    throw runtime_error("Image provided, but no UV data");

  const bool colorsFromImage = uvArraySize > 0 && !imageFilename.empty();
  if (useVertexCols && colorsFromImage)
    throw runtime_error("Can't copy colors from both image and color array");

  //--------------------------------------------------------------------------
  // Remove invalid points, create other arrays (segments, thickness etc.)
  //--------------------------------------------------------------------------

  // There can be invalid points, so we have to filter them
  const float epsilon = 0.000000001f;
  const Point invalidPoint(0.f, 0.f, 0.f);

  vector<u_short> segments;
  segments.reserve(pointArraySize / pointStride - strandCount);

  // We save the filtered points as raw floats so we can easily move later
  vector<float> filteredPoints;
  filteredPoints.reserve(pointArraySize);

  // We only need the thickness array if rootWidth and tipWidth are not equal.
  // Also, if the widthOffset is 1, there is no thickness variation.
  const bool useThicknessArray = !nearlyEqual(rootWidth, tipWidth, epsilon)
    && !nearlyEqual(widthOffset, 1.f, epsilon);
  vector<float> thickness;
  if (useThicknessArray) {
    thickness.reserve(pointArraySize);
  }
  else {
    thickness.push_back(strandDiameter * rootWidth);
  }

  const bool useColorsArray = colorsFromImage || useVertexCols || useRootTipColors;
  vector<float> filteredColors;
  if (useColorsArray) {
    filteredColors.reserve(inputPointCount * colorStride);
  }

  const bool useUVsArray = inputUVCount > 0 && copyUVs;
  vector<float> filteredUVs;
  if (useUVsArray) {
    filteredUVs.reserve(inputPointCount * uvStride);
  }

  const float* pointPtr = pointsStartPtr;

  const float* uvPtr = uvsStartPtr;
  const float* colorPtr = colorsStartPtr;

  int segment_count = 0;

  for (u_int strand = 0; strand < strandCount; ++strand) {
    u_short validPointCount = 0;

    // We only have uv and color information for the first point of each strand
    float u = 0.f, v = 0.f, r = 1.f, g = 1.f, b = 1.f;
    if (useUVsArray || colorsFromImage) {
      u = *uvPtr++;
      v = *uvPtr++;
      // Bring u and v into range 0..1
      u -= floor(u);
      v -= floor(v);
    }
    if (useVertexCols) {
      r = *colorPtr++;
      g = *colorPtr++;
      b = *colorPtr++;
    }

    Point currPoint = Point(pointPtr);
    if (hasTransformation)
      currPoint *= trans;
    pointPtr += pointStride;
    Point lastPoint;

    // Iterate over the strand. We can skip step == 0.
    for (u_int step = 1; step < pointsperStrand[strand]; ++step) {
      lastPoint = currPoint;
      currPoint = Point(pointPtr);
      if (hasTransformation)
        currPoint *= trans;
      pointPtr += pointStride;

      if (lastPoint == invalidPoint || currPoint == invalidPoint) {
        // Blender sometimes creates points that are all zeros, e.g. if
        // hair length is textured and an area is black (length == 0)
        continue;
      }

      const float segmentLengthSqr = DistanceSquared(currPoint, lastPoint);
      if (segmentLengthSqr < epsilon) {
        continue;
      }


      if (step == 1) {
        //cout << strand << "/0: (" << currPoint.x << ", " << currPoint.y << ", " << currPoint.z << ")" << endl;
        filteredPoints.push_back(lastPoint.x);
        filteredPoints.push_back(lastPoint.y);
        filteredPoints.push_back(lastPoint.z);
        validPointCount++;

        // The root point of a strand always uses the rootWidth
        if (useThicknessArray) {
          thickness.push_back(rootWidth * strandDiameter);
        }

        if (useUVsArray) {
          filteredUVs.push_back(u);
          filteredUVs.push_back(v);
        }

        Spectrum colPoint(1.f);

        if (colorsFromImage) {
          colPoint = getColorFromImage(imageData, imageGamma,
            width, height,
            channelCount, u, v);
        }

        if (useVertexCols) {
          colPoint = Spectrum(r, g, b);
        }

        if (useColorsArray) {
          if (useRootTipColors) {
            // We are in the root, no need to interpolate
            colPoint *= rootCol;
          }

          filteredColors.push_back(colPoint.c[0]);
          filteredColors.push_back(colPoint.c[1]);
          filteredColors.push_back(colPoint.c[2]);
        }
      }

      //cout << strand << "/" << step << ": (" << currPoint.x << ", " << currPoint.y << ", " << currPoint.z << ")" << endl;
      filteredPoints.push_back(currPoint.x);
      filteredPoints.push_back(currPoint.y);
      filteredPoints.push_back(currPoint.z);

      validPointCount++;
      /*
      if (useThicknessArray) {
        const float widthOffsetSteps = widthOffset * (pointsperStrand[strand] - 1);

        if (step < widthOffsetSteps) {
          // We are still in the root part
          thickness.push_back(rootWidth * strandDiameter);
        }
        else {
          // We are above the root, interpolate thickness
          const float normalizedPosition = ((float)step - widthOffsetSteps)
            / (pointsPerStrand - 1 - widthOffsetSteps);
          const float relativeThick = Lerp(normalizedPosition, rootWidth, tipWidth);
          thickness.push_back(relativeThick * strandDiameter);
        }
      }*/

      if (useUVsArray) {
        filteredUVs.push_back(u);
        filteredUVs.push_back(v);
      }

      Spectrum colPoint(1.f);

      if (colorsFromImage) {
        colPoint = getColorFromImage(imageData, imageGamma,
          width, height,
          channelCount, u, v);
      }

      if (useVertexCols) {
        colPoint = Spectrum(r, g, b);
      }

      if (useColorsArray) {
        if (useRootTipColors) {
          if (step == pointsperStrand[strand] - 1) {
            // We are in the root, no need to interpolate
            colPoint *= tipCol;
          }
          else {
            const float normalizedPosition = (float)step / (pointsperStrand[strand] - 1);
            colPoint *= Lerp(normalizedPosition, rootCol, tipCol);
          }
        }

        filteredColors.push_back(colPoint.c[0]);
        filteredColors.push_back(colPoint.c[1]);
        filteredColors.push_back(colPoint.c[2]);
      }
    }


    if (validPointCount == 1) {
      // Can't make a segment with only one point, rollback
      for (int i = 0; i < pointStride; ++i)
        filteredPoints.pop_back();
/*
      if (useThicknessArray)
        thickness.pop_back();
*/
      if (useColorsArray) {
        for (int i = 0; i < colorStride; ++i)
          filteredColors.pop_back();
      }

      if (useUVsArray) {
        for (int i = 0; i < uvStride; ++i)
          filteredUVs.pop_back();
      }
    }
    else if (validPointCount > 1) {
      segments.push_back(validPointCount - 1);
    }
  }

  if (segments.empty()) {
    SLG_LOG("Aborting strand definition: Could not find valid segments!");
    return false;
  }

  const size_t pointCount = filteredPoints.size() / pointStride;

  if (pointCount != inputPointCount) {
    SLG_LOG("Removed " << (inputPointCount - pointCount) << " invalid points");
  }

  const bool allSegmentsEqual = std::adjacent_find(segments.begin(), segments.end(),
    std::not_equal_to<u_short>()) == segments.end();

  //--------------------------------------------------------------------------
  // Create hair file header
  //--------------------------------------------------------------------------

  luxrays::cyHairFile strands;
  strands.SetHairCount(segments.size());
  strands.SetPointCount(filteredPoints.size() / pointStride);

  int flags = CY_HAIR_FILE_POINTS_BIT;

  if (allSegmentsEqual) {
    strands.SetDefaultSegmentCount(segments.at(0));
  }
  else {
    flags |= CY_HAIR_FILE_SEGMENTS_BIT;
  }

  if (useThicknessArray)
    flags |= CY_HAIR_FILE_THICKNESS_BIT;
  else
    strands.SetDefaultThickness(thickness.at(0));

  // We don't need/support vertex alpha at the moment
  strands.SetDefaultTransparency(0.f);

  if (useColorsArray)
    flags |= CY_HAIR_FILE_COLORS_BIT;
  else
    strands.SetDefaultColor(1.f, 1.f, 1.f);

  if (useUVsArray)
    flags |= CY_HAIR_FILE_UVS_BIT;

  strands.SetDefaultColor(1.f, 1.f, 1.f);
  strands.SetArrays(flags);

  //--------------------------------------------------------------------------
  // Copy/move data into hair file
  //--------------------------------------------------------------------------

  if (!allSegmentsEqual) {
    move(segments.begin(), segments.end(), strands.GetSegmentsArray());
  }
  /*
  if (useThicknessArray) {
    move(thickness.begin(), thickness.end(), strands.GetThicknessArray());
  }
  */
  if (useColorsArray) {
    move(filteredColors.begin(), filteredColors.end(), strands.GetColorsArray());
  }

  if (useUVsArray) {
    move(filteredUVs.begin(), filteredUVs.end(), strands.GetUVsArray());
  }

  move(filteredPoints.begin(), filteredPoints.end(), strands.GetPointsArray());

  const bool useCameraPosition = true;
  scene->DefineStrands(shapeName, strands,
    tessellationType, adaptiveMaxDepth, adaptiveError,
    solidSideCount, solidCapBottom, solidCapTop,
    useCameraPosition);

  return true;
}


}  // namespace blender
}  // namespace luxcore
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4

/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _LUXRAYS_MOTIONSYSTEM_H
#define _LUXRAYS_MOTIONSYSTEM_H

#include <string>

#include "luxrays/core/geometry/quaternion.h"
#include "luxrays/core/geometry/bbox.h"
using luxrays::BBox;
#include "luxrays/core/geometry/transform.h"
#include "luxrays/utils/properties.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/core/geometry/motionsystem_types.cl"
}

// Interpolates between two transforms
class InterpolatedTransform {
public:
	InterpolatedTransform() { startTime = endTime = 0; start = end = Transform(); }

	InterpolatedTransform(float st, float et,
		const Transform &s, const Transform &e);

	Matrix4x4 Sample(const float time) const;

	BBox Bound(BBox ibox, const bool storingGlobal2Local) const;

	// true if start and end transform or time is identical
	bool IsStatic() const {
		return !isActive;
	}

	class DecomposedTransform {
	public:
		DecomposedTransform() : Valid(false) {
		}

		// Decomposes the matrix m into a series of transformations
		// [Sx][Sy][Sz][Shearx/y][Sx/z][Sz/y][Rx][Ry][Rz][Tx][Ty][Tz][P(x,y,z,w)]
		// based on unmatrix() by Spencer W. Thomas from Graphic Gems II
		// TODO - implement extraction of perspective transform
		DecomposedTransform(const Matrix4x4 &m);

		// Scaling
		float Sx, Sy, Sz;
		// Shearing
		float Sxy, Sxz, Syz;
		// Rotation
		Matrix4x4 R;
		// Translation
		float Tx, Ty, Tz;
		// Perspective
		float Px, Py, Pz, Pw;
		// Represents a valid series of transformations
		bool Valid;
	};

	// InterpolatedTransform Data
	float startTime, endTime;
	Transform start, end;
	DecomposedTransform startT, endT;
	Quaternion startQ, endQ;
	// I'm using int instead of bool to match the OpenCL structure declaration
	// (note: bool are not allowed in CPU <=> GPU data transfers)
	int hasRotation, hasTranslation, hasScale;
	int hasTranslationX, hasTranslationY, hasTranslationZ;
	int hasScaleX, hasScaleY, hasScaleZ;
	// false if start and end transformations are identical
	int isActive;
};

class MotionSystem {
public:
	MotionSystem(const vector<float> &t, const vector<Transform> &transforms);
	explicit MotionSystem(const Transform &t);
	MotionSystem();

	bool IsStatic() const;

	float StartTime() const {
		return times.front();
	}

	float EndTime() const {
		return times.back();
	}

	Matrix4x4 Sample(const float time) const;

	BBox Bound(BBox ibox, const bool storingGlobal2Local) const;

	void ApplyTransform(const Transform &trans);

	luxrays::Properties ToProperties(const std::string &prefix) const;

	vector<float> times;
	vector<InterpolatedTransform> interpolatedTransforms;

private:
	void Init(const vector<float> &t, const vector<Transform> &transforms);
};

// Contains one or more <time, transform> pairs (knots) representing a path
class MotionTransform {
public:
	MotionTransform(const MotionTransform &other);
	MotionTransform(const vector<float> &times, const vector<Transform> &transforms);
	MotionTransform(const Transform &t);
	MotionTransform();

	bool Valid() const;

	size_t Size() const;

	std::pair<float, float> Interval() const;

	// Returns true if this MotionTransform represent a single, time-independent transform
	bool IsStatic() const {
		BOOST_ASSERT(Valid());
		return times.size() == 0;
	};

	Transform StaticTransform() const;
	MotionSystem GetMotionSystem() const;

	// Concatenate two MotionTransform.
	// Extract the unique knots from input MotionTransform'
	// for each unique knot interpolate the knots from the other MotionTransform and concantenate.
	// Thus if left hand has knots at (1, 3) and right hand has knots at (1, 4) then output has 
	// knots at (1, 3, 4) where right hand side has been interpolated at knot t=3 and left hand side
	// is kept constant after t=3.
	MotionTransform operator*(const MotionTransform &t) const;

	// 
	MotionTransform operator*(const Transform &t) const;

	MotionTransform GetInverse() const;

private:
	vector<float> times;
	vector<Transform> transforms;
};

}

#endif // _LUXRAYS_MOTIONSYSTEM_H

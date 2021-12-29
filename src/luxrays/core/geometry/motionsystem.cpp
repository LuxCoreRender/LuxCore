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

#include <cstring>
using std::memset;

#include <vector>
#include <algorithm>
#include <utility>
#include <boost/foreach.hpp>

#include "luxrays/core/geometry/motionsystem.h"

using namespace std;

namespace luxrays {

//------------------------------------------------------------------------------
// MotionSystem
//------------------------------------------------------------------------------

InterpolatedTransform::InterpolatedTransform(float st, float et,
	const Transform &s, const Transform &e) : hasRotation(false),
	hasTranslationX(false), hasTranslationY(false), hasTranslationZ(false),
	hasScaleX(false), hasScaleY(false), hasScaleZ(false), isActive(false) {
	startTime = st;
	endTime = et;
	start = s;
	end = e;

	if (startTime == endTime)
		return;

	startT = DecomposedTransform(start.m);
	endT = DecomposedTransform(end.m);

	if (!startT.Valid)
		throw std::runtime_error("Singular start matrix in InterpolatedTransform, interpolation disabled");
	if (!endT.Valid)
		throw std::runtime_error("Singular end matrix in InterpolatedTransform, interpolation disabled");

	startQ = Normalize(Quaternion(startT.R));
	endQ = Normalize(Quaternion(endT.R));

	hasTranslationX = startT.Tx != endT.Tx;
	hasTranslationY = startT.Ty != endT.Ty;
	hasTranslationZ = startT.Tz != endT.Tz;
	hasTranslation = hasTranslationX || hasTranslationY || hasTranslationZ;

	hasScaleX = startT.Sx != endT.Sx;
	hasScaleY = startT.Sy != endT.Sy;
	hasScaleZ = startT.Sz != endT.Sz;
	hasScale = hasScaleX || hasScaleY || hasScaleZ;

	hasRotation = fabsf(Dot(startQ, endQ) - 1.f) >= 1e-6f;

	isActive = hasTranslation || hasScale || hasRotation;
}

BBox InterpolatedTransform::Bound(BBox ibox, const bool storingGlobal2Local) const {
	// Compute total bounding box by naive unions.
	BBox tbox;
	const float N = 1024.f;
	for (float i = 0; i <= N; ++i) {
		const float t = Lerp(i / N, startTime, endTime);
		Matrix4x4 m = Sample(t);
		if (storingGlobal2Local)
			m = m.Inverse();
		tbox = Union(tbox, m * ibox);
	}
	return tbox;
}

Matrix4x4 InterpolatedTransform::Sample(const float time) const {
	if (!isActive)
		return start.m;

	// Determine interpolation value
	if (time <= startTime)
		return start.m;
	if (time >= endTime)
		return end.m;

	const float w = endTime - startTime;
	const float d = time - startTime;
	const float le = d / w;

	float interMatrix[4][4];

	// if translation only, just modify start matrix
	if (hasTranslation && !(hasScale || hasRotation)) {
		memcpy(interMatrix, start.m.m, sizeof(float) * 16);
		if (hasTranslationX)
			interMatrix[0][3] = Lerp(le, startT.Tx, endT.Tx);
		if (hasTranslationY)
			interMatrix[1][3] = Lerp(le, startT.Ty, endT.Ty);
		if (hasTranslationZ)
			interMatrix[2][3] = Lerp(le, startT.Tz, endT.Tz);
		return interMatrix;
	}

	if (hasRotation) {
		// Quaternion interpolation of rotation
		Quaternion interQ = Slerp(le, startQ, endQ);
		interQ.ToMatrix(interMatrix);
	} else
		memcpy(interMatrix, startT.R.m, sizeof(float) * 16);

	if (hasScale) {
		const float Sx = Lerp(le, startT.Sx, endT.Sx);
		const float Sy = Lerp(le, startT.Sy, endT.Sy); 
		const float Sz = Lerp(le, startT.Sz, endT.Sz);

		// T * S * R
		for (u_int j = 0; j < 3; ++j) {
			interMatrix[0][j] = Sx * interMatrix[0][j];
			interMatrix[1][j] = Sy * interMatrix[1][j];
			interMatrix[2][j] = Sz * interMatrix[2][j];
		}
	} else {
		for (u_int j = 0; j < 3; ++j) {
			interMatrix[0][j] = startT.Sx * interMatrix[0][j];
			interMatrix[1][j] = startT.Sy * interMatrix[1][j];
			interMatrix[2][j] = startT.Sz * interMatrix[2][j];
		}
	}

	if (hasTranslationX)
		interMatrix[0][3] = Lerp(le, startT.Tx, endT.Tx);
	else
		interMatrix[0][3] = startT.Tx;

	if (hasTranslationY)
		interMatrix[1][3] = Lerp(le, startT.Ty, endT.Ty);
	else
		interMatrix[1][3] = startT.Ty;

	if (hasTranslationZ)
		interMatrix[2][3] = Lerp(le, startT.Tz, endT.Tz);
	else
		interMatrix[2][3] = startT.Tz;

	return interMatrix;
}

static void V4MulByMatrix(const Matrix4x4 &A, const float x[4], float b[4]) {
	b[0] = A.m[0][0]*x[0] + A.m[0][1]*x[1] + A.m[0][2]*x[2] + A.m[0][3]*x[3];
	b[1] = A.m[1][0]*x[0] + A.m[1][1]*x[1] + A.m[1][2]*x[2] + A.m[1][3]*x[3];
	b[2] = A.m[2][0]*x[0] + A.m[2][1]*x[1] + A.m[2][2]*x[2] + A.m[2][3]*x[3];
	b[3] = A.m[3][0]*x[0] + A.m[3][1]*x[1] + A.m[3][2]*x[2] + A.m[3][3]*x[3];
}

InterpolatedTransform::DecomposedTransform::DecomposedTransform(const Matrix4x4 &m) :
		R(m), Valid(false) {
	// Normalize the matrix. 
	if (R.m[3][3] == 0)
		return;
	for (u_int i = 0; i < 4; ++i) {
		for (u_int j = 0; j < 4; ++j)
			R.m[i][j] /= R.m[3][3];
	}
	// pmat is used to solve for perspective, but it also provides
	// an easy way to test for singularity of the upper 3x3 component.	 
	Matrix4x4 pmat(R);
	for (u_int i = 0; i < 3; ++i)
		pmat.m[i][3] = 0.f;
	pmat.m[3][3] = 1.f;

	if (pmat.Determinant() == 0.f)
		return;

	// First, isolate perspective.  This is the messiest. 
	if (R.m[3][0] != 0.f || R.m[3][1] != 0.f || R.m[3][2] != 0.f) {
		// prhs is the right hand side of the equation. 
		float prhs[4];
		prhs[0] = R.m[3][0];
		prhs[1] = R.m[3][1];
		prhs[2] = R.m[3][2];
		prhs[3] = R.m[3][3];

		// Solve the equation by inverting pmat and multiplying
		// prhs by the inverse. This is the easiest way, not
		// necessarily the best.
		Matrix4x4 tinvpmat(pmat.Inverse().Transpose());
		float psol[4];
		V4MulByMatrix(tinvpmat, prhs, psol);
 
		// Stuff the answer away. 
		Px = psol[0];
		Py = psol[1];
		Pz = psol[2];
		Pw = psol[3];
		//Px = Py = Pz = Pw = 0;
		// Clear the perspective partition. 
		R.m[3][0] = R.m[3][1] = R.m[3][2] = 0.f;
		R.m[3][3] = 1.f;
	}
//	else		// No perspective. 
//		tran[U_PERSPX] = tran[U_PERSPY] = tran[U_PERSPZ] =
//			tran[U_PERSPW] = 0;

	// Next take care of translation (easy). 
	Tx = R.m[0][3];
	Ty = R.m[1][3];
	Tz = R.m[2][3];
	for (u_int i = 0; i < 3; ++i)
		R.m[i][3] = 0.f;
	
	Vector row[3];
	// Now get scale and shear. 
	for (u_int i = 0; i < 3; ++i) {
		row[i].x = R.m[i][0];
		row[i].y = R.m[i][1];
		row[i].z = R.m[i][2];
	}

	// Compute X scale factor and normalize first row. 
	Sx = row[0].Length();
	row[0] *= 1.f / Sx;

	// Compute XY shear factor and make 2nd row orthogonal to 1st. 
	Sxy = Dot(row[0], row[1]);
	row[1] -= Sxy * row[0];

	// Now, compute Y scale and normalize 2nd row. 
	Sy = row[1].Length();
	row[1] *= 1.f / Sy;
	Sxy /= Sy;

	// Compute XZ and YZ shears, orthogonalize 3rd row. 
	Sxz = Dot(row[0], row[2]);
	row[2] -= Sxz * row[0];
	Syz = Dot(row[1], row[2]);
	row[2] -= Syz * row[1];

	// Next, get Z scale and normalize 3rd row. 
	Sz = row[2].Length();
	row[2] *= 1.f / Sz;
	Sxz /= Sz;
	Syz /= Sz;

	// At this point, the matrix (in rows[]) is orthonormal.
	// Check for a coordinate system flip.  If the determinant
	// is -1, then negate the matrix and the scaling factors.	 
	if (Dot(row[0], Cross(row[1], row[2])) < 0.f) {
		Sx *= -1.f;
		Sy *= -1.f;
		Sz *= -1.f;
		for (u_int i = 0; i < 3; ++i)
			row[i] *= -1.f;
	}

	// Now, get the rotations out 
	for (u_int i = 0; i < 3; ++i) {
		R.m[i][0] = row[i].x;
		R.m[i][1] = row[i].y;
		R.m[i][2] = row[i].z;
	}
	// All done! 
	Valid = true;
}

//------------------------------------------------------------------------------
// MotionSystem
//------------------------------------------------------------------------------

MotionSystem::MotionSystem(const vector<float> &t, const vector<Transform> &transforms) {
	Init(t, transforms);
}

MotionSystem::MotionSystem(const Transform &t) : times(1, 0.f),
		interpolatedTransforms(1, InterpolatedTransform(0.f, 0.f, t, t)),
		interpolatedInverseTransforms(1, InterpolatedTransform(0.f, 0.f, Inverse(t), Inverse(t))) {
}

MotionSystem::MotionSystem() : times(1, 0.f),
		interpolatedTransforms(1, InterpolatedTransform(0.f, 0.f, Transform(), Transform())),
		interpolatedInverseTransforms(1, InterpolatedTransform(0.f, 0.f, Transform(), Transform())) {
}

void MotionSystem::Init(const vector<float> &t, const vector<Transform> &transforms) {
	times.clear();
	interpolatedTransforms.clear();
	interpolatedInverseTransforms.clear();

	times = t;

	typedef vector<float>::const_iterator time_cit;
	typedef vector<Transform>::const_iterator trans_cit;

	// allocate one non-interpolating InterpolatedTransform at start and end of the array
	// this saves us extra bounds checking
	time_cit time = times.begin();
	time_cit prev_time = times.begin();
	trans_cit prev_trans = transforms.begin();
	trans_cit trans = transforms.begin();

	interpolatedTransforms.reserve(times.size() + 1);
	interpolatedInverseTransforms.reserve(times.size() + 1);

	while (time != times.end()) {
		interpolatedTransforms.push_back(InterpolatedTransform(*prev_time, *time,
				*prev_trans, *trans));
		interpolatedInverseTransforms.push_back(InterpolatedTransform(*prev_time, *time,
				Inverse(*prev_trans), Inverse(*trans)));
		prev_time = time;
		++time;
		prev_trans = trans;
		++trans;
	}

	interpolatedTransforms.push_back(InterpolatedTransform(*prev_time, *prev_time,
			*prev_trans, *prev_trans));
	interpolatedInverseTransforms.push_back(InterpolatedTransform(*prev_time, *prev_time,
			Inverse(*prev_trans), Inverse(*prev_trans)));
}

bool MotionSystem::IsStatic() const {
	if (times.size() <= 1)
		return true;

	return false;
}

Matrix4x4 MotionSystem::Sample(float time) const {
	size_t index = std::upper_bound(times.begin(), times.end(), time) - times.begin();

	index = Min(index, times.size() - 1);

	return interpolatedTransforms[index].Sample(time);
}

Matrix4x4 MotionSystem::SampleInverse(float time) const {
	size_t index = std::upper_bound(times.begin(), times.end(), time) - times.begin();

	index = Min(index, times.size() - 1);

	return interpolatedInverseTransforms[index].Sample(time);
}

BBox MotionSystem::Bound(BBox ibox, const bool storingGlobal2Local) const {;
	typedef vector<InterpolatedTransform>::const_iterator msys_cit;

	BBox result;

	for(msys_cit ms = interpolatedTransforms.begin(); ms != interpolatedTransforms.end(); ++ms)
		result = Union(result, ms->Bound(ibox, storingGlobal2Local));

	return result;
}

void MotionSystem::ApplyTransform(const Transform &trans) {
	const vector<float> t = times;
	vector<Transform> transforms;

	// First and last interpolatedTransforms have the same transformation start and end
	for (u_int i = 1 ; i < interpolatedTransforms.size() - 1; ++i)
		transforms.push_back(interpolatedTransforms[i].start * trans);
	transforms.push_back(interpolatedTransforms[interpolatedTransforms.size() - 2].end * trans);

	Init(t, transforms);
}

Properties MotionSystem::ToProperties(const std::string &prefix, const bool storingGlobal2Local) const {
	Properties props;

	// First and last interpolatedTransforms have the same transformation start and end
	for (u_int i = 1; i < interpolatedTransforms.size() - 1; ++i) {
		const InterpolatedTransform &it = interpolatedTransforms[i];

		props.Set(Property(prefix+".motion." + ToString(i - 1) + ".time")(it.startTime));
		props.Set(Property(prefix+".motion." + ToString(i - 1) + ".transformation")(
				storingGlobal2Local ? it.start.m.Inverse() : it.start.m));
	}

	const u_int lastIndex = interpolatedTransforms.size() - 2;
	const InterpolatedTransform &it = interpolatedTransforms[lastIndex];
	props.Set(Property(prefix+".motion." + ToString(lastIndex) + ".time")(it.endTime));
	props.Set(Property(prefix+".motion." + ToString(lastIndex) + ".transformation")(
			storingGlobal2Local ? it.end.m.Inverse() : it.end.m));
		
	return props;
}

//------------------------------------------------------------------------------
// MotionTransform
//------------------------------------------------------------------------------

// Contains one or more <time, transform> pairs (knots) representing a path
MotionTransform::MotionTransform(const MotionTransform &other) : times(other.times), transforms(other.transforms) { }

MotionTransform::MotionTransform(const vector<float> &t, const vector<Transform> &trans) : times(t), transforms(trans) {
}

MotionTransform::MotionTransform(const Transform &t) : times(), transforms(1, t) {
}

MotionTransform::MotionTransform() : times(), transforms(1, Transform()) {
}

bool MotionTransform::Valid() const {
	// static
	if (times.size() == 0 && transforms.size() == 1)
		return true;

	// now perform dynamic tests
	if (times.size() > 0 && times.size() != transforms.size())
		return false;

	// first check monotonically increasing
	if (*std::max_element(times.begin(), times.end()) != times.back())
		return false;

	// then check strictly increasing
	if (std::adjacent_find(times.begin(), times.end()) != times.end())
		return false;

	return true;
}

size_t MotionTransform::Size() const {
	return times.size();
}

std::pair<float, float> MotionTransform::Interval() const {
	return std::make_pair(times.front(), times.back());
}

Transform MotionTransform::StaticTransform() const {
	if (!IsStatic())
		return Transform(MotionSystem(times, transforms).Sample(0.f));

	return transforms.front();
}

MotionSystem MotionTransform::GetMotionSystem() const {
	if (IsStatic())
		return MotionSystem(transforms.front());
	else
		return MotionSystem(times, transforms);
}

// Concatenates two MotionTransforms.
// Extract the unique knots from input MotionTransform
// for each unique knot interpolate the knots from the other MotionTransform and concatenate.
// Thus if left hand has knots at (1, 3) and right hand has knots at (1, 4) then output has 
// knots at (1, 3, 4) where right hand side has been interpolated at knot t=3 and left hand side
// is kept constant after t=3.
MotionTransform MotionTransform::operator*(const MotionTransform &t) const {
	BOOST_ASSERT(Valid());
	BOOST_ASSERT(t.Valid());

	if (IsStatic() && t.IsStatic())
		return MotionTransform(transforms.front() * t.transforms.front());

	vector<float> new_times(times.size() + t.times.size());
	vector<Transform> new_transforms;

	// remove duplicates
	const size_t new_size = std::set_union(times.begin(), times.end(), t.times.begin(), t.times.end(), new_times.begin()) - new_times.begin();
	new_times.resize(new_size);

	typedef vector<float>::const_iterator time_cit;
	//typedef vector<Transform>::iterator trans_it;
	typedef vector<Transform>::const_iterator trans_cit;

	// used so we have valid time iterators in case either is static
	vector<float> time_zero(1, 0.f);

	// left side's times and transforms
	time_cit timeL = IsStatic() ? time_zero.begin() : times.begin();
	time_cit timeEndL = IsStatic() ? time_zero.end() : times.end();
	trans_cit transL = transforms.begin();
	time_cit prev_timeL = timeL;
	trans_cit prev_transL = transL;

	// right side's times and transforms
	time_cit timeR = IsStatic() ? time_zero.begin() : t.times.begin();
	time_cit timeEndR = IsStatic() ? time_zero.end() : t.times.end();
	trans_cit transR = t.transforms.begin();
	time_cit prev_timeR = timeR;
	trans_cit prev_transR = transR;

	// motion systems interpolating left side knots
	InterpolatedTransform itL(*prev_timeL, *timeL, *prev_transL, *transL);
	// motion systems interpolating right side knots
	InterpolatedTransform itR(*prev_timeR, *timeR, *prev_transR, *transR);

	// loop over unique knots
	for (time_cit timeN = new_times.begin(); timeN != new_times.end(); timeN++) {
		if (*timeL <= *timeN) {
			// need to move to next knot
			prev_timeL = timeL;
			timeL = min(timeL+1, timeEndL-1);
			prev_transL = transL;
			transL = min(transL+1, transforms.end()-1);
			// update for interpolation
			itL = InterpolatedTransform(*prev_timeL, *timeL, *prev_transL, *transL);
		}
		if (*timeR <= *timeN) {
			// need to move to next knot
			prev_timeR = timeR;
			timeR = min(timeR+1, timeEndR-1);
			prev_transR = transR;
			transR = min(transR+1, t.transforms.end()-1);
			// update for interpolation
			itR = InterpolatedTransform(*prev_timeR, *timeR, *prev_transR, *transR);
		}

		// get (possibly interpolated) left and right hand values at new knot
		const Transform tL = Transform(itL.Sample(*timeN));
		const Transform tR = Transform(itR.Sample(*timeN));

		// append the concatenated result
		new_transforms.push_back(tL * tR);
	}

	return MotionTransform(new_times, new_transforms);
}

MotionTransform MotionTransform::operator*(const Transform &t) const {	
	MotionTransform cmt(*this);

	typedef vector<Transform>::iterator trans_it;

	for (trans_it trans = cmt.transforms.begin(); trans != cmt.transforms.end(); trans++) {
		*trans = *trans * t;
	}

	return cmt;
}

MotionTransform MotionTransform::GetInverse() const {
	MotionTransform imt(*this);

	typedef vector<Transform>::iterator trans_it;

	for (trans_it trans = imt.transforms.begin(); trans != imt.transforms.end(); trans++) {
		*trans = Inverse(*trans);
	}

	return imt;
}

}

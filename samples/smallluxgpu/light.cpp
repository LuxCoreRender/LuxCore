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

#include "light.h"

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight(TextureMap *tx) {
	tex = tx;
	shiftU = 0.f;
	shiftV = 0.f;
}

Spectrum InfiniteLight::Le(const Vector &dir) const {
	const float theta = SphericalTheta(dir);
	const UV uv(SphericalPhi(dir) * INV_TWOPI + shiftU, theta * INV_PI + shiftV);

	return gain * tex->GetColor(uv);
}

Spectrum InfiniteLight::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	Vector wi = CosineSampleHemisphere(u0, u1);
	*pdf = wi.z * INV_PI;

	Vector v1, v2;
	CoordinateSystem(Vector(N), &v1, &v2);

	wi = Vector(
			v1.x * wi.x + v2.x * wi.y + N.x * wi.z,
			v1.y * wi.x + v2.y * wi.y + N.y * wi.z,
			v1.z * wi.x + v2.z * wi.y + N.z * wi.z);
	*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);

	return Le(wi);
}

//------------------------------------------------------------------------------
// InfiniteLight with portals
//------------------------------------------------------------------------------

InfiniteLightPortal::InfiniteLightPortal(Context *ctx, TextureMap *tx, const string &portalFileName) :
	InfiniteLight(tx) {
	// Read portals
	cerr << "Portal PLY objects file name: " << portalFileName << endl;
	portals = ExtTriangleMesh::LoadExtTriangleMesh(ctx, portalFileName);
	const Triangle *tris = portals->GetTriangles();
	for (unsigned int i = 0; i < portals->GetTotalTriangleCount(); ++i)
		portalAreas.push_back(tris[i].Area(portals->GetVertices()));
}

InfiniteLightPortal::~InfiniteLightPortal() {
	portals->Delete();
	delete portals;
}

Spectrum InfiniteLightPortal::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	// Select one of the portals
	const unsigned int portalCount = portals->GetTotalTriangleCount();
	unsigned int portalIndex = Min<unsigned int>(Floor2UInt(portalCount * u2), portalCount - 1);

	// Looks for a valid portal
	const Triangle *tris = portals->GetTriangles();
	const Normal *normals = portals->GetNormal();
	for (unsigned int i = 0; i < portalCount; ++i) {
		// Sample the triangle
		const Triangle &tri = tris[portalIndex];
		Point samplePoint;
		float b0, b1, b2;
		tri.Sample(portals->GetVertices(), u0, u1, &samplePoint, &b0, &b1, &b2);
		const Normal &sampleN = normals[tri.v[0]];

		// Check if the portal is visible
		Vector wi = samplePoint - p;
		const float distanceSquared = wi.LengthSquared();
		wi /= sqrtf(distanceSquared);

		const float sampleNdotMinusWi = Dot(sampleN, -wi);
		const float NdotWi = Dot(N, wi);
		if ((sampleNdotMinusWi > 0.f) && (NdotWi > 0.f)) {
			*shadowRay = Ray(p, wi, RAY_EPSILON, INFINITY);
			*pdf = distanceSquared / (sampleNdotMinusWi * portalAreas[portalIndex] * portalCount);
			return Le(wi);
		}

		if (++portalIndex >= portalCount)
			portalIndex = 0;
	}

	*pdf = 0.f;
	return Spectrum();
}

//------------------------------------------------------------------------------
// InfiniteLight with importance sampling
//------------------------------------------------------------------------------

static void ComputeStep1dCDF(const float *f, u_int nSteps, float *c, float *cdf) {
	// Compute integral of step function at $x_i$
	cdf[0] = 0.f;
	for (u_int i = 1; i < nSteps + 1; ++i)
		cdf[i] = cdf[i - 1] + f[i - 1] / nSteps;
	// Transform step function integral into cdf
	*c = cdf[nSteps];
	for (u_int i = 1; i < nSteps + 1; ++i)
		cdf[i] /= *c;
}

// A utility class for sampling from a regularly sampled 1D distribution.
class Distribution1D {
public:

	/**
	 * Creates a 1D distribution for the given function.
	 * It is assumed that the given function is sampled regularly sampled in
	 * the interval [0,1] (ex. 0.1, 0.3, 0.5, 0.7, 0.9 for 5 samples).
	 *
	 * @param f The values of the function.
	 * @param n The number of samples.
	 */
	Distribution1D(const float *f, u_int n) {
		func = new float[n];
		cdf = new float[n + 1];
		count = n;
		memcpy(func, f, n * sizeof (float));
		// funcInt is the sum of all f elements divided by the number
		// of elements, ie the average value of f over [0;1)
		ComputeStep1dCDF(func, n, &funcInt, cdf);
		invFuncInt = 1.f / funcInt;
		invCount = 1.f / count;
	}

	~Distribution1D() {
		delete[] func;
		delete[] cdf;
	}

	/**
	 * Samples from this distribution.
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample
	 *            should be stored.
	 * @param off Optional parameter to get the offset of the value
	 *
	 * @return The x value of the sample (i.e. the x in f(x)).
	 */
	float SampleContinuous(float u, float *pdf, u_int *off = NULL) const {
		// Find surrounding CDF segments and _offset_
		float *ptr = std::lower_bound(cdf, cdf + count + 1, u);
		u_int offset = max<int>(0, ptr - cdf - 1);

		// Compute offset along CDF segment
		const float du = (u - cdf[offset]) /
				(cdf[offset + 1] - cdf[offset]);

		// Compute PDF for sampled offset
		*pdf = func[offset] * invFuncInt;

		// Save offset
		if (off)
			*off = offset;

		// Return $x \in [0,1)$ corresponding to sample
		return (offset + du) * invCount;
	}

	/**
	 * Samples from this distribution.
	 *
	 * @param u   The random value used to sample.
	 * @param pdf The pointer to the float where the pdf of the sample
	 *            should be stored.
	 * @param du  Optional parameter to get the remaining offset
	 *
	 * @return The index of the sampled interval.
	 */
	u_int SampleDiscrete(float u, float *pdf, float *du = NULL) const {
		// Find surrounding CDF segments and _offset_
		float *ptr = std::lower_bound(cdf, cdf + count + 1, u);
		u_int offset = max<int>(0, ptr - cdf - 1);

		// Compute offset along CDF segment
		if (du)
			*du = (u - cdf[offset]) /
			(cdf[offset + 1] - cdf[offset]);

		// Compute PDF for sampled offset
		*pdf = func[offset] * invFuncInt * invCount;
		return offset;
	}

	float Pdf(float u) const {
		return func[Offset(u)] * invFuncInt;
	}

	float Average() const {
		return funcInt;
	}

	u_int Offset(float u) const {
		return min(count - 1, Floor2UInt(u * count));
	}

private:
	// Distribution1D Private Data
	/*
	 * The function and its cdf.
	 */
	float *func, *cdf;
	/**
	 * The function integral (assuming it is regularly sampled with an interval of 1),
	 * the inverted function integral and the inverted count.
	 */
	float funcInt, invFuncInt, invCount;
	/*
	 * The number of function values. The number of cdf values is count+1.
	 */
	u_int count;
};

class Distribution2D {
public:
	Distribution2D(const float *data, u_int nu, u_int nv) {
		pConditionalV.reserve(nv);
		// Compute conditional sampling distribution for $\tilde{v}$
		for (u_int v = 0; v < nv; ++v)
			pConditionalV.push_back(new Distribution1D(data + v * nu, nu));
		// Compute marginal sampling distribution $p[\tilde{v}]$
		vector<float> marginalFunc;
		marginalFunc.reserve(nv);
		for (u_int v = 0; v < nv; ++v)
			marginalFunc.push_back(pConditionalV[v]->Average());
		pMarginal = new Distribution1D(&marginalFunc[0], nv);
	}

	~Distribution2D() {
		delete pMarginal;
		for (u_int i = 0; i < pConditionalV.size(); ++i)
			delete pConditionalV[i];
	}

	void SampleContinuous(float u0, float u1, float uv[2],
			float *pdf) const {
		float pdfs[2];
		u_int v;
		uv[1] = pMarginal->SampleContinuous(u1, &pdfs[1], &v);
		uv[0] = pConditionalV[v]->SampleContinuous(u0, &pdfs[0]);
		*pdf = pdfs[0] * pdfs[1];
	}

	void SampleDiscrete(float u0, float u1, u_int uv[2], float *pdf) const {
		float pdfs[2];
		uv[1] = pMarginal->SampleDiscrete(u1, &pdf[1]);
		uv[0] = pConditionalV[uv[1]]->SampleDiscrete(u0, &pdf[0]);
		*pdf = pdfs[0] * pdfs[1];
	}

	float Pdf(float u, float v) const {
		return pConditionalV[pMarginal->Offset(v)]->Pdf(u) *
				pMarginal->Pdf(v);
	}

	float Average() const {
		return pMarginal->Average();
	}
private:
	// Distribution2D Private Data
	vector<Distribution1D *> pConditionalV;
	Distribution1D *pMarginal;
};

/*InfiniteLight::InfiniteLight(TextureMap *tx) {
	tex = tx;
	portals = NULL;
	shiftU = 0.f;
	shiftV = 0.f;

	// Build the importance map
	const float nu = tex->GetWidth() / 2;
	const float nv = tex->GetHeight() / 2;

	float *img = new float[nu * nv];
	UV uv;
	for (unsigned int x = 0; x < nu; ++x) {
		uv.u = (x + .5f) / nu;

		for (u_int y = 0; y < nv; ++y) {
			uv.v = (y + .5f) / nv;

			img[y + x * nv] = tex->GetColor(uv).Filter();
		}
	}

	uvDistrib = new Distribution2D(img, nu, nv);
	delete[] img;
}*/

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const vector<ExtTriangleMesh *> &objs) {
	lightMaterial = mat;
	meshIndex = mshIndex;
	triIndex = triangleIndex;

	const ExtTriangleMesh *mesh = objs[meshIndex];
	area = (mesh->GetTriangles()[triIndex]).Area(mesh->GetVertices());
}

Spectrum TriangleLight::Le(const vector<ExtTriangleMesh *> &objs, const Vector &wo) const {
	const ExtTriangleMesh *mesh = objs[meshIndex];
	const Triangle &tri = mesh->GetTriangles()[triIndex];
	Normal sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

	if (Dot(sampleN, wo) <= 0.f)
		return Spectrum();

	const Spectrum *colors = mesh->GetColors();
	if (colors)
		return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain(); // Light sources are supposed to have flat color
}

Spectrum TriangleLight::Sample_L(const vector<ExtTriangleMesh *> &objs, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
	const ExtTriangleMesh *mesh = objs[meshIndex];
	const Triangle &tri = mesh->GetTriangles()[triIndex];

	Point samplePoint;
	float b0, b1, b2;
	tri.Sample(mesh->GetVertices(), u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = mesh->GetNormal()[tri.v[0]]; // Light sources are supposed to be flat

	Vector wi = samplePoint - p;
	const float distanceSquared = wi.LengthSquared();
	const float distance = sqrtf(distanceSquared);
	wi /= distance;

	const float sampleNdotMinusWi = Dot(sampleN, -wi);
	const float NdotWi = Dot(N, wi);
	if ((sampleNdotMinusWi <= 0.f) || (NdotWi <= 0.f)) {
		*pdf = 0.f;
		return Spectrum();
	}

	*shadowRay = Ray(p, wi, RAY_EPSILON, distance - RAY_EPSILON);
	*pdf = distanceSquared / (sampleNdotMinusWi * area);

	const Spectrum *colors = mesh->GetColors();
	if (colors)
		return mesh->GetColors()[tri.v[0]] * lightMaterial->GetGain(); // Light sources are supposed to have flat color
	else
		return lightMaterial->GetGain(); // Light sources are supposed to have flat color
}
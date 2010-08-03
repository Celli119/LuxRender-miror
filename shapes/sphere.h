/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

// sphere.cpp*
#include "shape.h"
#include "mc.h"

namespace lux
{

// Sphere Declarations
class Sphere: public Shape {
public:
	// Sphere Public Methods
	Sphere(const Transform &o2w, bool ro, float rad,
	       float zmin, float zmax, float phiMax);
	virtual ~Sphere() { }
	virtual BBox ObjectBound() const;
	virtual bool Intersect(const Ray &ray, float *tHit,
	               DifferentialGeometry *dg) const;
	virtual bool IntersectP(const Ray &ray) const;
	virtual float Area() const;
	virtual Point Sample(float u1, float u2, float u3, Normal *ns) const {
		Point p = Point(0,0,0) + radius *
			UniformSampleSphere(u1, u2);
		*ns = Normalize(ObjectToWorld(Normal(p.x, p.y, p.z)));
		if (reverseOrientation) *ns *= -1.f;
		return ObjectToWorld(p);
	}
	virtual Point Sample(const TsPack *tspack, const Point &p,
			float u1, float u2, float u3, Normal *ns) const {
		// Compute coordinate system for sphere sampling
		Point Pcenter = ObjectToWorld(Point(0,0,0));
		Vector wc = Normalize(Pcenter - p);
		Vector wcX, wcY;
		CoordinateSystem(wc, &wcX, &wcY);
		// Sample uniformly on sphere if \pt is inside it
		if (DistanceSquared(p, Pcenter) - radius*radius < 1e-4f)
			return Sample(u1, u2, u3, ns);
		// Sample sphere uniformly inside subtended cone
		float cosThetaMax = sqrtf(max(0.f, 1.f - radius*radius /
			DistanceSquared(p, Pcenter)));
		DifferentialGeometry dgSphere;
		float thit;
		Point ps;
		Ray r(p,
		      UniformSampleCone(u1, u2, cosThetaMax, wcX, wcY, wc));
		if (!Intersect(r, &thit, &dgSphere)) {
			ps = Pcenter - radius * wc;
		} else {
			ps = r(thit);
		}
		*ns = Normal(Normalize(ps - Pcenter));
		if (reverseOrientation) *ns *= -1.f;
		return ps;
	}
	virtual float Pdf(const Point &p, const Point &po) const {
		Point Pcenter = ObjectToWorld(Point(0,0,0));
		// Return uniform weight if point inside sphere
		if (DistanceSquared(p, Pcenter) - radius*radius < 1e-4f)
			return 1.f / Area();
		// Compute general sphere weight
		const float cosThetaMax = sqrtf(max(0.f, 1.f - radius*radius /
			DistanceSquared(p, Pcenter)));
		const Vector w(p - po);
		const float d2 = w.LengthSquared();
		return UniformConePdf(cosThetaMax) * AbsDot(w, po - Pcenter) /
			(d2 * sqrtf(d2) * radius);
	}
	
	static Shape* CreateShape(const Transform &o2w, bool reverseOrientation, const ParamSet &params);
private:
	// Sphere Private Data
	float radius;
	float phiMax;
	float zmin, zmax;
	float thetaMin, thetaMax;
};

}//namespace lux

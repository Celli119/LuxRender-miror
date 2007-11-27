/***************************************************************************
 *   Copyright (C) 1998-2007 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of Lux Renderer.                                    *
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
 *   Lux Renderer website : http://www.luxrender.org                       *
 ***************************************************************************/

#ifndef LUX_TEXTURE_H
#define LUX_TEXTURE_H
// texture.h*
#include "lux.h"
#include "color.h"
#include "geometry.h"
#include "transform.h"

// Texture Declarations
class  TextureMapping2D {
public:
	// TextureMapping2D Interface
	virtual ~TextureMapping2D() { }
	virtual void Map(const DifferentialGeometry &dg,
		float *s, float *t, float *dsdx, float *dtdx,
		float *dsdy, float *dtdy) const = 0;
};
class  UVMapping2D : public TextureMapping2D {
public:
	// UVMapping2D Public Methods
	UVMapping2D(float su = 1, float sv = 1,
		float du = 0, float dv = 0);
	void Map(const DifferentialGeometry &dg, float *s, float *t,
		float *dsdx, float *dtdx,
		float *dsdy, float *dtdy) const;
private:
	float su, sv, du, dv;
};
class  SphericalMapping2D : public TextureMapping2D {
public:
	// SphericalMapping2D Public Methods
	SphericalMapping2D(const Transform &toSph)
		: WorldToTexture(toSph) {
	}
	void Map(const DifferentialGeometry &dg, float *s, float *t,
		float *dsdx, float *dtdx,
		float *dsdy, float *dtdy) const;
private:
	void sphere(const Point &P, float *s, float *t) const;
	Transform WorldToTexture;
};
class
 CylindricalMapping2D : public TextureMapping2D {
public:
	// CylindricalMapping2D Public Methods
	CylindricalMapping2D(const Transform &toCyl)
		: WorldToTexture(toCyl) {
	}
	void Map(const DifferentialGeometry &dg, float *s, float *t,
		float *dsdx, float *dtdx,
		float *dsdy, float *dtdy) const;
private:
	void cylinder(const Point &P, float *s, float *t) const;
	Transform WorldToTexture;
};
class  PlanarMapping2D : public TextureMapping2D {
public:
	// PlanarMapping2D Public Methods
	PlanarMapping2D(const Vector &v1, const Vector &v2,
		float du = 0, float dv = 0);
	void Map(const DifferentialGeometry &dg, float *s, float *t,
		float *dsdx, float *dtdx,
		float *dsdy, float *dtdy) const;
private:
	Vector vs, vt;
	float ds, dt;
};
class  TextureMapping3D {
public:
	// TextureMapping3D Interface
	virtual ~TextureMapping3D() { }
	virtual Point Map(const DifferentialGeometry &dg,
		Vector *dpdx, Vector *dpdy) const = 0;
};
class  IdentityMapping3D : public TextureMapping3D {
public:
	IdentityMapping3D(const Transform &x)
		: WorldToTexture(x) { }
	Point Map(const DifferentialGeometry &dg, Vector *dpdx,
		Vector *dpdy) const;
private:
	Transform WorldToTexture;
};
template <class T> class Texture {
public:
	//typedef boost::shared_ptr<Texture> TexturePtr; <<! Not working with GCC
	// Texture Interface
	virtual T Evaluate(const DifferentialGeometry &) const = 0;
	virtual ~Texture() { }
};
// ConstantTexture Declarations
template <class T>
class ConstantTexture : public Texture<T> {
public:
	// ConstantTexture Public Methods
	ConstantTexture(const T &v) { value = v; }
	T Evaluate(const DifferentialGeometry &) const {
		return value;
	}
private:
	T value;
};



#endif // LUX_TEXTURE_H

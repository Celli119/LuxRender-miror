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

#ifndef LUX_MOTIONSYSTEM_H
#define LUX_MOTIONSYSTEM_H

// motionsystem.h*
#include "lux.h"
#include "geometry/quaternion.h"
#include "geometry/bbox.h"
#include "geometry/matrix4x4.h"
#include "geometry/transform.h"
// MotionSystem Declarations

namespace lux
{

class Transforms {
public:
	Transforms() : Sx(0), Sy(0), Sz(0),
		Sxy(0), Sxz(0), Syz(0), 
		R(new Matrix4x4()),
		Tx(0), Ty(0), Tz(0),
		Px(0), Py(0), Pz(0), Pw(0) {
	}

	// Scaling
	float Sx, Sy, Sz;
	// Shearing
	float Sxy, Sxz, Syz;
	// Rotation
	boost::shared_ptr<Matrix4x4> R;
	// Translation
	float Tx, Ty, Tz;
	// Perspective
	float Px, Py, Pz, Pw;
};

class  MotionSystem {
public:
	MotionSystem() { startTime = endTime = 0; start = end = Transform(); }

	MotionSystem(float st, float et,
		const Transform &s, const Transform &e);

	~MotionSystem() {};

	Transform Sample(float time) const;

	BBox Bound(BBox ibox) const {
      		// Compute total bounding box by naive unions.
		// NOTE - radiance - this needs some work.
		BBox tbox;
		const float s = 1.f / 1024.f;
		for(float time = 0.f; time < 1.f; time += s) {
			Transform t = Sample(time);
			tbox = Union(tbox, t(ibox));
		}
		return tbox;
	}

protected:
	// decomposes the matrix m into a series of transformations
	// [Sx][Sy][Sz][Shearx/y][Sx/z][Sz/y][Rx][Ry][Rz][Tx][Ty][Tz][P(x,y,z,w)]
	// based on unmatrix() by Spencer W. Thomas from Graphic Gems II
	// TODO - lordcrc - implement extraction of perspective transform
	bool DecomposeMatrix(const boost::shared_ptr<Matrix4x4> &m, Transforms &trans) const;

	// MotionSystem Protected Data
	float startTime, endTime;
	Transform start, end;
	Transforms startT, endT;
	boost::shared_ptr<Matrix4x4> startMat, endMat;
	Quaternion startQ, endQ;
	bool hasRotation, hasTranslation, hasScale;
	bool hasTranslationX, hasTranslationY, hasTranslationZ;
	bool hasScaleX, hasScaleY, hasScaleZ;
public:
	// false if start and end transformations are identical
	bool isActive; // At the end to get better data alignment
};

}//namespace lux

#endif // LUX_MOTIONSYSTEM_H
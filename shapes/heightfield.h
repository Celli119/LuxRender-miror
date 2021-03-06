/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

// heightfield.cpp*
#include "shape.h"
#include "paramset.h"
#include "dynload.h"

namespace lux
{

// Heightfield Declarations
class Heightfield : public Shape {
public:
	// Heightfield Public Methods
	Heightfield(const Transform &o2w, bool ro, const string &name, 
		u_int nu, u_int nv, const float *zs);
	virtual ~Heightfield();
	virtual bool CanIntersect() const;
	virtual void Refine(vector<boost::shared_ptr<Shape> > &refined) const;
	virtual BBox ObjectBound() const;
	
	static Shape* CreateShape(const Transform &o2w, bool reverseOrientation, const ParamSet &params);
private:
	// Heightfield Data
	float *z;
	u_int nx, ny;
};

}//namespace lux


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

// maxwhite.cpp*
#include "maxwhite.h"
#include "dynload.h"

using namespace lux;

// MaxWhiteOp Method Definitions
void MaxWhiteOp::Map(vector<XYZColor> &xyz, u_int xRes, u_int yRes, float maxDisplayY) const 
{
	const u_int numPixels = xRes * yRes;
	// Compute maximum luminance of all pixels
	float maxY = 0.f;
	for (u_int i = 0; i < numPixels; ++i) {
		maxY = max(maxY, xyz[i].Y());
	}
	const float s = 1.f / maxY;
	for (u_int i = 0; i < numPixels; ++i)
		xyz[i] *= s;
}
ToneMap * MaxWhiteOp::CreateToneMap(const ParamSet &ps) {
	return new MaxWhiteOp;
}

static DynamicLoader::RegisterToneMap<MaxWhiteOp> r("maxwhite");

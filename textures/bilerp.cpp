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

// bilerp.cpp*
#include "bilerp.h"
#include "dynload.h"

using namespace lux;

// BilerpTexture Method Definitions
Texture<float>* BilerpFloatTexture::CreateFloatTexture(const Transform &tex2world, const TextureParams &tp)
{
	// Initialize 2D texture mapping _map_ from _tp_
	TextureMapping2D *map = NULL;
	string type = tp.FindString("mapping");
	if (type == "" || type == "uv") {
		float su = tp.FindFloat("uscale", 1.);
		float sv = tp.FindFloat("vscale", 1.);
		float du = tp.FindFloat("udelta", 0.);
		float dv = tp.FindFloat("vdelta", 0.);
		map = new UVMapping2D(su, sv, du, dv);
	}
	else if (type == "spherical") map = new SphericalMapping2D(tex2world.GetInverse());
	else if (type == "cylindrical") map = new CylindricalMapping2D(tex2world.GetInverse());
	else if (type == "planar")
		map = new PlanarMapping2D(tp.FindVector("v1", Vector(1,0,0)),
			tp.FindVector("v2", Vector(0,1,0)),
			tp.FindFloat("udelta", 0.f), tp.FindFloat("vdelta", 0.f));
	else {
		std::stringstream ss;
		ss<<"2D texture mapping '"<<type<<"' unknown";
		luxError(LUX_UNIMPLEMENT,LUX_ERROR,ss.str().c_str());
		//luxError(LUX_UNIMPLEMENT,LUX_ERROR,"2D texture mapping \"%s\" unknown", type.c_str());
		map = new UVMapping2D;
	}
	return new BilerpFloatTexture(map,
		tp.FindFloat("v00", 0.f), tp.FindFloat("v01", 1.f),
		tp.FindFloat("v10", 0.f), tp.FindFloat("v11", 1.f));
}

Texture<SWCSpectrum>* BilerpSpectrumTexture::CreateSWCSpectrumTexture(const Transform &tex2world,
		const TextureParams &tp) {
	// Initialize 2D texture mapping _map_ from _tp_
	TextureMapping2D *map = NULL;
	string type = tp.FindString("mapping");
	if (type == "" || type == "uv") {
		float su = tp.FindFloat("uscale", 1.);
		float sv = tp.FindFloat("vscale", 1.);
		float du = tp.FindFloat("udelta", 0.);
		float dv = tp.FindFloat("vdelta", 0.);
		map = new UVMapping2D(su, sv, du, dv);
	}
	else if (type == "spherical") map = new SphericalMapping2D(tex2world.GetInverse());
	else if (type == "cylindrical") map = new CylindricalMapping2D(tex2world.GetInverse());
	else if (type == "planar")
		map = new PlanarMapping2D(tp.FindVector("v1", Vector(1,0,0)),
			tp.FindVector("v2", Vector(0,1,0)),
			tp.FindFloat("udelta", 0.f), tp.FindFloat("vdelta", 0.f));
	else {
		//Error("2D texture mapping \"%s\" unknown", type.c_str());
		std::stringstream ss;
		ss<<"2D texture mapping '"<<type<<"' unknown";
		luxError(LUX_UNIMPLEMENT,LUX_ERROR,ss.str().c_str());
		map = new UVMapping2D;
	}
	return new BilerpSpectrumTexture(map,
		tp.FindRGBColor("v00", 0.f), tp.FindRGBColor("v01", 1.f),
		tp.FindRGBColor("v10", 0.f), tp.FindRGBColor("v11", 1.f));
}

static DynamicLoader::RegisterFloatTexture<BilerpFloatTexture> r1("bilerp");
static DynamicLoader::RegisterSWCSpectrumTexture<BilerpSpectrumTexture> r2("bilerp");

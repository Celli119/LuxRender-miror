/***************************************************************************
 *   Copyright (C) 1998-2008 by authors (see AUTHORS.txt )                 *
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

// mirror.cpp*
#include "mirror.h"
#include "bxdf.h"
#include "specularreflection.h"
#include "fresnelnoop.h"
#include "paramset.h"
#include "dynload.h"

using namespace lux;

// Mirror Method Definitions
BSDF *Mirror::GetBSDF(const TsPack *tspack, const DifferentialGeometry &dgGeom, const DifferentialGeometry &dgShading) const {
	// Allocate _BSDF_, possibly doing bump-mapping with _bumpMap_
	DifferentialGeometry dgs;
	if (bumpMap)
		Bump(bumpMap, dgGeom, dgShading, &dgs);
	else
		dgs = dgShading;

	float flm = film->Evaluate(tspack, dgs);
	float flmindex = filmindex->Evaluate(tspack, dgs);

	// NOTE - lordcrc - changed clamping to 0..1 to avoid >1 reflection
	SWCSpectrum R = Kr->Evaluate(tspack, dgs).Clamp(0.f, 1.f);
	BxDF *bxdf = BSDF_ALLOC(tspack, SpecularReflection)(R,
		BSDF_ALLOC(tspack, FresnelNoOp)(), flm, flmindex);
	SingleBSDF *bsdf = BSDF_ALLOC(tspack, SingleBSDF)(dgs, dgGeom.nn, bxdf);

	// Add ptr to CompositingParams structure
	bsdf->SetCompositingParams(compParams);

	return bsdf;
}
Material* Mirror::CreateMaterial(const Transform &xform,
		const TextureParams &mp) {
	boost::shared_ptr<Texture<SWCSpectrum> > Kr = mp.GetSWCSpectrumTexture("Kr", RGBColor(1.f));
	boost::shared_ptr<Texture<float> > film = mp.GetFloatTexture("film", 0.f);				// Thin film thickness in nanometers
	boost::shared_ptr<Texture<float> > filmindex = mp.GetFloatTexture("filmindex", 1.5f);				// Thin film index of refraction
	boost::shared_ptr<Texture<float> > bumpMap = mp.GetFloatTexture("bumpmap");

	// Get Compositing Params
	CompositingParams cP;
	FindCompositingParams(mp, &cP);

	return new Mirror(Kr, film, filmindex, bumpMap, cP);
}

static DynamicLoader::RegisterMaterial<Mirror> r("mirror");

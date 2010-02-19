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

// wrinkled.cpp*
#include "lux.h"
#include "spectrum.h"
#include "texture.h"
#include "equalspd.h"
#include "paramset.h"

// TODO - radiance - add methods for Power and Illuminant propagation

namespace lux
{

// WrinkledTexture Declarations
template <class T> class WrinkledTexture : public Texture<T> {
public:
	// WrinkledTexture Public Methods
	virtual ~WrinkledTexture() {
		delete mapping;
	}
	WrinkledTexture(int oct, float roughness, TextureMapping3D *map) {
		omega = roughness;
		octaves = oct;
		mapping = map;
	}
	virtual T Evaluate(const TsPack *tspack, const DifferentialGeometry &dg) const {
		Vector dpdx, dpdy;
		Point P = mapping->Map(dg, &dpdx, &dpdy);
		return Turbulence(P, dpdx, dpdy, omega, octaves);
	}
	virtual float Y() const { return EqualSPD(.5f).Y(); }
	virtual float Filter() const { return .5f; }
	
	static Texture<float> * CreateFloatTexture(const Transform &tex2world, const ParamSet &tp);
	static Texture<SWCSpectrum> * CreateSWCSpectrumTexture(const Transform &tex2world, const ParamSet &tp);
private:
	// WrinkledTexture Private Data
	int octaves;
	float omega;
	TextureMapping3D *mapping;
};


// WrinkledTexture Method Definitions
template <class T> inline Texture<float> * WrinkledTexture<T>::CreateFloatTexture(const Transform &tex2world,
	const ParamSet &tp) {
	// Initialize 3D texture mapping _map_ from _tp_
	TextureMapping3D *map = new IdentityMapping3D(tex2world);
	// Apply texture specified transformation option for 3D mapping
	IdentityMapping3D *imap = (IdentityMapping3D*) map;
	imap->Apply3DTextureMappingOptions(tp);
	return new WrinkledTexture<float>(tp.FindOneInt("octaves", 8),
		tp.FindOneFloat("roughness", .5f), map);
}

template <class T> inline Texture<SWCSpectrum> * WrinkledTexture<T>::CreateSWCSpectrumTexture(const Transform &tex2world,
	const ParamSet &tp) {
	// Initialize 3D texture mapping _map_ from _tp_
	TextureMapping3D *map = new IdentityMapping3D(tex2world);
	// Apply texture specified transformation option for 3D mapping
	IdentityMapping3D *imap = (IdentityMapping3D*) map;
	imap->Apply3DTextureMappingOptions(tp);
	return new WrinkledTexture<SWCSpectrum>(tp.FindOneInt("octaves", 8),
		tp.FindOneFloat("roughness", .5f), map);
}

}//namespace lux


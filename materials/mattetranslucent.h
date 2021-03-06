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

// mattetranslucent.cpp*
#include "lux.h"
#include "material.h"

namespace lux
{

// MatteTranslucent Class Declarations
class MatteTranslucent : public Material {
public:
	// MatteTranslucent Public Methods
	MatteTranslucent(boost::shared_ptr<Texture<SWCSpectrum> > &kr,
		boost::shared_ptr<Texture<SWCSpectrum> > &kt,
		boost::shared_ptr<Texture<float> > &sig,
		bool conserving,
		const ParamSet &mp) : Material("MatteTranslucent-" + boost::lexical_cast<string>(this), mp), Kr(kr), Kt(kt), sigma(sig),
		energyConserving(conserving) { }
	virtual ~MatteTranslucent() { }
	virtual BSDF *GetBSDF(luxrays::MemoryArena &arena, const SpectrumWavelengths &sw,
		const Intersection &isect,
		const DifferentialGeometry &dgShading) const;

	Texture<SWCSpectrum> *GetKrTexture() { return Kr.get(); }
	Texture<SWCSpectrum> *GetKtTexture() { return Kt.get(); }
	Texture<float> *GetSigmaTexture() { return sigma.get(); }

	static Material * CreateMaterial(const Transform &xform,
		const ParamSet &mp);
private:
	// MatteTranslucent Private Data
	boost::shared_ptr<Texture<SWCSpectrum> > Kr, Kt;
	boost::shared_ptr<Texture<float> > sigma;
	bool energyConserving;
};

}//namespace lux


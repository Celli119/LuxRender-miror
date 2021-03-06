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

#ifndef LUX_ORENNAYAR_H
#define LUX_ORENNAYAR_H
// orennayar.h*
#include "lux.h"
#include "bxdf.h"
#include "luxrays/core/color/swcspectrum.h"

namespace lux
{

class  OrenNayar : public BxDF {
public:
	// OrenNayar Public Methods
	OrenNayar(const SWCSpectrum &reflectance, float sig)
		: BxDF(BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE)),
		  R(reflectance) {
		float sigma = luxrays::Radians(sig);
		float sigma2 = sigma*sigma;
		A = 1.f - (sigma2 / (2.f * (sigma2 + 0.33f)));
		B = 0.45f * sigma2 / (sigma2 + 0.09f);
	}
	virtual ~OrenNayar() { }
	virtual void F(const SpectrumWavelengths &sw, const Vector &wo,
		const Vector &wi, SWCSpectrum *const f) const;
	bool SampleF(const SpectrumWavelengths &sw, const Vector &wo,
		Vector *wi, float u1, float u2, SWCSpectrum *const f_,
		float *pdf, float *pdfBack, bool reverse) const;
private:
	// OrenNayar Private Data
	SWCSpectrum R;
	float A, B;
};

}//namespace lux

#endif // LUX_ORENNAYAR_H


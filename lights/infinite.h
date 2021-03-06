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

#include "luxrays/core/color/spds/rgbillum.h"
using luxrays::RGBIllumSPD;

#include "lux.h"
#include "light.h"
#include "texture.h"
#include "mipmap.h"

namespace lux
{

// InfiniteAreaLight Declarations
class InfiniteAreaLight : public Light {
public:
	// InfiniteAreaLight Public Methods
	InfiniteAreaLight(const Transform &light2world, const RGBColor &l,
		u_int ns, const string &texmap, EnvironmentMapping *m,
		float gain, float gamma);
	virtual ~InfiniteAreaLight();
	virtual float Power(const Scene &scene) const;
	virtual bool IsDeltaLight() const { return false; }
	virtual bool IsEnvironmental() const { return true; }
	virtual bool Le(const Scene &scene, const Sample &arena, const Ray &r,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *Le) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3, BSDF **bsdf,
		float *pdf, float *pdfDirect, SWCSpectrum *Le) const;
	
	static Light *CreateLight(const Transform &light2world,
		const ParamSet &paramSet);

	MIPMap *GetRadianceMap() { return radianceMap; }

	MIPMap *radianceMap;
	EnvironmentMapping *mapping;
private:
	// Used by Queryable interface
	float GetColorR() { return lightColor.c[0]; }
	float GetColorG() { return lightColor.c[1]; }
	float GetColorB() { return lightColor.c[2]; }

	RGBColor lightColor;
	float gain, gamma;

	// InfiniteAreaLight Private Data
	RGBIllumSPD SPDbase;
};

}//namespace lux



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

// goniometric.cpp*
#include "goniometric.h"

using namespace lux;

// GonioPhotometricLight Method Definitions
Spectrum GonioPhotometricLight::Sample_L(const Point &p, Vector *wi,
		VisibilityTester *visibility) const {
	*wi = Normalize(lightPos - p);
	visibility->SetSegment(p, lightPos);
	return Intensity * Scale(-*wi) / DistanceSquared(lightPos, p);
}
GonioPhotometricLight::GonioPhotometricLight(
		const Transform &light2world,
		const Spectrum &intensity, const string &texname)
	: Light(light2world) {
	lightPos = LightToWorld(Point(0,0,0));
	Intensity = intensity;
	// Create _mipmap_ for _GonioPhotometricLight_
	int width, height;
	Spectrum *texels = ReadImage(texname, &width, &height);
	if (texels) {
		mipmap = new MIPMap<Spectrum>(width, height, texels);
		delete[] texels;
	}
	else mipmap = NULL;
}
Spectrum GonioPhotometricLight::Sample_L(const Point &P, float u1, float u2,
		Vector *wo, float *pdf,
		VisibilityTester *visibility) const {
	*wo = Normalize(lightPos - P);
	*pdf = 1.f;
	visibility->SetSegment(P, lightPos);
	return Intensity * Scale(-*wo) / DistanceSquared(lightPos, P);
}
Spectrum GonioPhotometricLight::Sample_L(const Scene *scene, float u1, float u2,
		float u3, float u4, Ray *ray, float *pdf) const {
	ray->o = lightPos;
	ray->d = UniformSampleSphere(u1, u2);
	*pdf = UniformSpherePdf();
	return Intensity * Scale(ray->d);
}
float GonioPhotometricLight::Pdf(const Point &, const Vector &) const {
	return 0.;
}
Light* GonioPhotometricLight::CreateLight(const Transform &light2world,
		const ParamSet &paramSet) {
	Spectrum I = paramSet.FindOneSpectrum("I", Spectrum(1.0));
	string texname = paramSet.FindOneString("mapname", "");
	return new GonioPhotometricLight(light2world, I, texname);
}
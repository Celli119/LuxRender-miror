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

// volume.cpp*
#include "volume.h"

namespace lux
{
// Volume Scattering Definitions

float PhaseIsotropic(const Vector &, const Vector &) {
	return 1.f / (4.f * M_PI);
}
 float PhaseRayleigh(const Vector &w, const Vector &wp) {
	const float costheta = Dot(w, wp);
	return  3.f / (16.f * M_PI) * (1.f + costheta * costheta);
}
 float PhaseMieHazy(const Vector &w, const Vector &wp) {
	const float costheta = Dot(w, wp);
	return (0.5f + 4.5f * powf(0.5f * (1.f + costheta), 8.f)) / (4.f * M_PI);
}
 float PhaseMieMurky(const Vector &w, const Vector &wp) {
	const float costheta = Dot(w, wp);
	return (0.5f + 16.5f * powf(0.5f * (1.f + costheta), 32.f)) / (4.f * M_PI);
}

float PhaseHG(const Vector &w, const Vector &wp, float g) {
	const float costheta = Dot(w, wp);
	return 1.f / (4.f * M_PI) * (1.f - g * g) /
		powf(1.f + g * g - 2.f * g * costheta, 1.5f);
}

float PhaseSchlick(const Vector &w,
                   const Vector &wp, float g) {
	const float k = g * (1.55f - .55f * g * g);
	const float compkcostheta = 1.f - k * Dot(w, wp);
	return (1.f - k * k) / (4.f * M_PI * compkcostheta * compkcostheta);
}
RGBColor VolumeRegion::sigma_t(const Point &p,
                               const Vector &w) const {
	return sigma_a(p, w) + sigma_s(p, w);
}
DensityRegion::DensityRegion(const RGBColor &sa,
		const RGBColor &ss, float gg,
	 	const RGBColor &emit,
		const Transform &VolumeToWorld)
	: sig_a(sa), sig_s(ss), le(emit), g(gg)  {
	WorldToVolume = VolumeToWorld.GetInverse();
}
AggregateVolume::
	AggregateVolume(const vector<VolumeRegion *> &r) {
	regions = r;
	for (u_int i = 0; i < regions.size(); ++i)
		bound = Union(bound, regions[i]->WorldBound());
}
RGBColor AggregateVolume::sigma_a(const Point &p,
		const Vector &w) const {
	RGBColor s(0.);
	for (u_int i = 0; i < regions.size(); ++i)
		s += regions[i]->sigma_a(p, w);
	return s;
}
RGBColor AggregateVolume::sigma_s(const Point &p, const Vector &w) const {
	RGBColor s(0.);
	for (u_int i = 0; i < regions.size(); ++i)
		s += regions[i]->sigma_s(p, w);
	return s;
}
RGBColor AggregateVolume::Lve(const Point &p, const Vector &w) const {
	RGBColor L(0.);
	for (u_int i = 0; i < regions.size(); ++i)
		L += regions[i]->Lve(p, w);
	return L;
}
float AggregateVolume::P(const Point &p, const Vector &w, const Vector &wp) const {
	float ph = 0, sumWt = 0;
	for (u_int i = 0; i < regions.size(); ++i) {
		float sigt = regions[i]->sigma_t(p, w).Y();
		if (sigt != 0.f) {
			float wt = regions[i]->sigma_s(p, w).Y() / sigt;
			sumWt += wt;
			ph += wt * regions[i]->P(p, w, wp);
		}
	}
	return ph / sumWt;
}
RGBColor AggregateVolume::sigma_t(const Point &p, const Vector &w) const {
	RGBColor s(0.);
	for (u_int i = 0; i < regions.size(); ++i)
		s += regions[i]->sigma_t(p, w);
	return s;
}
RGBColor AggregateVolume::Tau(const Ray &ray, float step, float offset) const {
	RGBColor t(0.);
	for (u_int i = 0; i < regions.size(); ++i)
		t += regions[i]->Tau(ray, step, offset);
	return t;
}
bool AggregateVolume::IntersectP(const Ray &ray,
		float *t0, float *t1) const {
	*t0 = INFINITY;
	*t1 = -INFINITY;
	for (u_int i = 0; i < regions.size(); ++i) {
		float tr0, tr1;
		if (regions[i]->IntersectP(ray, &tr0, &tr1)) {
			*t0 = min(*t0, tr0);
			*t1 = max(*t1, tr1);
		}
	}
	return (*t0 < *t1);
}
AggregateVolume::~AggregateVolume() {
	for (u_int i = 0; i < regions.size(); ++i)
		delete regions[i];
}
BBox AggregateVolume::WorldBound() const {
	return bound;
}
RGBColor DensityRegion::Tau(const Ray &r,
		float stepSize, float u) const {
	float t0, t1;
	float length = r.d.Length();
	if (length == 0.f) return 0.f;
	Ray rn(r.o, r.d / length,
	       r.mint * length,
		   r.maxt * length);
	if (!IntersectP(rn, &t0, &t1)) return 0.;
	RGBColor tau(0.);
	t0 += u * stepSize;
	while (t0 < t1) {
		tau += sigma_t(rn(t0), -rn.d);
		t0 += stepSize;
	}
	return tau * stepSize;
}

}//namespace lux


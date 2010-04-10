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

// bxdf.cpp*
#include "bxdf.h"
#include "spectrum.h"
#include "spectrumwavelengths.h"
#include "mc.h"
#include "sampling.h"
#include "fresnel.h"
#include "volume.h"

using namespace lux;

// BxDF Method Definitions
bool BxDF::Sample_f(const TsPack *tspack, const Vector &wo, Vector *wi,
	float u1, float u2, SWCSpectrum *const f_, float *pdf, float *pdfBack,
	bool reverse) const
{
	// Cosine-sample the hemisphere, flipping the direction if necessary
	*wi = CosineSampleHemisphere(u1, u2);
	if (wo.z < 0.f) wi->z *= -1.f;
	// wi may be in the tangent plane, which will 
	// fail the SameHemisphere test in Pdf()
	if (!SameHemisphere(wo, *wi)) 
		return false;
	*pdf = Pdf(tspack, wo, *wi);
	if (pdfBack)
		*pdfBack = Pdf(tspack, *wi, wo);
	*f_ = SWCSpectrum(0.f);
	if (reverse) {
		this->f(tspack, *wi, wo, f_);
	}
	else
		this->f(tspack, wo, *wi, f_);
	return true;
}
float BxDF::Pdf(const TsPack *tspack, const Vector &wo, const Vector &wi) const {
	return SameHemisphere(wo, wi) ? fabsf(wi.z) * INV_PI : 0.f;
}

SWCSpectrum BxDF::rho(const TsPack *tspack, const Vector &w, u_int nSamples,
		float *samples) const {
	if (!samples) {
		samples =
			static_cast<float *>(alloca(2 * nSamples * sizeof(float)));
		LatinHypercube(tspack, samples, nSamples, 2);
	}
	SWCSpectrum r(0.f);
	for (u_int i = 0; i < nSamples; ++i) {
		// Estimate one term of $\rho_{dh}$
		Vector wi;
		float pdf = 0.f;
		SWCSpectrum f_(0.f);		
		if (Sample_f(tspack, w, &wi, samples[2*i], samples[2*i+1], &f_, &pdf) && pdf > 0.f) 
			r.AddWeighted(fabsf(wi.z) / pdf, f_);
	}
	return r / nSamples;
}
SWCSpectrum BxDF::rho(const TsPack *tspack, u_int nSamples, float *samples) const {
	if (!samples) {
		samples =
			static_cast<float *>(alloca(4 * nSamples * sizeof(float)));
		LatinHypercube(tspack, samples, nSamples, 4);
	}
	SWCSpectrum r(0.f);
	for (u_int i = 0; i < nSamples; ++i) {
		// Estimate one term of $\rho_{hh}$
		Vector wo, wi;
		wo = UniformSampleHemisphere(samples[4*i], samples[4*i+1]);
		float pdf_o = INV_TWOPI, pdf_i = 0.f;
		SWCSpectrum f_(0.f);			
		if (Sample_f(tspack, wo, &wi, samples[4*i+2], samples[4*i+3], &f_, &pdf_i) && pdf_i > 0.f)
			r.AddWeighted(fabsf(wi.z * wo.z) / (pdf_o * pdf_i), f_);
	}
	return r / (M_PI * nSamples);
}
float BxDF::Weight(const TsPack *tspack, const Vector &wo) const
{
	return 1.f;
}
BSDF::BSDF(const DifferentialGeometry &dg, const Normal &ngeom,
	const Volume *ex, const Volume *in)
	: nn(dg.nn), ng(ngeom), dgShading(dg), exterior(ex), interior(in)
{
	sn = Normalize(dgShading.dpdu);
	tn = Cross(nn, sn);
	compParams = NULL; 
}
float BSDF::Eta(const TsPack *tspack) const
{
	if (exterior) {
		const Fresnel *fre = exterior->Fresnel(tspack, dgShading.p,
			Vector(dgShading.nn));
		if (interior) {
			const Fresnel *fri = interior->Fresnel(tspack,
				dgShading.p, Vector(dgShading.nn));
			return fri->Index(tspack) / fre->Index(tspack);
		}
		return 1.f / fre->Index(tspack);
	} else if (interior) {
		const Fresnel *fri = interior->Fresnel(tspack, dgShading.p,
			Vector(dgShading.nn));
		return fri->Index(tspack);
	}
	return 1.f;
}
void BSDF::ComputeReflectionDifferentials(const RayDifferential &ray,
	RayDifferential &rd) const
{
	if (!ray.hasDifferentials)
		return;
	rd.rx.o = rd.o + dgShading.dpdx;
	rd.ry.o = rd.o + dgShading.dpdy;
	// Compute differential reflected directions
	const Normal dndx(dgShading.dndu * dgShading.dudx +
		dgShading.dndv * dgShading.dvdx);
	const Normal dndy(dgShading.dndu * dgShading.dudy +
		dgShading.dndv * dgShading.dvdy);
	const Vector dwodx(ray.d - ray.rx.d), dwody(ray.d - ray.ry.d);
	const float dDNdx = Dot(dwodx, nn) - Dot(ray.d, dndx);
	const float dDNdy = Dot(dwody, nn) - Dot(ray.d, dndy);
	rd.rx.d = rd.d - dwodx +
		2.f * Vector(Dot(ray.d, nn) * dndx + dDNdx * nn);
	rd.ry.d = rd.d - dwody +
		2.f * Vector(Dot(ray.d, nn) * dndy + dDNdy * nn);
	rd.hasDifferentials = true;
}
void BSDF::ComputeTransmissionDifferentials(const TsPack *tspack,
	const RayDifferential &ray, RayDifferential &rd) const
{
	if (!ray.hasDifferentials)
		return;
	rd.rx.o = rd.o + dgShading.dpdx;
	rd.ry.o = rd.o + dgShading.dpdy;

	const float cosi = Dot(ray.d, nn), coso = Dot(rd.d, nn);
	float eta = Eta(tspack);
	if (cosi > 0.f)
		eta = 1.f / eta;

	const Normal dndx(dgShading.dndu * dgShading.dudx +
		dgShading.dndv * dgShading.dvdx);
	const Normal dndy(dgShading.dndu * dgShading.dudy +
		dgShading.dndv * dgShading.dvdy);

	const Vector dwodx(ray.d - ray.rx.d), dwody(ray.d - ray.ry.d);
	const float dDNdx = Dot(dwodx, nn) - Dot(ray.d, dndx);
	const float dDNdy = Dot(dwody, nn) - Dot(ray.d, dndy);

	const float mu = eta * cosi - coso;
	const float dmudn = -mu * eta / coso;
	const float dmudx = dmudn * dDNdx;
	const float dmudy = dmudn * dDNdy;

	rd.rx.d = rd.d + eta * dwodx - Vector(mu * dndx + dmudx * nn);
	rd.ry.d = rd.d + eta * dwody - Vector(mu * dndy + dmudy * nn);
	rd.hasDifferentials = true;
}
bool SingleBSDF::Sample_f(const TsPack *tspack, const Vector &woW, Vector *wiW,
	float u1, float u2, float u3, SWCSpectrum *const f_, float *pdf,
	BxDFType flags, BxDFType *sampledType, float *pdfBack,
	bool reverse) const
{
	BOOST_ASSERT(bxdf); // NOBOOK
	if (!bxdf->MatchesFlags(flags))
		return false;
	// Sample chosen _BxDF_
	Vector wi;
	if (!bxdf->Sample_f(tspack, WorldToLocal(woW), &wi, u1, u2, f_,
		pdf, pdfBack, reverse))
		return false;
	if (sampledType)
		*sampledType = bxdf->type;
	*wiW = LocalToWorld(wi);
	const float sideTest = Dot(*wiW, ng) * Dot(woW, ng);
	if (sideTest > 0.f) {
		// ignore BTDFs
		if (bxdf->type & BSDF_TRANSMISSION)
			return false;
	} else if (sideTest < 0.f) {
		// ignore BRDFs
		if (bxdf->type & BSDF_REFLECTION)
			return false;
	} else
		return false;
	return true;
}
float SingleBSDF::Pdf(const TsPack *tspack, const Vector &woW,
	const Vector &wiW, BxDFType flags) const
{
	if (!bxdf->MatchesFlags(flags))
		return 0.f;
	return bxdf->Pdf(tspack, WorldToLocal(woW), WorldToLocal(wiW));
}

SWCSpectrum SingleBSDF::f(const TsPack *tspack, const Vector &woW,
	const Vector &wiW, BxDFType flags) const
{
	const float sideTest = Dot(wiW, ng) * Dot(woW, ng);
	if (sideTest > 0.f)
		// ignore BTDFs
		flags = BxDFType(flags & ~BSDF_TRANSMISSION);
	else if (sideTest < 0.f)
		// ignore BRDFs
		flags = BxDFType(flags & ~BSDF_REFLECTION);
	else
		flags = static_cast<BxDFType>(0);
	if (!bxdf->MatchesFlags(flags))
		return SWCSpectrum(0.f);
	SWCSpectrum f_(0.f);
	bxdf->f(tspack, WorldToLocal(woW), WorldToLocal(wiW), &f_);
	return f_;
}
SWCSpectrum SingleBSDF::rho(const TsPack *tspack, BxDFType flags) const
{
	if (!bxdf->MatchesFlags(flags))
		return SWCSpectrum(0.f);
	return bxdf->rho(tspack);
}
SWCSpectrum SingleBSDF::rho(const TsPack *tspack, const Vector &woW,
	BxDFType flags) const
{
	if (!bxdf->MatchesFlags(flags))
		return SWCSpectrum(0.f);
	return bxdf->rho(tspack, WorldToLocal(woW));
}
MultiBSDF::MultiBSDF(const DifferentialGeometry &dg, const Normal &ngeom,
	const Volume *exterior, const Volume *interior) :
	BSDF(dg, ngeom, exterior, interior)
{
	nBxDFs = 0;
}
bool MultiBSDF::Sample_f(const TsPack *tspack, const Vector &woW, Vector *wiW,
	float u1, float u2, float u3, SWCSpectrum *const f_, float *pdf,
	BxDFType flags, BxDFType *sampledType, float *pdfBack,
	bool reverse) const
{
	float weights[MAX_BxDFS];
	// Choose which _BxDF_ to sample
	Vector wo(WorldToLocal(woW));
	u_int matchingComps = 0;
	float totalWeight = 0.f;
	for (u_int i = 0; i < nBxDFs; ++i) {
		if (bxdfs[i]->MatchesFlags(flags)) {
			weights[i] = bxdfs[i]->Weight(tspack, wo);
			totalWeight += weights[i];
			++matchingComps;
		} else
			weights[i] = 0.f;
	}
	if (matchingComps == 0 || !(totalWeight > 0.f)) {
		*pdf = 0.f;
		if (pdfBack)
			*pdfBack = 0.f;
		return false;
	}
	u3 *= totalWeight;
	u_int which = 0;
	for (u_int i = 0; i < nBxDFs; ++i) {
		if (weights[i] > 0.f) {
			which = i;
			u3 -= weights[i];
			if (u3 < 0.f) {
				break;
			}
		}
	}
	BxDF *bxdf = bxdfs[which];
	BOOST_ASSERT(bxdf); // NOBOOK
	// Sample chosen _BxDF_
	Vector wi;
	*pdf = 0.f;
	if (pdfBack)
		*pdfBack = 0.f;
	if (!bxdf->Sample_f(tspack, wo, &wi, u1, u2, f_, pdf, pdfBack, reverse))
		return false;
	if (sampledType) *sampledType = bxdf->type;
	*wiW = LocalToWorld(wi);
	*pdf *= weights[which];
	float totalWeightR = bxdfs[which]->Weight(tspack, wi);
	if (pdfBack)
		*pdfBack *= totalWeightR;
	// Compute overall PDF with all matching _BxDF_s
	// Compute value of BSDF for sampled direction
	const float sideTest = Dot(*wiW, ng) * Dot(woW, ng);
	BxDFType flags2;
	if (sideTest > 0.f)
		// ignore BTDFs
		flags2 = BxDFType(flags & ~BSDF_TRANSMISSION);
	else if (sideTest < 0.f)
		// ignore BRDFs
		flags2 = BxDFType(flags & ~BSDF_REFLECTION);
	else
		return false;
	if (!(bxdf->type & BSDF_SPECULAR) && matchingComps > 1) {
		if (!bxdf->MatchesFlags(flags2))
			*f_ = SWCSpectrum(0.f);
		for (u_int i = 0; i < nBxDFs; ++i) {
			if (i== which)
				continue;
			if (bxdfs[i]->MatchesFlags(flags2)) {
				if (reverse)
					bxdfs[i]->f(tspack, wi, wo, f_);
				else
					bxdfs[i]->f(tspack, wo, wi, f_);
			}
			if (bxdfs[i]->MatchesFlags(flags)) {
				*pdf += bxdfs[i]->Pdf(tspack, wo, wi) *
					weights[i];
				if (pdfBack) {
					const float weightR = bxdfs[i]->Weight(tspack, wi);
					*pdfBack += bxdfs[i]->Pdf(tspack, wi, wo) *
						weightR;
					totalWeightR += weightR;
				}
			}
		}
	}
	*pdf /= totalWeight;
	if (pdfBack)
		*pdfBack /= totalWeightR;
	return true;
}
float MultiBSDF::Pdf(const TsPack *tspack, const Vector &woW, const Vector &wiW,
	BxDFType flags) const
{
	Vector wo(WorldToLocal(woW)), wi(WorldToLocal(wiW));
	float pdf = 0.f;
	float totalWeight = 0.f;
	for (u_int i = 0; i < nBxDFs; ++i)
		if (bxdfs[i]->MatchesFlags(flags)) {
			float weight = bxdfs[i]->Weight(tspack, wo);
			pdf += bxdfs[i]->Pdf(tspack, wo, wi) * weight;
			totalWeight += weight;
		}
	return totalWeight > 0.f ? pdf / totalWeight : 0.f;
}

SWCSpectrum MultiBSDF::f(const TsPack *tspack, const Vector &woW,
		const Vector &wiW, BxDFType flags) const
{
	const float sideTest = Dot(wiW, ng) * Dot(woW, ng);
	if (sideTest > 0.f)
		// ignore BTDFs
		flags = BxDFType(flags & ~BSDF_TRANSMISSION);
	else if (sideTest < 0.f)
		// ignore BRDFs
		flags = BxDFType(flags & ~BSDF_REFLECTION);
	else
		flags = static_cast<BxDFType>(0);
	Vector wi(WorldToLocal(wiW)), wo(WorldToLocal(woW));
	SWCSpectrum f_(0.f);
	for (u_int i = 0; i < nBxDFs; ++i)
		if (bxdfs[i]->MatchesFlags(flags))
			bxdfs[i]->f(tspack, wo, wi, &f_);
	return f_;
}
SWCSpectrum MultiBSDF::rho(const TsPack *tspack, BxDFType flags) const
{
	SWCSpectrum ret(0.f);
	for (u_int i = 0; i < nBxDFs; ++i)
		if (bxdfs[i]->MatchesFlags(flags))
			ret += bxdfs[i]->rho(tspack);
	return ret;
}
SWCSpectrum MultiBSDF::rho(const TsPack *tspack, const Vector &woW,
	BxDFType flags) const
{
	Vector wo(WorldToLocal(woW));
	SWCSpectrum ret(0.f);
	for (u_int i = 0; i < nBxDFs; ++i)
		if (bxdfs[i]->MatchesFlags(flags))
			ret += bxdfs[i]->rho(tspack, wo);
	return ret;
}

MixBSDF::MixBSDF(const DifferentialGeometry &dgs, const Normal &ngeom,
	const Volume *exterior, const Volume *interior) :
	BSDF(dgs, ngeom, exterior, interior), totalWeight(1.f)
{
	// totalWeight is initialized to 1 to avoid divisions by 0 when there
	// are no components in the mix
	nBSDFs = 0;
}
bool MixBSDF::Sample_f(const TsPack *tspack, const Vector &wo, Vector *wi,
	float u1, float u2, float u3, SWCSpectrum *const f_, float *pdf,
	BxDFType flags, BxDFType *sampledType, float *pdfBack,
	bool reverse) const
{
	if (nBSDFs == 0)
		return false;
	u3 *= totalWeight;
	u_int which = 0;
	for (u_int i = 0; i < nBSDFs; ++i) {
		if (u3 < weights[i]) {
			which = i;
			break;
		}
		u3 -= weights[i];
	}
	if (!bsdfs[which]->Sample_f(tspack,
		wo, wi, u1, u2, u3 / weights[which], f_, pdf, flags,
		sampledType, pdfBack, reverse))
		return false;
	// To make bump map work, we must compensate for the shading normal
	// that can be different in the sub BSDF than in the mix. The values
	// will always end up being multiplied by the cosine between the
	// incoming light ray and the shading normal of the mix. By convention,
	// the incoming light ray will be *wi if reverse and wo otherwise.
	// Thus we divide by the cosine of the light ray and the mix shading
	// normal and multiply by the cosine of the light ray and the sub BSDF
	// shading normal.
	if (reverse)
		*f_ *= weights[which] * AbsDot(*wi, bsdfs[which]->nn) /
			AbsDot(*wi, nn);
	else
		*f_ *= weights[which] * AbsDot(wo, bsdfs[which]->nn) /
			AbsDot(wo, nn);
	*pdf *= weights[which];
	if (pdfBack)
		*pdfBack *= weights[which];
	for (u_int i = 0; i < nBSDFs; ++i) {
		if (i == which)
			continue;
		BSDF *bsdf = bsdfs[i];
		// Same trick than above for the shading normal
		if (reverse)
			f_->AddWeighted(weights[i] * AbsDot(*wi, bsdfs[i]->nn) /
				AbsDot(*wi, nn),
				bsdf->f(tspack, *wi, wo, flags));
		else
			f_->AddWeighted(weights[i] * AbsDot(wo, bsdfs[i]->nn) /
				AbsDot(wo, nn),
				bsdf->f(tspack, wo, *wi, flags));
		*pdf += weights[i] * bsdf->Pdf(tspack, wo, *wi, flags);
		if (pdfBack)
			*pdfBack += weights[i] * bsdf->Pdf(tspack, *wi, wo, flags);
	}
	*f_ /= totalWeight;
	*pdf /= totalWeight;
	if (pdfBack)
		*pdfBack /= totalWeight;
	return true;
}
float MixBSDF::Pdf(const TsPack *tspack, const Vector &wo, const Vector &wi,
	BxDFType flags) const
{
	float pdf = 0.f;
	for (u_int i = 0; i < nBSDFs; ++i)
		pdf += weights[i] * bsdfs[i]->Pdf(tspack, wo, wi, flags);
	return pdf / totalWeight;
}
SWCSpectrum MixBSDF::f(const TsPack *tspack, const Vector &woW,
	const Vector &wiW, BxDFType flags) const
{
	SWCSpectrum ff(0.f);
	for (u_int i = 0; i < nBSDFs; ++i) {
		// To make bump map work, we must compensate for the shading
		// normal that can be different in the sub BSDF than in the mix.
		// The values will always end up being multiplied by the cosine
		// between the incoming light ray and the shading normal of the
		// mix. By convention, the incoming light ray should always be
		// in woW.
		// Thus we divide by the cosine of woW and the mix shading
		// normal and multiply by the cosine of woW and the sub BSDF
		// shading normal.
		ff.AddWeighted(weights[i] * AbsDot(woW, bsdfs[i]->nn) /
			AbsDot(woW, nn), bsdfs[i]->f(tspack, woW, wiW, flags));
	}
	return ff / totalWeight;
}
SWCSpectrum MixBSDF::rho(const TsPack *tspack, BxDFType flags) const
{
	SWCSpectrum ret(0.f);
	for (u_int i = 0; i < nBSDFs; ++i)
		ret.AddWeighted(weights[i], bsdfs[i]->rho(tspack, flags));
	ret /= totalWeight;
	return ret;
}
SWCSpectrum MixBSDF::rho(const TsPack *tspack, const Vector &wo,
	BxDFType flags) const
{
	SWCSpectrum ret(0.f);
	for (u_int i = 0; i < nBSDFs; ++i)
		ret.AddWeighted(weights[i], bsdfs[i]->rho(tspack, wo, flags));
	ret /= totalWeight;
	return ret;
}
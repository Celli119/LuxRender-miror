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

// path.cpp*
#include "path.h"
#include "bxdf.h"
#include "light.h"
#include "camera.h"
#include "paramset.h"
#include "dynload.h"

using namespace lux;

static const u_int passThroughLimit = 10000;

// PathIntegrator Method Definitions
void PathIntegrator::RequestSamples(Sample *sample, const Scene *scene)
{
	if (lightStrategy == SAMPLE_AUTOMATIC) {
		if (scene->sampler->IsMutating() || scene->lights.size() > 5)
			lightStrategy = SAMPLE_ONE_UNIFORM;
		else
			lightStrategy = SAMPLE_ALL_UNIFORM;
	}

	vector<u_int> structure;
	structure.push_back(2);	// light position sample
	structure.push_back(1);	// light number sample
	structure.push_back(2);	// bsdf direction sample for light
	structure.push_back(1);	// bsdf component sample for light
	structure.push_back(2);	// bsdf direction sample for path
	structure.push_back(1);	// bsdf component sample for path
	if (rrStrategy != RR_NONE)
		structure.push_back(1);	// continue sample
	sampleOffset = sample->AddxD(structure, maxDepth + 1);
}
void PathIntegrator::Preprocess(const TsPack *tspack, const Scene *scene)
{
	// Prepare image buffers
	BufferType type = BUF_TYPE_PER_PIXEL;
	scene->sampler->GetBufferType(&type);
	bufferId = scene->camera->film->RequestBuffer(type, BUF_FRAMEBUFFER, "eye");
}

u_int PathIntegrator::Li(const TsPack *tspack, const Scene *scene,
	const Sample *sample) const
{
	u_int nrContribs = 0;
	// Declare common path integration variables
	RayDifferential r;
	float rayWeight = tspack->camera->GenerateRay(*sample, &r);
	if (rayWeight > 0.f) {
		// Generate ray differentials for camera ray
		++(sample->imageX);
		float wt1 = tspack->camera->GenerateRay(*sample, &r.rx);
		--(sample->imageX);
		++(sample->imageY);
		float wt2 = tspack->camera->GenerateRay(*sample, &r.ry);
		r.hasDifferentials = (wt1 > 0.f) && (wt2 > 0.f);
		--(sample->imageY);
	}

	RayDifferential ray(r);
	SWCSpectrum pathThroughput(1.0f);
	vector<SWCSpectrum> L(scene->lightGroups.size(), SWCSpectrum(0.f));
	vector<float> V(scene->lightGroups.size(), 0.f);
	float VContrib = .1f;
	bool specularBounce = true, specular = true;
	float alpha = 1.f;
	float distance = INFINITY;
	u_int through = 0;
	for (u_int pathLength = 0; ; ++pathLength) {
		// Find next vertex of path
		Intersection isect;
		if (!scene->Intersect(ray, &isect)) {
			if (pathLength == 0) {
				// Dade - now I know ray.maxt and I can call volumeIntegrator
				SWCSpectrum Lv;
				u_int g = scene->volumeIntegrator->Li(tspack, scene, ray, sample, &Lv, &alpha);
				if (!Lv.Black()) {
					L[g] = Lv;
					V[g] += Lv.Filter(tspack) * VContrib;
					++nrContribs;
				}
				pathThroughput = 1.f;
				scene->volumeIntegrator->Transmittance(tspack, scene, ray, sample, &alpha, &pathThroughput);
			}

			// Stop path sampling since no intersection was found
			// Possibly add horizon in render & reflections
			if (includeEnvironment || pathLength > 0) {
				if (specularBounce) {
					for (u_int i = 0; i < scene->lights.size(); ++i) {
						SWCSpectrum Le(scene->lights[i]->Le(tspack, ray));
						Le *= pathThroughput;
						if (!Le.Black()) {
							L[scene->lights[i]->group] += Le;
							V[scene->lights[i]->group] += Le.Filter(tspack) * VContrib;
							++nrContribs;
						}
					}
				}
			}

			// Set alpha channel
			if (pathLength == 0)
				alpha = 0.f;
			break;
		}
		if (pathLength == 0 && through == 0) {
			r.maxt = ray.maxt;
			distance = ray.maxt * ray.d.Length();
		}

		SWCSpectrum Lv;
		u_int g = scene->volumeIntegrator->Li(tspack, scene, ray, sample, &Lv, &alpha);
		if (!Lv.Black()) {
			Lv *= pathThroughput;
			L[g] += Lv;
			V[g] += Lv.Filter(tspack) * VContrib;
			++nrContribs;
		}
		scene->volumeIntegrator->Transmittance(tspack, scene, ray, sample, &alpha, &pathThroughput);

		// Possibly add emitted light at path vertex
		Vector wo(-ray.d);
		if (specularBounce) {
			SWCSpectrum Le(isect.Le(tspack, wo));
			if (!Le.Black()) {
				Le *= pathThroughput;
				L[isect.arealight->group] += Le;
				V[isect.arealight->group] += Le.Filter(tspack) * VContrib;
				++nrContribs;
			}
		}
		if (pathLength == maxDepth)
			break;
		// Evaluate BSDF at hit point
		float *data = sample->sampler->GetLazyValues(const_cast<Sample *>(sample), sampleOffset, pathLength);
		BSDF *bsdf = isect.GetBSDF(tspack, ray);
		// Sample illumination from lights to find path contribution
		const Point &p = bsdf->dgShading.p;
		const Normal &n = bsdf->dgShading.nn;

		SWCSpectrum Ll;
		switch (lightStrategy) {
			case SAMPLE_ALL_UNIFORM:
			{
				const u_int nLights = scene->lights.size();
				if (nLights == 0)
					break;
				const float lIncrement = 1.f / nLights;
				float l = data[2] * lIncrement;
				for (u_int i = 0; i < nLights; ++i, l += lIncrement) {
					g = UniformSampleOneLight(tspack, scene, p, n,
						wo, bsdf, sample,
						data, &l, data + 3, data + 5, &Ll);
					if (!Ll.Black()) {
						Ll *= pathThroughput;
						Ll *= lIncrement;
						L[g] += Ll;
						V[g] += Ll.Filter(tspack) * VContrib;
						++nrContribs;
					}
				}
				break;
			}
			case SAMPLE_ONE_UNIFORM:
				g = UniformSampleOneLight(tspack, scene, p, n,
					wo, bsdf, sample,
					data, data + 2, data + 3, data + 5, &Ll);
				if (!Ll.Black()) {
					Ll *= pathThroughput;
					L[g] += Ll;
					V[g] += Ll.Filter(tspack) * VContrib;
					++nrContribs;
				}
				break;
			default:
				Ll = 0.f;
				g = 0;
		}

		// Sample BSDF to get new path direction
		Vector wi;
		float pdf;
		BxDFType flags;
		SWCSpectrum f;
		if (!bsdf->Sample_f(tspack, wo, &wi, data[6], data[7], data[8], &f, &pdf, BSDF_ALL, &flags, NULL, true))
			break;

		const float dp = AbsDot(wi, n) / pdf;

		// Possibly terminate the path
		if (pathLength > 3) {
			if (rrStrategy == RR_EFFICIENCY) { // use efficiency optimized RR
				const float q = min<float>(1.f, f.Filter(tspack) * dp);
				if (q < data[9])
					break;
				// increase path contribution
				pathThroughput /= q;
			} else if (rrStrategy == RR_PROBABILITY) { // use normal/probability RR
				if (continueProbability < data[9])
					break;
				// increase path contribution
				pathThroughput /= continueProbability;
			}
		}

		if (flags == (BSDF_TRANSMISSION | BSDF_SPECULAR) && bsdf->Pdf(tspack, wi, wo, BxDFType(BSDF_TRANSMISSION | BSDF_SPECULAR)) > 0.f) {
			if (through++ > passThroughLimit)
				break;
			--pathLength;
		} else
			specularBounce = (flags & BSDF_SPECULAR) != 0;
		specular = specular && specularBounce;
		pathThroughput *= f;
		pathThroughput *= dp;
		if (!specular)
			VContrib += dp;

		ray = RayDifferential(p, wi);
		ray.time = r.time;
	}
	for (u_int i = 0; i < scene->lightGroups.size(); ++i) {
		if (!L[i].Black())
			V[i] /= L[i].Filter(tspack);
		sample->AddContribution(sample->imageX, sample->imageY,
			XYZColor(tspack, L[i]) * rayWeight, alpha, distance,
			V[i], bufferId, i);
	}

	return nrContribs;
}
SurfaceIntegrator* PathIntegrator::CreateSurfaceIntegrator(const ParamSet &params)
{
	// general
	int maxDepth = params.FindOneInt("maxdepth", 16);
	float RRcontinueProb = params.FindOneFloat("rrcontinueprob", .65f);			// continueprobability for plain RR (0.0-1.0)
	LightStrategy estrategy;
	string st = params.FindOneString("strategy", "auto");
	if (st == "one") estrategy = SAMPLE_ONE_UNIFORM;
	else if (st == "all") estrategy = SAMPLE_ALL_UNIFORM;
	else if (st == "auto") estrategy = SAMPLE_AUTOMATIC;
	else {
		std::stringstream ss;
		ss<<"Strategy  '"<<st<<"' for direct lighting unknown. Using \"auto\".";
		luxError(LUX_BADTOKEN,LUX_WARNING,ss.str().c_str());
		estrategy = SAMPLE_AUTOMATIC;
	}
	RRStrategy rstrategy;
	string rst = params.FindOneString("rrstrategy", "efficiency");
	if (rst == "efficiency") rstrategy = RR_EFFICIENCY;
	else if (rst == "probability") rstrategy = RR_PROBABILITY;
	else if (rst == "none") rstrategy = RR_NONE;
	else {
		std::stringstream ss;
		ss<<"Strategy  '"<<rst<<"' for russian roulette path termination unknown. Using \"efficiency\".";
		luxError(LUX_BADTOKEN,LUX_WARNING,ss.str().c_str());
		rstrategy = RR_EFFICIENCY;
	}
	bool include_environment = params.FindOneBool("includeenvironment", true);
	return new PathIntegrator(estrategy, rstrategy, max(maxDepth, 0), RRcontinueProb, include_environment);
}

static DynamicLoader::RegisterSurfaceIntegrator<PathIntegrator> r("path");

/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include "renderers/sppmrenderer.h"
#include "integrators/sppm.h"
#include "camera.h"
#include "film.h"
#include "sampling.h"
#include "light.h"
#include "reflection/bxdf.h"
#include "context.h"
#include "dynload.h"

using namespace lux;

//------------------------------------------------------------------------------
// HaltonEyeSampler methods
//------------------------------------------------------------------------------

HaltonEyeSampler::HaltonEyeSampler(int x0, int x1, int y0, int y1,
	const string &ps) : Sampler(x0, x1, y0, y1, 1)
{
	pixelSampler = MakePixelSampler(ps, x0, x1, y0, y1);
	nPixels = pixelSampler->GetTotalPixels();
	halton.reserve(nPixels);
	haltonOffset.reserve(nPixels);
}

//------------------------------------------------------------------------------
// HitPoints methods
//------------------------------------------------------------------------------

HitPoints::HitPoints(SPPMRenderer *engine, RandomGenerator *rng)  {
	renderer = engine;
	Scene *scene = renderer->scene;
	currentPass = 0;

	wavelengthSampleScramble = rng->uintValue();
	timeSampleScramble = rng->uintValue();
	wavelengthSample = Halton(0, wavelengthSampleScramble);
	timeSample = Halton(0, timeSampleScramble);

	// Get the count of hit points required
	int xstart, xend, ystart, yend;
	scene->camera->film->GetSampleExtent(&xstart, &xend, &ystart, &yend);

	// Set the sampler
	eyeSampler = new HaltonEyeSampler(xstart, xend, ystart, yend,
		renderer->sppmi->PixelSampler);

	hitPoints = new std::vector<HitPoint>(eyeSampler->GetTotalSamplePos());
	LOG(LUX_DEBUG, LUX_NOERROR) << "Hit points count: " << hitPoints->size();

	// Initialize hit points field
	const u_int lightGroupsNumber = scene->lightGroups.size();

	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		hp->lightGroupData.resize(lightGroupsNumber);
		hp->photonCount = 0;
		hp->accumPhotonCount = 0;

		for(u_int j = 0; j < lightGroupsNumber; j++) {
			hp->lightGroupData[j].reflectedFlux = XYZColor();

			// hp->accumPhotonRadius2 is initialized in the Init() method
			hp->lightGroupData[j].accumReflectedFlux = XYZColor();
			hp->lightGroupData[j].accumRadiance = XYZColor();
			// Debug code
			//hp->lightGroupData[j].radianceSSE = 0.f;
		}
	}
}

HitPoints::~HitPoints() {
	delete lookUpAccel;
	delete hitPoints;
	delete eyeSampler;
}

const double HitPoints::GetPhotonHitEfficency() {
	u_int surfaceHitPointsCount = 0;
	u_int hitPointsUpdatedCount = 0;
	for (u_int i = 0; i < GetSize(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];
		HitPointEyePass *hpep = &hp->eyePass;

		if (hpep->type == SURFACE) {
			++surfaceHitPointsCount;

			if (hp->accumPhotonCount > 0)
				++hitPointsUpdatedCount;
		}
	}

	return 100.0 * hitPointsUpdatedCount / surfaceHitPointsCount;
}

void HitPoints::Init() {
	// Not using UpdateBBox() because hp->accumPhotonRadius2 is not yet set
	BBox hpBBox = BBox();
	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];
		HitPointEyePass *hpep = &hp->eyePass;

		if (hpep->type == SURFACE)
			hpBBox = Union(hpBBox, hpep->position);
	}

	// Calculate initial radius
	Vector ssize = hpBBox.pMax - hpBBox.pMin;
	initialPhotonRadius = renderer->sppmi->photonStartRadiusScale *
		((ssize.x + ssize.y + ssize.z) / 3.f) / sqrtf(eyeSampler->GetTotalSamplePos()) * 2.f;
	const float photonRadius2 = initialPhotonRadius * initialPhotonRadius;

	// Expand the bounding box by used radius
	hpBBox.Expand(initialPhotonRadius);
	// Update hit points information
	hitPointBBox = hpBBox;
	maxHitPointRadius2 = photonRadius2;

	LOG(LUX_DEBUG, LUX_NOERROR) << "Hit points bounding box: " << hitPointBBox;
	LOG(LUX_DEBUG, LUX_NOERROR) << "Hit points max. radius: " << sqrtf(maxHitPointRadius2);

	// Initialize hit points field
	for (u_int i = 0; i < (*hitPoints).size(); ++i) {
		HitPoint *hp = &(*hitPoints)[i];

		hp->accumPhotonRadius2 = photonRadius2;
	}

	// Allocate hit points lookup accelerator
	switch (renderer->sppmi->lookupAccelType) {
		case HASH_GRID:
			lookUpAccel = new HashGrid(this);
			break;
		case KD_TREE:
			lookUpAccel = new KdTree(this);
			break;
		case HYBRID_HASH_GRID:
			lookUpAccel = new HybridHashGrid(this);
			break;
		default:
			assert (false);
	}
}

void HitPoints::AccumulateFlux(const u_int index, const u_int count) {
	const unsigned int workSize = hitPoints->size() / count;
	const unsigned int first = workSize * index;
	const unsigned int last = (index == count - 1) ? hitPoints->size() : (first + workSize);
	assert (first >= 0);
	assert (last <= hitPoints->size());

	LOG(LUX_DEBUG, LUX_NOERROR) << "Accumulate photons flux: " << first << " to " << last - 1;

	const u_int lightGroupsNumber = renderer->scene->lightGroups.size();

	for (u_int i = first; i < last; ++i) {
		HitPoint *hp = &(*hitPoints)[i];
		HitPointEyePass *hpep = &hp->eyePass;

		if(hpep->type == SURFACE) {
			if (hp->accumPhotonCount > 0) {
				u_int k = renderer->sppmi->photonStartK;
				if(k > 0 && hp->photonCount == 0)
				{
					// This heuristic is triggered by hitpoint on the first pass
					// which gather photons.

					// If the pass gather more than k photons, and with the
					// assumption that photons are uniformly spread on the
					// hitpoint, we reduce the search radius.

					if(hp->accumPhotonCount > k)
					{
						// We now suppose that we only gather k photons, and
						// reduce the radius accordingly.
						// Note: the flux is already normalised, so it does
						// not depends of the radius, no need to change it.
						hp->accumPhotonRadius2 *= ((float) k) / ((float) hp->accumPhotonCount);
						hp->accumPhotonCount = k;
					}
				}
				const unsigned long long pcount = hp->photonCount + hp->accumPhotonCount;

				// Compute g and do radius reduction
				const double alpha = renderer->sppmi->photonAlpha;
				const float g = alpha * pcount / (hp->photonCount * alpha + hp->accumPhotonCount);

				// Radius reduction
				hp->accumPhotonRadius2 *= g;

				// Update light group flux
				for (u_int j = 0; j < lightGroupsNumber; ++j) {
					// NOTE: the stored flux is already normalized, so no need to multiply by g
					hp->lightGroupData[j].reflectedFlux += hp->lightGroupData[j].accumReflectedFlux;

					hp->lightGroupData[j].accumReflectedFlux = 0.f;
				}

				hp->photonCount = pcount;
				hp->accumPhotonCount = 0;
			}
		} else
			assert(hpep->type == CONSTANT_COLOR);
	}
}

void HitPoints::SetHitPoints(RandomGenerator *rng, const u_int index, const u_int count, MemoryArena &arena) {
	const unsigned int workSize = hitPoints->size() / count;
	const unsigned int first = workSize * index;
	const unsigned int last = (index == count - 1) ? hitPoints->size() : (first + workSize);

	arena.FreeAll();

	assert (first >= 0);
	assert (last <= hitPoints->size());

	LOG(LUX_DEBUG, LUX_NOERROR) << "Building hit points: " << first << " to " << last - 1;

	Scene &scene(*renderer->scene);

	Sample sample;
	sample.contribBuffer = NULL;
	sample.camera = scene.camera->Clone();
	sample.realTime = 0.f;
	sample.rng = rng;
	vector<u_int> structure;
	structure.push_back(1);	// volume scattering
	structure.push_back(2);	// bsdf sampling direction
	structure.push_back(1);	// bsdf sampling component
	sample.AddxD(structure, renderer->sppmi->maxEyePathDepth + 1);
	scene.volumeIntegrator->RequestSamples(&sample, scene);
	eyeSampler->InitSample(&sample);

	for (u_int i = first; i < last; ++i) {
		static_cast<HaltonEyeSampler::HaltonEyeSamplerData *>(sample.samplerData)->index = i; //FIXME sampler data shouldn't be accessed directly
		static_cast<HaltonEyeSampler::HaltonEyeSamplerData *>(sample.samplerData)->pathCount = currentPass; //FIXME sampler data shouldn't be accessed directly
		sample.wavelengths = wavelengthSample;
		sample.time = timeSample;
		sample.swl.Sample(sample.wavelengths);
		sample.realTime = sample.camera->GetTime(sample.time);
		sample.camera->SampleMotion(sample.realTime);
		// Generate the sample values
		eyeSampler->GetNextSample(&sample);
		HitPoint *hp = &(*hitPoints)[i];

		// Trace the eye path
		TraceEyePath(hp, sample, arena);

		sample.arena.FreeAll();
	}
	eyeSampler->FreeSample(&sample);
	//delete sample.camera; //FIXME deleting the camera clone would delete the film!
}

void HitPoints::TraceEyePath(HitPoint *hp, const Sample &sample, MemoryArena &hp_arena)
{
	HitPointEyePass *hpep = &hp->eyePass;

	Scene &scene(*renderer->scene);
	const bool includeEnvironment = renderer->sppmi->includeEnvironment;
	const u_int maxDepth = renderer->sppmi->maxEyePathDepth;

	//--------------------------------------------------------------------------
	// Following code is, given or taken, a copy of path integrator Li() method
	//--------------------------------------------------------------------------

	// Declare common path integration variables
	const SpectrumWavelengths &sw(sample.swl);
	Ray ray;
	const float rayWeight = sample.camera->GenerateRay(scene, sample, &ray);

	const float nLights = scene.lights.size();
	SWCSpectrum pathThroughput(1.f);
	const u_int lightGroupCount = scene.lightGroups.size();
	vector<SWCSpectrum> L(lightGroupCount, 0.f);
	bool scattered = false;
	hpep->alpha = 1.f;
	hpep->distance = INFINITY;
	u_int vertexIndex = 0;
	const Volume *volume = NULL;

	for (u_int pathLength = 0; ; ++pathLength) {
		const SWCSpectrum prevThroughput(pathThroughput);

		float *data = eyeSampler->GetLazyValues(sample, 0, pathLength);

		// Find next vertex of path
		Intersection isect;
		BSDF *bsdf;
		float spdf;
		if (!scene.Intersect(sample, volume, scattered, ray, data[0],
			&isect, &bsdf, &spdf, NULL, &pathThroughput)) {
			pathThroughput /= spdf;
			// Dade - now I know ray.maxt and I can call volumeIntegrator
			SWCSpectrum Lv;
			u_int g = scene.volumeIntegrator->Li(scene, ray, sample,
				&Lv, &hpep->alpha);
			if (!Lv.Black()) {
				Lv *= prevThroughput;
				L[g] += Lv;
			}

			// Stop path sampling since no intersection was found
			// Possibly add horizon in render & reflections
			if (includeEnvironment || (vertexIndex > 0)) {
				BSDF *ibsdf;
				for (u_int i = 0; i < nLights; ++i) {
					SWCSpectrum Le(pathThroughput);
					if (scene.lights[i]->Le(scene, sample,
						ray, &ibsdf, NULL, NULL, &Le))
						L[scene.lights[i]->group] += Le;
				}
			}

			// Set alpha channel
			if (vertexIndex == 0)
				hpep->alpha = 0.f;

			hpep->type = CONSTANT_COLOR;
			break;
		}
		scattered = bsdf->dgShading.scattered;
		pathThroughput /= spdf;
		if (vertexIndex == 0)
			hpep->distance = ray.maxt * ray.d.Length();

		SWCSpectrum Lv;
		const u_int g = scene.volumeIntegrator->Li(scene, ray, sample,
			&Lv, &hpep->alpha);
		if (!Lv.Black()) {
			Lv *= prevThroughput;
			L[g] += Lv;
		}

		// Possibly add emitted light at path vertex
		Vector wo(-ray.d);
		if (isect.arealight) {
			BSDF *ibsdf;
			SWCSpectrum Le(isect.Le(sample, ray, &ibsdf, NULL, NULL));
			if (!Le.Black()) {
				Le *= pathThroughput;
				L[isect.arealight->group] += Le;
			}
		}

		const Point &p = bsdf->dgShading.p;

		// Sample BSDF to get new path direction
		Vector wi;
		float pdf;
		BxDFType flags;
		SWCSpectrum f;
		if (pathLength == maxDepth || !bsdf->SampleF(sw, wo, &wi,
			data[1], data[2], data[3], &f, &pdf, BSDF_ALL, &flags,
			NULL, true)) {
			// Make it an approximate hitpoint
			hpep->type = SURFACE;
			hpep->pathThroughput = pathThroughput * rayWeight;
			hpep->position = p;
			hpep->wo = wo;

			// TODO: find a way to copy the generated bsdf to a new one
			hpep->bsdf = isect.GetBSDF(hp_arena, sw, ray);
			break;
		}

		if ((flags & BSDF_DIFFUSE) || ((flags & BSDF_GLOSSY) && (pdf < renderer->sppmi->GlossyThreshold))) {
			// It is a valid hit point
			hpep->type = SURFACE;
			hpep->pathThroughput = pathThroughput * rayWeight;
			hpep->position = p;
			hpep->wo = wo;

			// TODO: find a way to copy the generated bsdf to a new one
			hpep->bsdf = isect.GetBSDF(hp_arena, sw, ray);
			break;
		}

		if (flags != (BSDF_TRANSMISSION | BSDF_SPECULAR) ||
			!(bsdf->Pdf(sw, wi, wo, BxDFType(BSDF_TRANSMISSION | BSDF_SPECULAR)) > 0.f))
			++vertexIndex;

		pathThroughput *= f;
		if (pathThroughput.Black()) {
			hpep->type = CONSTANT_COLOR;
			break;
		}

		ray = Ray(p, wi);
		ray.time = sample.realTime;
		volume = bsdf->GetVolume(wi);
	}
	for(unsigned int j = 0; j < lightGroupCount; ++j)
		hp->lightGroupData[j].accumRadiance += XYZColor(sw, L[j] * rayWeight);
}

void HitPoints::UpdatePointsInformation() {
	// Calculate hit points bounding box
	BBox bbox;
	float maxr2, minr2, meanr2;
	u_int minp, maxp, meanp;
	u_int surfaceHits, constantHits, zeroHits;

	surfaceHits = constantHits = zeroHits = 0;

	assert((*hitPoints).size() > 0);
	HitPoint *hp = &(*hitPoints)[0];
	HitPointEyePass *hpep = &hp->eyePass;

	maxr2 = minr2 = meanr2 = hp->accumPhotonRadius2;
	minp = maxp = meanp = hp->photonCount;

	for (u_int i = 1; i < (*hitPoints).size(); ++i) {
		hp = &(*hitPoints)[i];
		hpep = &hp->eyePass;

		if (hpep->type == SURFACE) {
			if(hp->photonCount == 0)
				++zeroHits;

			bbox = Union(bbox, hpep->position);

			maxr2 = max<float>(maxr2, hp->accumPhotonRadius2);
			minr2 = min<float>(minr2, hp->accumPhotonRadius2);
			meanr2 += hp->accumPhotonRadius2;

			maxp = max<float>(maxp, hp->photonCount);
			minp = min<float>(minp, hp->photonCount);
			meanp += hp->photonCount;

			++surfaceHits;
		}
		else
			++constantHits;
	}

	LOG(LUX_DEBUG, LUX_NOERROR) << "Hit points stats:";
	LOG(LUX_DEBUG, LUX_NOERROR) << "\tbounding box: " << bbox;
	LOG(LUX_DEBUG, LUX_NOERROR) << "\tmin/max radius: " << sqrtf(minr2) << "/" << sqrtf(maxr2);
	LOG(LUX_DEBUG, LUX_NOERROR) << "\tmin/max photonCount: " << minp << "/" << maxp;
	LOG(LUX_DEBUG, LUX_NOERROR) << "\tmean radius/photonCount: " << sqrtf(meanr2 / surfaceHits) << "/" << meanp / surfaceHits;
	LOG(LUX_DEBUG, LUX_NOERROR) << "\tconstant/zero hits: " << constantHits << "/" << zeroHits;

	hitPointBBox = bbox;
	maxHitPointRadius2 = maxr2;
}

void HitPoints::UpdateFilm(const unsigned long long totalPhotons, const float fluxScale) {
	Scene &scene(*renderer->scene);
	const u_int bufferId = renderer->sppmi->bufferId;
	int xPos, yPos;
	const u_int lightGroupsNumber = scene.lightGroups.size();
	Film &film(*scene.camera->film);

	/*if (renderer->sppmi->dbg_enableradiusdraw) {
		// Draw the radius of hit points
		XYZColor c;
		for (u_int i = 0; i < GetSize(); ++i) {
			HitPoint *hp = GetHitPoint(i);
			pixelSampler->GetNextPixel(&xPos, &yPos, i);

			for(u_int j = 0; j < lightGroupsNumber; j++) {
				if (hp->lightGroupData[j].surfaceHitsCount > 0)
					c.c[1] = sqrtf(hp->accumPhotonRadius2) / initialPhotonRadius;
				else
					c.c[1] = 0;

				Contribution contrib(xPos, yPos, c, hp->eyeAlpha,
						hp->eyeDistance, 0.f, bufferId, j);
				film.SetSample(&contrib);
			}
		}
	} else if (renderer->sppmi->dbg_enablemsedraw) {
		// Draw the radius of hit points
		XYZColor c;
		for (u_int i = 0; i < GetSize(); ++i) {
			HitPoint *hp = GetHitPoint(i);
			pixelSampler->GetNextPixel(&xPos, &yPos, i);

			for(u_int j = 0; j < lightGroupsNumber; j++) {
				// Radiance Mean Square Error
				c.c[1] = hp->lightGroupData[j].radianceSSE /
						(hp->lightGroupData[j].constantHitsCount + hp->lightGroupData[j].surfaceHitsCount);

				Contribution contrib(xPos, yPos, c, hp->eyeAlpha,
						hp->eyeDistance, 0.f, bufferId, j);
				film.SetSample(&contrib);
			}
		}
	} else {*/
		// Just normal rendering
		
		const double k = fluxScale / totalPhotons;

		for (u_int i = 0; i < GetSize(); ++i) {
			HitPoint *hp = &(*hitPoints)[i];
			HitPointEyePass *hpep = &hp->eyePass;
			static_cast<HaltonEyeSampler *>(eyeSampler)->pixelSampler->GetNextPixel(&xPos, &yPos, i); //FIXME shouldn't access directly sampler data

			// Update radiance
			for(u_int j = 0; j < lightGroupsNumber; ++j) {

				const XYZColor newRadiance = hp->lightGroupData[j].accumRadiance / currentPass +
						hp->lightGroupData[j].reflectedFlux * k;

				Contribution contrib(xPos, yPos, newRadiance, hpep->alpha,
						hpep->distance, 0.f, bufferId, j);
				film.SetSample(&contrib);
					// Debug code
					/*// Update Sum Square Error statistic
					if (hitCount > 1) {
						const float v = newRadiance.Y() - hp->lightGroupData[j].radiance.Y();
						hp->lightGroupData[j].radianceSSE += v * v;
					}*/
			}
		}
	//}

	scene.camera->film->CheckWriteOuputInterval();

	int passCount = luxStatistics("pass");
	int hltSpp = scene.camera->film->haltSamplesPerPixel;
	if(hltSpp > 0){
		if(passCount == hltSpp){
			renderer->Terminate();
		}
	}
	
	int secsElapsed = luxStatistics("secElapsed");
	int hltTime = scene.camera->film->haltTime;
	if(hltTime > 0){
		if(secsElapsed > hltTime){
			renderer->Terminate();
		}
	}
}

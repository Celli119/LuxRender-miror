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

// TODO: add scaling of output image samples

// metrosampler.cpp*
#include "metrosampler.h"
#include "scene.h"
#include "dynload.h"
#include "timer.h"

using namespace lux;

#define SAMPLE_FLOATS 6
static const u_int rngN = 8191;
static const u_int rngA = 884;

MetropolisSampler::MetropolisData::MetropolisData(const MetropolisSampler &sampler) :
	consecRejects(0), stamp(0), currentStamp(0), weight(0.f),
	LY(0.f), alpha(0.f), totalLY(0.f), sampleCount(0.f),
	noiseAwareMapVersion(0), userSamplingMapVersion(0),
	large(true), cooldown(sampler.cooldownTime > 0)
{
	u_int i;
	// Compute number of non lazy samples
	normalSamples = SAMPLE_FLOATS;
	for (i = 0; i < sampler.n1D.size(); ++i)
		normalSamples += sampler.n1D[i];
	for (i = 0; i < sampler.n2D.size(); ++i)
		normalSamples += 2 * sampler.n2D[i];

	// Compute number of lazy samples and initialize management data
	totalSamples = normalSamples;
	offset = new u_int[sampler.nxD.size()];
	totalTimes = 0;
	timeOffset = new u_int[sampler.nxD.size()];
	for (i = 0; i < sampler.nxD.size(); ++i) {
		timeOffset[i] = totalTimes;
		offset[i] = totalSamples;
		totalTimes += sampler.nxD[i];
		totalSamples += sampler.dxD[i] * sampler.nxD[i];
	}

	// Allocate sample image to hold the current reference
	sampleImage = AllocAligned<float>(totalSamples);
	currentImage = AllocAligned<float>(totalSamples);
	timeImage = AllocAligned<int>(totalTimes);
	currentTimeImage = AllocAligned<int>(totalTimes);

	// Compute best offset between sample vectors in the rng
	// TODO use the smallest gcf of totalSamples and rngN that is greater
	// than totalSamples or equal
	rngOffset = totalSamples;
	if (rngOffset >= rngN)
		rngOffset = rngOffset % (rngN - 1) + 1;
	// Current base index, minus offset to compensate for the advance done
	// in GetNextSample
	rngBase = rngN - rngOffset;
	// Allocate memory for the Cranley-Paterson rotation vector
	rngRotation = AllocAligned<float>(totalSamples);
}

MetropolisSampler::MetropolisData::~MetropolisData()
{
	FreeAligned(rngRotation);
	FreeAligned(currentTimeImage);
	FreeAligned(timeImage);
	FreeAligned(currentImage);
	FreeAligned(sampleImage);
	delete[] timeOffset;
	delete[] offset;
}

// mutate a value in the range [0-1]
static float mutate(const float x, const float randomValue)
{
	static const float s1 = 1.f / 512.f, s2 = 1.f / 16.f;
	const float dx = s1 / (s1 / s2 + fabsf(2.f * randomValue - 1.f)) -
		s1 / (s1 / s2 + 1.f);
	if (randomValue < 0.5f) {
		float x1 = x + dx;
		return (x1 < 1.f) ? x1 : x1 - 1.f;
	} else {
		float x1 = x - dx;
		return (x1 < 0.f) ? x1 + 1.f : x1;
	}
}

// mutate a value in the range [min-max]
static float mutateScaled(const float x, const float randomValue, const float mini, const float maxi, const float range)
{
	static const float s1 = 32.f;
	const float dx = range / (s1 / (1.f + s1) + (s1 * s1) / (1.f + s1) *
		fabsf(2.f * randomValue - 1.f)) - range / s1;
	if (randomValue < 0.5f) {
		float x1 = x + dx;
		return (x1 < maxi) ? x1 : x1 - maxi + mini;
	} else {
		float x1 = x - dx;
		return (x1 < mini) ? x1 - mini + maxi : x1;
	}
}

static float fracf(const float &v) {
	const long i = static_cast<long>(v);
	return v - i;
}

#define rngGet(__pos) (fracf(rngSamples[(data->rngBase + (__pos)) % rngN] + data->rngRotation[(__pos)]))
#define rngGet2(__pos,__off) (fracf(rngSamples[(data->rngBase + (__pos) + (__off)) % rngN] + data->rngRotation[(__pos)]))


// Metropolis method definitions
MetropolisSampler::MetropolisSampler(int xStart, int xEnd, int yStart, int yEnd,
	u_int maxRej, float largeProb, float rng, bool useV, bool useC, bool useNoise) :
	Sampler(xStart, xEnd, yStart, yEnd, 1, useNoise), maxRejects(maxRej),
	pLarge(largeProb), range(rng), useVariance(useV) {
	// Allocate and compute all values of the rng
	rngSamples = AllocAligned<float>(rngN);
	rngSamples[0] = 0.f;
	rngSamples[1] = 1.f / rngN;
	u_int rngI = 1;
	for (u_int i = 2; i < rngN; ++i) {
		rngI = (rngI * rngA) % rngN;
		rngSamples[i] = static_cast<float>(rngI) / rngN;
	}
	RandomGenerator rndg(1);
	Shuffle(rndg, rngSamples, rngN, 1);
	// Cooldown samples are computed to minimize start-up bias
	if (useC) {
		float pLarge_factor = (pLarge < 0.5) ? 1.5 * fabs(pLarge - 0.5) : 0;
		cooldownTime = (xPixelEnd - xPixelStart) * (yPixelEnd - yPixelStart) * pLarge_factor;
		if(cooldownTime > 0) LOG(LUX_INFO, LUX_NOERROR) << "Metropolis cooldown during first " << cooldownTime << " samples";
	} else
		cooldownTime = 0;

	AddStringConstant(*this, "name", "Name of current sampler", "metropolis");
	AddIntAttribute(*this, "maxRejects", "Metropolis max. rejections", &MetropolisSampler::GetMaxRejects);
	AddFloatAttribute(*this, "pLarge", "Metropolis probability of a large mutation", &MetropolisSampler::pLarge);
	AddFloatAttribute(*this, "range", "Metropolis image mutation range", &MetropolisSampler::range);
}

MetropolisSampler::~MetropolisSampler() {
	FreeAligned(rngSamples);
}

// interface for new ray/samples from scene
bool MetropolisSampler::GetNextSample(Sample *sample)
{
	// Stop right away if the rendering limit has been reached
	// as it seems no major artifacts from QMC are observed
	if (film->enoughSamplesPerPixel)
		return false;
	MetropolisData *data = (MetropolisData *)(sample->samplerData);

	// Advance to the next vector in the QMC sequence and stay inside the
	// array bounds
	data->rngBase += data->rngOffset;
	if (data->rngBase >= rngN)
		data->rngBase -= rngN;
	// If all possible combinations have been used,
	// change the Cranley-Paterson rotation vector
	// This is also responsible for first time initialization of the vector
	if (data->rngBase == 0) {
		for (u_int i = 0; i < data->totalSamples; ++i)
			data->rngRotation[i] = sample->rng->floatValue();
	}
	if (data->large) {
		// *** large mutation ***

		if (useNoiseAware || film->HasUserSamplingMap()) {
			// Noise-aware and/or User Sampling support

			bool newSamplingMap = false;
			// Check if there is a new version of the noise map and/or user-sampling map
			if (useNoiseAware) {
				if (film->HasUserSamplingMap()) {
					newSamplingMap = film->GetSamplingMap(data->noiseAwareMapVersion, data->userSamplingMapVersion,
							data->samplingMap, data->samplingDistribution2D);
				} else
					newSamplingMap = film->GetNoiseAwareMap(data->noiseAwareMapVersion,
							data->samplingMap, data->samplingDistribution2D);
			} else {
				if (film->HasUserSamplingMap()) {
					newSamplingMap = film->GetUserSamplingMap(data->userSamplingMapVersion,
							data->samplingMap, data->samplingDistribution2D);
				} else {
					// This should never happen
					LOG(LUX_ERROR, LUX_SYSTEM) << "Internal error in MetropolisSampler::GetNextSample()";
				}
			}

			if (newSamplingMap) {
				// There is a new version so reset some data
				data->totalLY = 0.0;
				data->sampleCount = 0.f;
				data->consecRejects = 0;
				data->LY = 0.f;
				data->weight = 0.f;
			}
		}

		if ((data->noiseAwareMapVersion > 0) || (data->userSamplingMapVersion > 0)) {
			float uv[2], pdf;
			data->samplingDistribution2D->SampleContinuous(rngGet(0), rngGet(1), uv, &pdf);
			data->currentImage[0] = uv[0] * (xPixelEnd - xPixelStart) + xPixelStart;
			data->currentImage[1] = uv[1] * (yPixelEnd - yPixelStart) + yPixelStart;
		} else {
			data->currentImage[0] = rngGet(0) * (xPixelEnd - xPixelStart) + xPixelStart;
			data->currentImage[1] = rngGet(1) * (yPixelEnd - yPixelStart) + yPixelStart;
		}

		sample->imageX = data->currentImage[0];
		sample->imageY = data->currentImage[1];

		// Initialize all non lazy samples
		for (u_int i = 2; i < data->normalSamples; ++i)
			data->currentImage[i] = rngGet(i);
		sample->lensU = data->currentImage[2];
		sample->lensV = data->currentImage[3];
		sample->time = data->currentImage[4];
		sample->wavelengths = data->currentImage[5];
		// Reset number of mutations for lazy samples
		for (u_int i = 0; i < data->totalTimes; ++i)
			data->currentTimeImage[i] = -1;
		// Reset number of mutations for whole sample
		data->currentStamp = 0;
	} else {
		// *** small mutation ***
		// Mutation of non lazy samples
		sample->imageX = data->currentImage[0] =
			mutateScaled(data->sampleImage[0], rngGet(0),
			xPixelStart, xPixelEnd, range);
		sample->imageY = data->currentImage[1] =
			mutateScaled(data->sampleImage[1], rngGet(1),
			yPixelStart, yPixelEnd, range);

		sample->lensU = data->currentImage[2] =
			mutateScaled(data->sampleImage[2], rngGet(2),
			0.f, 1.f, .5f);
		sample->lensV = data->currentImage[3] =
			mutateScaled(data->sampleImage[3], rngGet(3),
			0.f, 1.f, .5f);
		sample->time = data->currentImage[4] =
			mutateScaled(data->sampleImage[4], rngGet(4),
			0.f, 1.f, .5f);
		sample->wavelengths = data->currentImage[5] =
			mutateScaled(data->sampleImage[5], rngGet(5),
			0.f, 1.f, .5f);
		for (u_int i = SAMPLE_FLOATS; i < data->normalSamples; ++i)
			data->currentImage[i] =
				mutate(data->sampleImage[i], rngGet(i));
		for (u_int i = 0; i < data->totalTimes; ++i)
			data->currentTimeImage[i] = data->timeImage[i];
		// Increase reference mutation count
		data->currentStamp = data->stamp + 1;
	}
	return true;
}

float MetropolisSampler::GetOneD(const Sample &sample, u_int num, u_int pos)
{
	MetropolisData *data = (MetropolisData *)(sample.samplerData);
	u_int offset = SAMPLE_FLOATS;
	for (u_int i = 0; i < num; ++i)
		offset += n1D[i];
	return data->currentImage[offset + pos];
}

void MetropolisSampler::GetTwoD(const Sample &sample, u_int num, u_int pos, float u[2])
{
	MetropolisData *data = (MetropolisData *)(sample.samplerData);
	u_int offset = SAMPLE_FLOATS;
	for (u_int i = 0; i < n1D.size(); ++i)
		offset += n1D[i];
	for (u_int i = 0; i < num; ++i)
		offset += n2D[i] * 2;
	u[0] = data->currentImage[offset + pos];
	u[1] = data->currentImage[offset + pos + 1];
}

float *MetropolisSampler::GetLazyValues(const Sample &sample, u_int num, u_int pos)
{
	MetropolisData *data = (MetropolisData *)(sample.samplerData);
	// Get size and position of current lazy values node
	const u_int size = dxD[num];
	const u_int offset = data->offset[num] + pos * size;
	const u_int timeOffset = data->timeOffset[num] + pos;
	// If we are at the target, don't do anything
	int &currentTime(data->currentTimeImage[timeOffset]);
	if (data->large) {
		for (u_int i = offset; i < offset + size; ++i)
			data->currentImage[i] = rngGet(i);
		currentTime = 0;
	} else {
		// Get the reference number of mutations
		const int stampLimit = data->currentStamp;
		if (currentTime != stampLimit) {
			int &time(data->timeImage[timeOffset]);
			// If the node has not yet been initialized, do it now
			// otherwise get the last known value from the sample image
			if (time == -1) {
				const u_int roffs = data->rngOffset * static_cast<u_int>(stampLimit);
				for (u_int i = offset; i < offset + size; ++i)
					data->sampleImage[i] = rngGet2(i, roffs);
				time = 0;
			}
			for (u_int i = offset; i < offset + size; ++i)
				data->currentImage[i] = data->sampleImage[i];
			currentTime = time;
		}
		// Mutate as needed
		for (; currentTime < stampLimit; ++currentTime) {
			const u_int roffs = data->rngOffset * static_cast<u_int>(stampLimit - currentTime + 1);
			for (u_int i = offset; i < offset + size; ++i)
				data->currentImage[i] = mutate(data->currentImage[i], rngGet2(i, roffs));
		}
	}
	return data->currentImage + offset;
}

void MetropolisSampler::AddSample(const Sample &sample)
{
	MetropolisData *data = (MetropolisData *)(sample.samplerData);
	vector<Contribution> &newContributions(sample.contributions);
	float newLY = 0.f;
	if ((data->noiseAwareMapVersion > 0) || (data->userSamplingMapVersion > 0)) {
		for(u_int i = 0; i < newContributions.size(); ++i) {
			const float ly = newContributions[i].color.Y();

			if (ly > 0.f && !isinf(ly)) {
				const u_int xPixelCount = film->GetXPixelCount();
				const u_int yPixelCount = film->GetYPixelCount();
				const u_int x = min(xPixelCount - 1, Floor2UInt(data->currentImage[0] - xPixelStart));
				const u_int y = min(yPixelCount - 1, Floor2UInt(data->currentImage[1] - yPixelStart));
				const int index = x + y * xPixelCount;

				const float samplingMapValue = data->samplingMap[index];

				if (useVariance && newContributions[i].variance > 0.f)
					newLY += ly * newContributions[i].variance * samplingMapValue;
				else
					newLY += ly * samplingMapValue;
			} else
				newContributions[i].color = XYZColor(0.);
		}
	} else {
		for(u_int i = 0; i < newContributions.size(); ++i) {
			const float ly = newContributions[i].color.Y();
			if (ly > 0.f && !isinf(ly)) {
				if (useVariance && newContributions[i].variance > 0.f)
					newLY += ly * newContributions[i].variance;
				else
					newLY += ly;
			} else
				newContributions[i].color = XYZColor(0.f);
		}
	}
		
	if (data->large) {
		data->totalLY += newLY;
		++(data->sampleCount);
	}

	const float meanIntensity = data->totalLY > 0. ? static_cast<float>(data->totalLY / data->sampleCount) : 1.f;

	sample.contribBuffer->AddSampleCount(1.f);

	// Define the probability of large mutations. It is 50% if we are still
	// inside the cooldown phase.
	const float largeMutationProb = (data->cooldown) ? .5f : pLarge;

	// calculate accept probability from old and new image sample
	float accProb;
	if (data->LY > 0.f && data->consecRejects < maxRejects)
		accProb = min(1.f, newLY / data->LY);
	else
		accProb = 1.f;
	const float newWeight = accProb + (data->large ? 1.f : 0.f);
	data->weight += 1.f - accProb;
	// try or force accepting of the new sample
	if (accProb == 1.f || sample.rng->floatValue() < accProb) {
		// Add accumulated contribution of previous reference sample
		const float norm = data->weight / (data->LY / meanIntensity + largeMutationProb);
		if (norm > 0.f) {
			for(u_int i = 0; i < data->oldContributions.size(); ++i)
				sample.contribBuffer->Add(data->oldContributions[i], norm);
		}
		// Save new contributions for reference
		data->weight = newWeight;
		data->LY = newLY;
		data->oldContributions = newContributions;
		swap(data->currentImage, data->sampleImage);
		swap(data->currentTimeImage, data->timeImage);
		data->stamp = data->currentStamp;

		data->consecRejects = 0;
	} else {
		// Add contribution of new sample before rejecting it
		const float norm = newWeight / (newLY / meanIntensity + largeMutationProb);
		if (norm > 0.f) {
			for(u_int i = 0; i < newContributions.size(); ++i)
				sample.contribBuffer->Add(newContributions[i], norm);
		}
		// Restart from previous reference
		data->currentStamp = data->stamp;

		++(data->consecRejects);
	}
	newContributions.clear();

	const float mutationSelector = sample.rng->floatValue();
	if (data->cooldown) {
		if (data->sampleCount >= cooldownTime) {
			data->cooldown = false;
			LOG(LUX_DEBUG, LUX_NOERROR) << "Cooldown process has now ended";
			data->large = (mutationSelector < pLarge);
		} else
			data->large = (mutationSelector < .5f);
	} else
		data->large = (mutationSelector < pLarge);
}

Sampler* MetropolisSampler::CreateSampler(const ParamSet &params, Film *film)
{
	int xStart, xEnd, yStart, yEnd;
	film->GetSampleExtent(&xStart, &xEnd, &yStart, &yEnd);
	int maxConsecRejects = params.FindOneInt("maxconsecrejects", 512);	// number of consecutive rejects before a next mutation is forced
	float largeMutationProb = params.FindOneFloat("largemutationprob", 0.4f);	// probability of generating a large sample mutation
	bool useVariance = params.FindOneBool("usevariance", false);
	bool useCooldown = params.FindOneBool("usecooldown", true);
	bool useNoiseAware = params.FindOneBool("noiseaware", false);
	float range = params.FindOneFloat("mutationrange", (xEnd - xStart + yEnd - yStart) / 32.f);	// maximum distance in pixel for a small mutation

	if (useNoiseAware) {
		// Enable Film noise-aware map generation
		film->EnableNoiseAwareMap();
	}

	return new MetropolisSampler(xStart, xEnd, yStart, yEnd, max(maxConsecRejects, 0),
		largeMutationProb, range, useVariance, useCooldown, useNoiseAware);
}

static DynamicLoader::RegisterSampler<MetropolisSampler> r("metropolis");

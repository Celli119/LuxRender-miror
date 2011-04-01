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

#ifndef _RENDERINGHINTS_H
#define	_RENDERINGHINTS_H

#include "lux.h"

namespace lux {

//******************************************************************************
// Strategies
//******************************************************************************

class Strategy {
public:
	Strategy() { }
	virtual ~Strategy() { }
	virtual void InitParam(const ParamSet &params) = 0;
	virtual void Init(const Scene *scene) = 0;
};

//------------------------------------------------------------------------------
// Light Sampling Strategies
//------------------------------------------------------------------------------

class LightsSamplingStrategy : public Strategy {
public:
	enum LightStrategyType {
		SAMPLE_ALL_UNIFORM, SAMPLE_ONE_UNIFORM,
		SAMPLE_AUTOMATIC, SAMPLE_ONE_IMPORTANCE,
		SAMPLE_ONE_POWER_IMPORTANCE, SAMPLE_ALL_POWER_IMPORTANCE,
		SAMPLE_ONE_LOG_POWER_IMPORTANCE
	};

	LightsSamplingStrategy() : Strategy() { }
	virtual ~LightsSamplingStrategy() { }
	virtual void InitParam(const ParamSet &params) { }
	virtual void Init(const Scene *scene) { }

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const = 0;
	virtual u_int RequestSamplesCount(const Scene *scene) const = 0;

	// Note: results are added to L
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const = 0;

	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const = 0;
};

class LSSAllUniform : public LightsSamplingStrategy {
public:
	LSSAllUniform() : LightsSamplingStrategy() { }
	virtual ~LSSAllUniform() { }
	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const {
		return 6;
	}
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;

	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const;
};

class LSSOneUniform : public LightsSamplingStrategy {
public:
	LSSOneUniform() : LightsSamplingStrategy() { }
	virtual ~LSSOneUniform() { }
	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 6; }
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;

	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const;
};

class LSSAuto : public LightsSamplingStrategy {
public:
	LSSAuto() : LightsSamplingStrategy(), strategy(NULL) { }
	virtual ~LSSAuto() { delete strategy; }
	virtual void Init(const Scene *scene);

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const {
		strategy->RequestSamples(scene, structure);
	}
	virtual u_int RequestSamplesCount(const Scene *scene) const {
		return strategy->RequestSamplesCount(scene);
	}

	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const {
		return strategy->SampleLights(tspack, scene, shadowRayCount,
			p, n, wo, bsdf, sample, sampleData, scale, L);
	}

	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const {
		return strategy->SampleLights(tspack, scene, shadowRayCount,
			p, n, wo, bsdf, sample, sampleData, scale, L, rayDepth, from_IsSup, to_IsSup, path_type);
	}
private:
	LightsSamplingStrategy *strategy;
};

class LSSOneImportance : public LightsSamplingStrategy {
public:
	LSSOneImportance() : LightsSamplingStrategy(), lightDistribution(NULL) { }
	virtual ~LSSOneImportance();
	virtual void Init(const Scene *scene);

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 6; }
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;
	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const;

private:
	Distribution1D *lightDistribution;
};

class LSSOnePowerImportance : public LightsSamplingStrategy {
public:
	LSSOnePowerImportance() : LightsSamplingStrategy(), lightDistribution(NULL) { }
	virtual ~LSSOnePowerImportance();
	virtual void Init(const Scene *scene);

	virtual void RequestSamples(const Scene *scene, vector<u_int> &structure) const;
	virtual u_int RequestSamplesCount(const Scene *scene) const { return 6; }
	// Note: results are added to L
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;
	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const;

protected:
	Distribution1D *lightDistribution;
};

class LSSAllPowerImportance : public LSSOnePowerImportance {
public:
	LSSAllPowerImportance() : LSSOnePowerImportance() { }
	virtual ~LSSAllPowerImportance() { }
	// Note: results are added to L
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L) const;
	// SampleLights Method for Augmented Reality
	virtual u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const u_int shadowRayCount, const Point &p, const Normal &n,
		const Vector &wo, BSDF *bsdf, const Sample *sample,
		const float *sampleData, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type) const;
};

class LSSOneLogPowerImportance : public LSSOnePowerImportance {
public:
	LSSOneLogPowerImportance() : LSSOnePowerImportance() { }
	virtual ~LSSOneLogPowerImportance() { }
	virtual void Init(const Scene *scene);
};

//******************************************************************************
// Rendering Hints
//******************************************************************************

//------------------------------------------------------------------------------
// Light Rendering Hints
//------------------------------------------------------------------------------

class LightRenderingHints {
public:
	LightRenderingHints() {
		importance = 1.f;
	}
	void InitParam(const ParamSet &params);

	float GetImportance() const { return importance; }

private:
	float importance;
};

class SurfaceIntegratorRenderingHints {
public:
	SurfaceIntegratorRenderingHints() {
		shadowRayCount = 1;
		nLights = 0;
		lightStrategyType = LightsSamplingStrategy::SAMPLE_AUTOMATIC;
		lsStrategy = NULL;
	}
	~SurfaceIntegratorRenderingHints() {
		delete lsStrategy;
	}

	void InitParam(const ParamSet &params);

	u_int GetShadowRaysCount() const { return shadowRayCount; }
	LightsSamplingStrategy::LightStrategyType GetLightStrategy() const { return lightStrategyType; }

	void InitStrategies(const Scene *scene);
	void RequestSamples(Sample *sample, const Scene *scene, u_int maxDepth);

	// Note: results are added to L and optional parameter V content
	u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, u_int depth, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, vector<float> *V = NULL) const;
	// SampleLights Method for Augmented Reality
	u_int SampleLights(const TsPack *tspack, const Scene *scene,
		const Point &p, const Normal &n, const Vector &wo, BSDF *bsdf,
		const Sample *sample, u_int depth, const SWCSpectrum &scale,
		vector<SWCSpectrum> &L, int rayDepth, bool from_IsSup, bool to_IsSup, bool path_type, vector<float> *V = NULL) const;
private:
	// Light Strategies
	u_int shadowRayCount, nLights;
	LightsSamplingStrategy::LightStrategyType lightStrategyType;
	LightsSamplingStrategy *lsStrategy;
	u_int lightSampleOffset;
};

}

#endif	/* _RENDERINGHINTS_H */

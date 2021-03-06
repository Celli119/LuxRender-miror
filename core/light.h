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

#ifndef LUX_LIGHT_H
#define LUX_LIGHT_H
// light.h*
#include "lux.h"
#include "geometry/transform.h"
#include "error.h"
#include "renderinghints.h"
#include "queryable.h"

#include "luxrays/core/geometry/motionsystem.h"
#include "luxrays/core/color/swcspectrum.h"

// Light Declarations

namespace lux
{

class  Light : public Queryable {
public:
	// Light Interface
	Light(const string &name, const Transform &l2w, u_int ns = 1U)
		: Queryable(name), nSamples(max(1U, ns)), nrPortalShapes(0),
		PortalArea(0.f), group(0), LightToWorld(l2w),
		havePortalShape(false) {
		if (LightToWorld.HasScale())
			LOG(LUX_DEBUG,LUX_UNIMPLEMENT)<< "Scaling detected in light-to-world transformation! Some lights might not support it yet.";

		AddIntAttribute(*this, "group", "Light group", &Light::group);
	}
	virtual ~Light() { }
	const Volume *GetVolume() const { return volume.get(); }
	void SetVolume(boost::shared_ptr<Volume> &v) {
		// Create a temporary to increase shared count
		// The assignment is just a swap
		boost::shared_ptr<Volume> vol(v);
		volume = vol;
	}
	virtual float Power(const Scene &scene) const = 0;
	virtual bool IsDeltaLight() const = 0;
	virtual bool IsEnvironmental() const = 0;
	virtual bool Le(const Scene &scene, const Sample &sample, const Ray &r,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const { return false; }
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const = 0;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *L) const = 0;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const = 0;
	const LightRenderingHints *GetRenderingHints() const { return &hints; }

	void AddPortalShape(boost::shared_ptr<Primitive> &shape);

	const Transform &GetTransform() const { return LightToWorld; }
	
	// Light Public Data
	const u_int nSamples;
	u_int nrPortalShapes;
	vector<boost::shared_ptr<Primitive> > PortalShapes;
	float PortalArea;
	u_int group;
protected:
	// Light Protected Data
	const Transform LightToWorld;
	LightRenderingHints hints;
public: // Put last for better data alignment
	bool havePortalShape;
protected:
	boost::shared_ptr<Volume> volume;
};

class AreaLight : public Light {
public:
	// AreaLight Interface
	AreaLight(const string &name, const Transform &l2w, u_int ns) :
		Light(name, l2w, ns) { }
	virtual ~AreaLight() { }
	virtual bool L(const Sample &sample, const Ray &ray,
		const DifferentialGeometry &dg, BSDF **bsdf, float *pdf,
		float *pdfDirect, SWCSpectrum *Le) const = 0;
	virtual bool IsDeltaLight() const { return false; }
	virtual bool IsEnvironmental() const { return false; }

	virtual Texture<SWCSpectrum> *GetTexture() = 0;
	virtual const SampleableSphericalFunction *GetFunc() const = 0;

protected:
	// AreaLight Protected Data
};

class AreaLightImpl : public AreaLight {
public:
	// AreaLight Interface
	AreaLightImpl(const Transform &light2world,
		boost::shared_ptr<Texture<SWCSpectrum> > &Le, float g,
		float pow, float e, SampleableSphericalFunction *ssf,
		u_int ns, const boost::shared_ptr<Primitive> &prim);
	virtual ~AreaLightImpl();
	virtual bool L(const Sample &sample, const Ray &ray,
		const DifferentialGeometry &dg, BSDF **bsdf, float *pdf,
		float *pdfDirect, SWCSpectrum *Le) const;
	virtual float Power(const Scene &scene) const;
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *Le) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *Le) const;

	virtual Texture<SWCSpectrum> *GetTexture() { return Le.get(); }
	virtual const SampleableSphericalFunction *GetFunc() const { return func; }

	static AreaLight *CreateAreaLight(const Transform &light2world,
		const ParamSet &paramSet,
		const boost::shared_ptr<Primitive> &prim);

protected:
	// AreaLight Protected Data
	boost::shared_ptr<Texture<SWCSpectrum> > Le;
	boost::shared_ptr<Primitive> prim;
	float paramGain, gain, power, efficacy, area;
	SampleableSphericalFunction *func;
};

class  InstanceLight : public Light {
public:
	// Light Interface
	InstanceLight(const Transform &l2w, boost::shared_ptr<Light> &l)
		: Light("InstanceLight-" + boost::lexical_cast<string>(this),
		l2w, l->nSamples), light(l) { group = light->group; }
	virtual ~InstanceLight() { }
	virtual float Power(const Scene &scene) const {
		return light->Power(scene);
	}
	virtual bool IsDeltaLight() const { return light->IsDeltaLight(); }
	virtual bool IsEnvironmental() const {
		return light->IsEnvironmental();
	}
	virtual bool Le(const Scene &scene, const Sample &sample, const Ray &r,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const {
		const PartialDifferentialGeometry dgi(Inverse(LightToWorld) *
			dg);
		const float factor = dgi.Volume() / dg.Volume();
		return light->Pdf(Inverse(LightToWorld) * p, dgi) * factor;
	}
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *L) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;

protected:
	boost::shared_ptr<Light> light;
};

class  MotionLight : public Light {
public:
	// Light Interface
	MotionLight(const MotionSystem &mp, boost::shared_ptr<Light> &l)
		: Light("MotionLight-" + boost::lexical_cast<string>(this),
		Transform(), l->nSamples), light(l), motionPath(mp) { group = light->group; }
	virtual ~MotionLight() { }
	virtual float Power(const Scene &scene) const {
		return light->Power(scene);
	}
	virtual bool IsDeltaLight() const { return light->IsDeltaLight(); }
	virtual bool IsEnvironmental() const {
		return light->IsEnvironmental();
	}
	virtual bool Le(const Scene &scene, const Sample &sample, const Ray &r,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const {
		const Transform LightToWorld(motionPath.Sample(dg.time));
		const PartialDifferentialGeometry dgi(Inverse(LightToWorld) *
			dg);
		const float factor = dgi.Volume() / dg.Volume();
		return light->Pdf(Inverse(LightToWorld) * p, dgi) * factor;
	}
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *L) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;

protected:
	boost::shared_ptr<Light> light;
	MotionSystem motionPath;
};

class  InstanceAreaLight : public AreaLight {
public:
	// Light Interface
	InstanceAreaLight(const Transform &l2w, boost::shared_ptr<AreaLight> &l) :
		AreaLight("InstanceAreaLight-" + boost::lexical_cast<string>(this),
		l2w, l->nSamples), light(l) { group = light->group; }
	virtual ~InstanceAreaLight() { }
	virtual float Power(const Scene &scene) const {
		return light->Power(scene);
	}
	virtual bool IsDeltaLight() const { return light->IsDeltaLight(); }
	virtual bool IsEnvironmental() const {
		return light->IsEnvironmental();
	}
	virtual bool Le(const Scene &scene, const Sample &sample, const Ray &r,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;
	virtual bool L(const Sample &sample, const Ray &ray,
		const DifferentialGeometry &dg, BSDF **bsdf, float *pdf,
		float *pdfDirect, SWCSpectrum *Le) const;
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const {
		const PartialDifferentialGeometry dgi(Inverse(LightToWorld) *
			dg);
		const float factor = dgi.Volume() / dg.Volume();
		return light->Pdf(Inverse(LightToWorld) * p, dgi) * factor;
	}
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *L) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;

	virtual Texture<SWCSpectrum> *GetTexture() {
		return light->GetTexture();
	}	
	virtual const SampleableSphericalFunction *GetFunc() const {
		return light->GetFunc();
	}

protected:
	boost::shared_ptr<AreaLight> light;
};

class  MotionAreaLight : public AreaLight {
public:
	// Light Interface
	MotionAreaLight(const MotionSystem &mp, boost::shared_ptr<AreaLight> &l) :
		AreaLight("MotionAreaLight-" + boost::lexical_cast<string>(this),
		Transform(), l->nSamples), light(l), motionPath(mp) { group = light->group; }
	virtual ~MotionAreaLight() { }
	virtual float Power(const Scene &scene) const {
		return light->Power(scene);
	}
	virtual bool IsDeltaLight() const { return light->IsDeltaLight(); }
	virtual bool IsEnvironmental() const {
		return light->IsEnvironmental();
	}
	virtual bool Le(const Scene &scene, const Sample &sample, const Ray &r,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;
	virtual bool L(const Sample &sample, const Ray &ray,
		const DifferentialGeometry &dg, BSDF **bsdf, float *pdf,
		float *pdfDirect, SWCSpectrum *Le) const;
	virtual float Pdf(const Point &p, const PartialDifferentialGeometry &dg) const {
		const Transform LightToWorld(motionPath.Sample(dg.time));
		const PartialDifferentialGeometry dgi(Inverse(LightToWorld) *
			dg);
		const float factor = dgi.Volume() / dg.Volume();
		return light->Pdf(Inverse(LightToWorld) * p, dgi) * factor;
	}
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		float u1, float u2, float u3, BSDF **bsdf, float *pdf,
		SWCSpectrum *L) const;
	virtual bool SampleL(const Scene &scene, const Sample &sample,
		const Point &p, float u1, float u2, float u3,
		BSDF **bsdf, float *pdf, float *pdfDirect,
		SWCSpectrum *L) const;

	virtual Texture<SWCSpectrum> *GetTexture() {
		return light->GetTexture();
	}
	virtual const SampleableSphericalFunction *GetFunc() const {
		return light->GetFunc();
	}

protected:
	boost::shared_ptr<AreaLight> light;
	MotionSystem motionPath;
};

}//namespace lux

#endif // LUX_LIGHT_H

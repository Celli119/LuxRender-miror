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

// scene.cpp*
#include <sstream>
#include <stdlib.h>

#include "scene.h"
#include "camera.h"
#include "film.h"
#include "sampling.h"
#include "volume.h"
#include "error.h"
#include "context.h"
#include "bxdf.h"
#include "light.h"
#include "spectrumwavelengths.h"
#include "transport.h"
#include "primitive.h"
#include "material.h"

#include "randomgen.h"

#include "fastmutex.h"

#include <boost/thread/xtime.hpp>
#include <boost/bind.hpp>

using namespace lux;

// global sample pos/mutex
static u_int sampPos;
static fast_mutex sampPosMutex;

// Engine Control (start/pause/restart) methods
void Scene::Start() {
    SignalThreads(RUN);
}

void Scene::Pause() {
    SignalThreads(PAUSE);
}

void Scene::Exit() {
    SignalThreads(EXIT);
}

u_int Scene::GetThreadsStatus(RenderingThreadInfo *info, u_int maxInfoCount) {
	boost::mutex::scoped_lock lock(renderThreadsMutex);

	for (size_t i = 0; i < min<size_t>(renderThreads.size(), maxInfoCount); ++i) {
		info[i].threadIndex = renderThreads[i]->n;
		info[i].status = renderThreads[i]->signal;
	}

	return renderThreads.size();
}

void Scene::SaveFLM( const string& filename ) {
	camera->film->WriteFilm(filename);
}

// Framebuffer Access for GUI
void Scene::UpdateFramebuffer() {
    camera->film->updateFrameBuffer();

	// I have to call ContributionPool method here in order
	// to acquire splattingMutex lock

	if (contribPool)
		contribPool->CheckFilmWriteOuputInterval();
}

unsigned char* Scene::GetFramebuffer() {
    return camera->film->getFrameBuffer();
}

// histogram access for GUI
void Scene::GetHistogramImage(unsigned char *outPixels, u_int width, u_int height, int options){
	camera->film->getHistogramImage(outPixels, width, height, options);
}


// Parameter Access functions
void Scene::SetParameterValue(luxComponent comp, luxComponentParameters param, double value, u_int index) { 
	if(comp == LUX_FILM)
		camera->film->SetParameterValue(param, value, index);
}
double Scene::GetParameterValue(luxComponent comp, luxComponentParameters param, u_int index) {
	if(comp == LUX_FILM)
		return camera->film->GetParameterValue(param, index);
	else
		return 0.;
}
double Scene::GetDefaultParameterValue(luxComponent comp, luxComponentParameters param, u_int index) {
	if(comp == LUX_FILM)
		return camera->film->GetDefaultParameterValue(param, index);
	else
		return 0.;
}
void Scene::SetStringParameterValue(luxComponent comp, luxComponentParameters param, const string& value, u_int index) { 
}
string Scene::GetStringParameterValue(luxComponent comp, luxComponentParameters param, u_int index) {
	if(comp == LUX_FILM)
		return camera->film->GetStringParameterValue(param, index);
	else
		return "";
}
string Scene::GetDefaultStringParameterValue(luxComponent comp, luxComponentParameters param, u_int index) {
	return "";
}

int Scene::DisplayInterval() {
    return camera->film->getldrDisplayInterval();
}

u_int Scene::FilmXres() {
    return camera->film->GetXPixelCount();
}

u_int Scene::FilmYres() {
    return camera->film->GetYPixelCount();
}

// Statistics Access
double Scene::Statistics(const string &statName) {
	if(statName=="secElapsed") {
		// Dade - s_Timer is inizialized only after the preprocess phase
		if (preprocessDone)
			return s_Timer.Time();
		else
			return 0.0;
	} else if(statName=="samplesSec")
		return Statistics_SamplesPSec();
	else if(statName=="samplesTotSec")
		return Statistics_SamplesPTotSec();
	else if(statName=="samplesPx")
		return Statistics_SamplesPPx();
	else if(statName=="efficiency")
		return Statistics_Efficiency();
	else if(statName=="filmXres")
		return FilmXres();
	else if(statName=="filmYres")
		return FilmYres();
	else if(statName=="displayInterval")
		return DisplayInterval();
	else if(statName == "filmEV")
		return camera->film->EV;
	else if (statName == "enoughSamples")
		return camera->film->enoughSamplePerPixel;
	else {
		std::string eString("luxStatistics - requested an invalid data : ");
		eString+=statName;
		luxError(LUX_BADTOKEN, LUX_ERROR, eString.c_str());
		return 0.;
	}
}

// Control Implementations in Scene:
double Scene::GetNumberOfSamples()
{
	if (s_Timer.Time() - lastTime > .5f) {
		boost::mutex::scoped_lock lockThreads(renderThreadsMutex);
		for (u_int i = 0; i < renderThreads.size(); ++i) {
			fast_mutex::scoped_lock lockStats(renderThreads[i]->statLock);
			stat_Samples += renderThreads[i]->samples;
			stat_blackSamples += renderThreads[i]->blackSamples;
			renderThreads[i]->samples = 0.;
			renderThreads[i]->blackSamples = 0.;
		}
	}

	return stat_Samples + numberOfSamplesFromNetwork;
}

double Scene::Statistics_SamplesPPx()
{
	// divide by total pixels
	int xstart, xend, ystart, yend;
	camera->film->GetSampleExtent(&xstart, &xend, &ystart, &yend);
	return GetNumberOfSamples() / ((xend - xstart) * (yend - ystart));
}

double Scene::Statistics_SamplesPSec()
{
	// Dade - s_Timer is inizialized only after the preprocess phase
	if (!preprocessDone)
		return 0.0;

	double samples = GetNumberOfSamples();
	double time = s_Timer.Time();
	double dif_samples = samples - lastSamples;
	double elapsed = time - lastTime;
	lastSamples = samples;
	lastTime = time;

	// return current samples / sec total
	if (elapsed == 0.0)
		return 0.0;
	else
		return dif_samples / elapsed;
}

double Scene::Statistics_SamplesPTotSec()
{
	// Dade - s_Timer is inizialized only after the preprocess phase
	if (!preprocessDone)
		return 0.0;

	double samples = GetNumberOfSamples();
	double time = s_Timer.Time();

	// return current samples / total elapsed secs
	return samples / time;
}

double Scene::Statistics_Efficiency()
{
	if (stat_Samples == 0.0)
		return 0.0;

	return (100.f * stat_blackSamples) / stat_Samples;
}

void Scene::SignalThreads(ThreadSignals signal) {
	boost::mutex::scoped_lock lock(renderThreadsMutex);

    for(unsigned int i=0;i<renderThreads.size();i++) {
		if(renderThreads[i])
			renderThreads[i]->signal=signal;
    }
    CurThreadSignal = signal;
}

// Scene Methods -----------------------
RenderThread::RenderThread(u_int _n, ThreadSignals _signal,
	SurfaceIntegrator* _Si, VolumeIntegrator* _Vi, Sampler* _Splr,
	Camera* _Cam, Scene* _Scn) : n(_n), signal(_signal),
	surfaceIntegrator(_Si), volumeIntegrator(_Vi), sample(NULL),
	sampler(_Splr->clone()), camera(_Cam), scene(_Scn), thread(NULL),
	samples(0.), blackSamples(0.)
{
	sample = new Sample(surfaceIntegrator, volumeIntegrator, scene);

}

RenderThread::~RenderThread()
{
//	delete sampler; //FIXME some samplers don't clone the data pointers so deleting here will result in a double free in Scene::~Scene and use of freed memory in other render threads
	delete sample;
	delete thread;
}

void RenderThread::Render(RenderThread *myThread) {
	if (myThread->scene->IsFilmOnly())
		return;

	// Dade - wait the end of the preprocessing phase
	while (!myThread->scene->preprocessDone) {
		boost::xtime xt;
		boost::xtime_get(&xt, boost::TIME_UTC);
		++xt.sec;
		boost::thread::sleep(xt);
	}


	// initialize the thread's rangen
	u_long seed = myThread->scene->seedBase + myThread->n;

	// initialize the threads tspack
	myThread->tspack = new TsPack();	// TODO - radiance - remove

	myThread->tspack->swl = new SpectrumWavelengths();
	myThread->tspack->rng = new RandomGenerator();
	myThread->tspack->rng->init(seed);
	myThread->tspack->arena = new MemoryArena();
	myThread->tspack->camera = myThread->scene->camera->Clone();
	myThread->tspack->time = 0.f;

	myThread->sampler->SetTsPack(myThread->tspack);



	// allocate sample pos
	u_int *useSampPos = new u_int();
	*useSampPos = 0;
	u_int maxSampPos = myThread->sampler->GetTotalSamplePos();


	// Trace rays: The main loop
	while (true) { 


		if (!myThread->sampler->GetNextSample(myThread->sample, useSampPos)) {

			// Dade - we have done, check what we have to do now
			if (myThread->scene->suspendThreadsWhenDone) {
				myThread->signal = PAUSE;

				// Dade - wait for a resume rendering or exit
				while (myThread->signal == PAUSE) {
					boost::xtime xt;
					boost::xtime_get(&xt, boost::TIME_UTC);
					xt.sec += 1;
					boost::thread::sleep(xt);
				}

				if (myThread->signal == EXIT)
					break;
				else
					continue;
			} else
				break;
		}

		// save ray time value to tspack for later use
		myThread->tspack->time = myThread->tspack->camera->GetTime(myThread->sample->time);
		// sample camera transformation
		myThread->tspack->camera->SampleMotion(myThread->tspack->time);



		// Sample new SWC thread wavelengths
		myThread->tspack->swl->Sample(myThread->sample->wavelengths);

		while (myThread->signal == PAUSE) {
			boost::xtime xt;
			boost::xtime_get(&xt, boost::TIME_UTC);
			xt.sec += 1;
			boost::thread::sleep(xt);
		}
		if (myThread->signal == EXIT)
			break;

		// Evaluate radiance along camera ray
		// Jeanphi - Hijack statistics until volume integrator revamp
		{

			const u_int nContribs = myThread->surfaceIntegrator->Li(myThread->tspack,
				myThread->scene, myThread->sample);


			// update samples statistics
			fast_mutex::scoped_lock lockStats(myThread->statLock);
			myThread->blackSamples += nContribs;
			++(myThread->samples);
		}

		myThread->sampler->AddSample(*(myThread->sample));

		// Free BSDF memory from computing image sample value
		myThread->tspack->arena->FreeAll();

		// increment (locked) global sample pos if necessary (eg maxSampPos != 0)
		if (*useSampPos == ~0U && maxSampPos != 0) {
			fast_mutex::scoped_lock lock(sampPosMutex);
			sampPos++;
			if (sampPos == maxSampPos)
				sampPos = 0;
			*useSampPos = sampPos;
		}

#ifdef WIN32
		//Work around Windows bad scheduling -- Jeanphi
		myThread->thread->yield();
#endif


	}

	myThread->sampler->Cleanup();


	delete useSampPos;

	delete myThread->tspack->swl;
	delete myThread->tspack->rng;
	delete myThread->tspack->arena;
//	delete myThread->tspack->camera; //FIXME deleting the camera clone would delete the film!
	delete myThread->tspack;
	return;
}

u_int Scene::CreateRenderThread()
{

	if (IsFilmOnly())
		return 0;

	boost::mutex::scoped_lock lock(renderThreadsMutex);

	// Avoid to create the thread in case signal is EXIT. for instance, it
	// can happen when the rendering is done.
	if (CurThreadSignal != EXIT) {
		RenderThread *rt = new  RenderThread(renderThreads.size(),
			CurThreadSignal, surfaceIntegrator, volumeIntegrator,
			sampler, camera, this);

		renderThreads.push_back(rt);
		rt->thread = new boost::thread(boost::bind(RenderThread::Render, rt));
	}



	return renderThreads.size();
}

void Scene::RemoveRenderThread()
{
	boost::mutex::scoped_lock lock(renderThreadsMutex);

	if (renderThreads.size() == 0)
		return;
	renderThreads.back()->signal = EXIT;
	renderThreads.back()->thread->join();
	delete renderThreads.back();
	renderThreads.pop_back();

}

void Scene::Render() {
	if (IsFilmOnly())
		return;

	if (lights.size() == 0) {
		luxError(LUX_MISSINGDATA, LUX_SEVERE, "No light sources defined in scene; nothing to render.");
		return;
	}

	// Dade - I have to do initiliaziation here for the current thread.
	// It can be used by the Preprocess() methods.

	// initialize the thread's rangen
	u_long seed = seedBase - 1;


	// initialize the contribution pool

	contribPool = new ContributionPool();
	contribPool->SetFilm(camera->film);

	// initialize the preprocess thread's tspack
	tspack = new TsPack();
	tspack->swl = new SpectrumWavelengths();
	tspack->rng = new RandomGenerator();
	tspack->rng->init(seed);
	tspack->arena = new MemoryArena();

	sampler->SetTsPack(tspack);


	// integrator preprocessing
	camera->film->SetScene(this);
	sampler->SetFilm(camera->film);
	sampler->SetContributionPool(contribPool);
	surfaceIntegrator->Preprocess(tspack, this);
	volumeIntegrator->Preprocess(tspack, this);
	camera->film->CreateBuffers();

	// Dade - to support autofocus for some camera model
	camera->AutoFocus(this);

	sampPos = 0;

	///////////////////////////////////////////////////////////////
	load_suport();	
	///////////////////////////////////////////////////////////////

	//start the timer
	s_Timer.Start();

	// Dade - preprocessing done
	preprocessDone = true;
	Context::GetActive()->SceneReady();

	// initial thread signal is paused
	CurThreadSignal = RUN;

	// add a thread
	if (CreateRenderThread() > 0) {

		// The first thread can not be removed
		// it will terminate when the rendering is finished


		renderThreads[0]->thread->join();

		// rendering done, now I can remove all rendering threads
		{
			boost::mutex::scoped_lock lock(renderThreadsMutex);

			// wait for all threads to finish their job
			for (u_int i = 0; i < renderThreads.size(); ++i) {
				renderThreads[i]->thread->join();
				delete renderThreads[i];

			}
			renderThreads.clear();

			// I change the current signal to exit in order to disable the creation
			// of new threads after this point
			CurThreadSignal = EXIT;
		}

		// Flush the contribution pool
		contribPool->Flush();
		contribPool->Delete();
	}


	delete tspack->swl;
	delete tspack->rng;
	delete tspack->arena;
	delete tspack;
}

Scene::~Scene() {
	delete camera;
	delete sampler;
	delete surfaceIntegrator;
	delete volumeIntegrator;
	delete contribPool;
	delete volumeRegion;
	for (u_int i = 0; i < lights.size(); ++i)
		delete lights[i];
}

Scene::Scene(Camera *cam, SurfaceIntegrator *si, VolumeIntegrator *vi,
	Sampler *s, boost::shared_ptr<Primitive> &accel,
	const vector<Light *> &lts, const vector<string> &lg, Region *vr) :
	lastSamples(0.), lastTime(0.), numberOfSamplesFromNetwork(0.),
	stat_Samples(0.), stat_blackSamples(0.), aggregate(accel), lights(lts),
	lightGroups(lg), camera(cam), volumeRegion(vr), surfaceIntegrator(si),
	volumeIntegrator(vi), sampler(s), contribPool(NULL),
	CurThreadSignal(PAUSE), tspack(NULL), filmOnly(false),
	preprocessDone(false), suspendThreadsWhenDone(false)
{
	s_Timer.Reset();
	// Scene Constructor Implementation
	bound = Union(aggregate->WorldBound(), camera->Bounds());
	if (volumeRegion)
		bound = Union(bound, volumeRegion->WorldBound());

	// Dade - Initialize the base seed with the standard C lib random number generator
	seedBase = rand();

	camera->film->RequestBufferGroups(lightGroups);

}

Scene::Scene(Camera *cam) :
	lastSamples(0.), lastTime(0.), numberOfSamplesFromNetwork(0.),
	stat_Samples(0.), stat_blackSamples(0.), camera(cam),
	volumeRegion(NULL), surfaceIntegrator(NULL),
	volumeIntegrator(NULL), sampler(NULL), contribPool(NULL),
	CurThreadSignal(PAUSE), tspack(NULL), filmOnly(true),
	preprocessDone(false), suspendThreadsWhenDone(false)
{
	for(u_int i = 0; i < cam->film->GetNumBufferGroups(); i++)
		lightGroups.push_back( cam->film->GetGroupName(i) );
	s_Timer.Reset();

	// Dade - Initialize the base seed with the standard C lib random number generator
	seedBase = rand();

}

SWCSpectrum Scene::Li(const RayDifferential &ray,
		const Sample *sample, float *alpha) const {
//  NOTE - radiance - leave these off for now, should'nt be used (broken with multithreading)
//  TODO - radiance - cleanup / reimplement into integrators
//	SWCSpectrum Lo = surfaceIntegrator->Li(this, ray, sample, alpha);
//	SWCSpectrum T = volumeIntegrator->Transmittance(this, ray, sample, alpha);
//	SWCSpectrum Lv = volumeIntegrator->Li(this, ray, sample, alpha);
//	return T * Lo + Lv;
	return 0.f;
}

void Scene::Transmittance(const TsPack *tsp, const Ray &ray, 
	const Sample *sample, SWCSpectrum *const L) const {
    volumeIntegrator->Transmittance(tsp, this, ray, sample, NULL, L);
}

void Scene::load_suport(void) const{

	vector<boost::shared_ptr<Primitive> > objects;
	Aggregate *aggr = dynamic_cast<Aggregate*>(aggregate.get());
	aggr->GetPrimitives(objects);

	Primitive *shp;

	Vector Z;
	Point cam = camera->CameraToWorld( Point(0.f, 0.f, 0.f) );

	for (u_int i=0; i < objects.size(); i++) { 

		shp = objects[i].get(); 
		if( shp != NULL ) 
			if( shp->IsSupport() ) {
				Vector N;
				Point P;
				shp->GetNormal(&N);
				shp->GetBaryPoint(&P);
				Z = cam-P;
				if( Dot(N,Z) < 0.f )  N = N * (-1);
				for( u_int k=0 ; k < lights.size() ; k++ ) 
					if (lights[k]->IsEnvironmental()){
						float Pow = lights[k]->DirProb(N);
						shp->SetScale(Pow); 
					}
			}
	}
}



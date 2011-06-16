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

#ifndef LUX_SAMPLING_H
#define LUX_SAMPLING_H
// sampling.h*
#include "lux.h"
#include "randomgen.h"
#include "contribution.h"
#include "spectrumwavelengths.h"
#include "film.h"

namespace lux
{

class Sample {
public:
	// Sample Public Methods
	Sample();
	~Sample();

	u_int Add1D(u_int num) {
		n1D.push_back(num);
		return n1D.size()-1;
	}
	u_int Add2D(u_int num) {
		n2D.push_back(num);
		return n2D.size()-1;
	}
	u_int AddxD(vector<u_int> &structure, u_int num) {
		nxD.push_back(num);
		sxD.push_back(structure);
		u_int d = 0;
		for (u_int i = 0; i < structure.size(); ++i)
			d += structure[i];
		dxD.push_back(d);
		return nxD.size()-1;
	}
	void AddContribution(float x, float y, const XYZColor &c, float a,
		float zd, float v, u_int b = 0, u_int g = 0) const {
		contributions.push_back(Contribution(x, y, c, a, zd, v, b, g));
	}

	//Sample public data
	vector<u_int> n1D, n2D, nxD, dxD;
	vector<vector<u_int> > sxD;
	// Reference to the sampler for lazy evaluation
	mutable MemoryArena arena;
	const RandomGenerator *rng;
	mutable ContributionBuffer *contribBuffer;
	Sampler *sampler;
	void *samplerData;
	// _Sample_ Data generated by the Camera.
	float imageX, imageY;
	float lensU, lensV;
	float time;
	float wavelengths;
	SpectrumWavelengths swl;
	Camera *camera;
	float realTime;
public:
	mutable vector<Contribution> contributions;
};

class  Sampler {
public:
	// Sampler Interface
	Sampler(int xstart, int xend, int ystart, int yend, u_int spp);
	virtual ~Sampler() {}
	virtual void InitSample(Sample *sample) const = 0;
	virtual void FreeSample(Sample *sample) const = 0;
	virtual bool GetNextSample(Sample *sample) = 0;
	virtual float GetOneD(const Sample &sample, u_int num, u_int pos) = 0;
	virtual void GetTwoD(const Sample &sample, u_int num, u_int pos,
		float u[2]) = 0;
	virtual float *GetLazyValues(const Sample &sample, u_int num, u_int pos) = 0;
	virtual u_int GetTotalSamplePos() = 0;
	u_int TotalSamples() const {
		return samplesPerPixel * static_cast<u_int>((xPixelEnd - xPixelStart) * (yPixelEnd - yPixelStart));
	}
	virtual u_int RoundSize(u_int size) const = 0;
	virtual void SetFilm(Film* f) { film = f; }
	virtual void GetBufferType(BufferType *t) { }
	virtual void AddSample(const Sample &sample);

	// Sampler Public Data
	int xPixelStart, xPixelEnd, yPixelStart, yPixelEnd;
	u_int samplesPerPixel;
	Film *film;
};

// PxLoc X and Y pixel coordinate struct
struct PxLoc {
	int x;
	int y;
};
class PixelSampler {
public:
	PixelSampler() : renderingDone(false) {}
	virtual ~PixelSampler() {}

	virtual u_int GetTotalPixels() = 0;
	virtual bool GetNextPixel(int *xPos, int *yPos, const u_int usePos) = 0;

	// Dade - used by sampler to store the renderingDone condition. Placed here
	// because PixelSampler is shared among threads
	bool renderingDone;
};

void StratifiedSample1D(const RandomGenerator &rng, float *samples, u_int nsamples, bool jitter = true);
void StratifiedSample2D(const RandomGenerator &rng, float *samples, u_int nx, u_int ny, bool jitter = true);
// The following 2 Shuffle() should be replaced by a template
void Shuffle(const RandomGenerator &rng, float *samp, u_int count, u_int dims);
void Shuffle(const RandomGenerator &rng, u_int *samp, u_int count, u_int dims);
void LatinHypercube(const RandomGenerator &rng, float *samples, u_int nSamples, u_int nDim);

// Sampling Inline Functions
inline double RadicalInverse(u_int n, u_int base)
{
	double val = 0.;
	double invBase = 1. / base, invBi = invBase;
	while (n > 0) {
		// Compute next digit of radical inverse
		u_int d_i = (n % base);
		val += d_i * invBi;
		n /= base;
		invBi *= invBase;
	}
	return val;
}
inline double FoldedRadicalInverse(u_int n, u_int base)
{
	const double invBase = 1. / base;
	double val = 0., invBi = invBase;
	u_int modOffset = 0;
	while (val + base * invBi != val) {
		// Compute next digit of folded radical inverse
		u_int digit = ((n + modOffset) % base);
		val += digit * invBi;
		n /= base;
		invBi *= invBase;
		++modOffset;
	}
	return val;
}
inline float VanDerCorput(u_int n, u_int scramble = 0)
{
	n = (n << 16) | (n >> 16);
	n = ((n & 0x00ff00ff) << 8) | ((n & 0xff00ff00) >> 8);
	n = ((n & 0x0f0f0f0f) << 4) | ((n & 0xf0f0f0f0) >> 4);
	n = ((n & 0x33333333) << 2) | ((n & 0xcccccccc) >> 2);
	n = ((n & 0x55555555) << 1) | ((n & 0xaaaaaaaa) >> 1);
	n ^= scramble;
	return static_cast<float>(static_cast<double>(n) / static_cast<double>(0x100000000LL));
}
inline float Sobol2(u_int n, u_int scramble = 0)
{
	for (u_int v = 1u << 31; n != 0; n >>= 1, v ^= v >> 1)
		if (n & 0x1) scramble ^= v;
	return static_cast<float>(static_cast<double>(scramble) / static_cast<double>(0x100000000LL));
}
inline float LarcherPillichshammer2(u_int n, u_int scramble = 0)
{
	for (u_int v = 1u << 31; n != 0; n >>= 1, v |= v >> 1)
		if (n & 0x1) scramble ^= v;
	return static_cast<float>(static_cast<double>(scramble) / static_cast<double>(0x100000000LL));
}
inline float Halton(u_int n, u_int scramble = 0)
{
	double s = FoldedRadicalInverse(n, 2);
	u_int s0 = static_cast<u_int>(s * 0x100000000LL);
	s0 ^= scramble;
	return static_cast<float>(static_cast<double>(s0) / static_cast<double>(0x100000000LL));
}
inline float Halton2(u_int n, u_int scramble = 0)
{
	double s = FoldedRadicalInverse(n, 3);
	u_int s0 = static_cast<u_int>(s * 0x100000000LL);
	s0 ^= scramble;
	return static_cast<float>(static_cast<double>(s0) / static_cast<double>(0x100000000LL));
}
inline void SampleHalton(u_int n, u_int scramble[2], float sample[2])
{
	sample[0] = Halton(n, scramble[0]);
	sample[1] = Halton2(n, scramble[1]);
}
inline void Sample02(u_int n, u_int scramble[2], float sample[2])
{
	sample[0] = VanDerCorput(n, scramble[0]);
	sample[1] = Sobol2(n, scramble[1]);
}

inline void LDShuffleScrambled1D(const RandomGenerator &rng, u_int nSamples,
	u_int nPixel, float *samples)
{
	u_int scramble = rng.uintValue();
	for (u_int i = 0; i < nSamples * nPixel; ++i)
		samples[i] = VanDerCorput(i, scramble);
	for (u_int i = 0; i < nPixel; ++i)
		Shuffle(rng, samples + i * nSamples, nSamples, 1);
	Shuffle(rng, samples, nPixel, nSamples);
}
inline void LDShuffleScrambled2D(const RandomGenerator &rng, u_int nSamples,
	u_int nPixel, float *samples)
{
	u_int scramble[2] = { rng.uintValue(), rng.uintValue() };
	for (u_int i = 0; i < nSamples * nPixel; ++i)
		Sample02(i, scramble, &samples[2*i]);
	for (u_int i = 0; i < nPixel; ++i)
		Shuffle(rng, samples + 2 * i * nSamples, nSamples, 2);
	Shuffle(rng, samples, nPixel, 2 * nSamples);
}
inline void HaltonShuffleScrambled1D(const RandomGenerator &rng, u_int nSamples,
	u_int nPixel, float *samples)
{
	u_int scramble = rng.uintValue();
	for (u_int i = 0; i < nSamples * nPixel; ++i)
		samples[i] = Halton(i, scramble);
	for (u_int i = 0; i < nPixel; ++i)
		Shuffle(rng, samples + i * nSamples, nSamples, 1);
	Shuffle(rng, samples, nPixel, nSamples);
}
inline void HaltonShuffleScrambled2D(const RandomGenerator &rng, u_int nSamples,
	u_int nPixel, float *samples)
{
	u_int scramble[2] = { rng.uintValue(), rng.uintValue() };
	for (u_int i = 0; i < nSamples * nPixel; ++i)
		SampleHalton(i, scramble, &samples[2*i]);
	for (u_int i = 0; i < nPixel; ++i)
		Shuffle(rng, samples + 2 * i * nSamples, nSamples, 2);
	Shuffle(rng, samples, nPixel, 2 * nSamples);
}

// Directly from PBRT2
// smallest floating point value less than one; all canonical random samples
// should be <= this.
#ifdef WIN32
// sadly, MSVC2008 (at least) doesn't support hexidecimal fp constants...
static const float OneMinusEpsilon=0.9999999403953552f;
#else
static const float OneMinusEpsilon=0x1.fffffep-1;
#endif

// Directly from PBRT2
inline void GeneratePermutation(u_int *buf, u_int b, const RandomGenerator &rng) {
	for (u_int i = 0; i < b; ++i)
		buf[i] = i;
	Shuffle(rng, buf, b, 1);
}

// Directly from PBRT2
inline double PermutedRadicalInverse(u_int n, u_int base, const u_int *p) {
	double val = 0;
	double invBase = 1. / base, invBi = invBase;

	while (n > 0) {
		u_int d_i = p[n % base];
		val += d_i * invBi;
		n *= invBase;
		invBi *= invBase;
	}
	return val;
}

// Directly from PBRT2
class PermutedHalton {
	public:
		// PermutedHalton Public Methods
		PermutedHalton(u_int d, const RandomGenerator &rng);

		~PermutedHalton() {
			delete[] b;
			delete[] permute;
		}

		void Sample(u_int n, float *out) const {
			u_int *p = permute;
			for (u_int i = 0; i < dims; ++i) {
				out[i] = min<float>(float(PermutedRadicalInverse(n, b[i], p)), OneMinusEpsilon);
				p += b[i];
			}
		}

	private:
		// PermutedHalton Private Data
		u_int dims;
		u_int *b, *permute;

		PermutedHalton(const PermutedHalton &);
		PermutedHalton & operator=(const PermutedHalton &);
};

}//namespace lux

#endif // LUX_SAMPLING_H

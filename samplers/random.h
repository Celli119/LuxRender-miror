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

// random.cpp*
#include "sampling.h"
#include "paramset.h"
#include "film.h"
class RandomSampler : public Sampler
{
public:
    RandomSampler(int xstart, int xend, int ystart,
                  int yend, int xs, int ys, bool prog);
    ~RandomSampler()
    {
        FreeAligned(imageSamples);
    }
    bool GetNextSample(Sample *sample);
    int RoundSize(int sz) const
    {
        return sz;
    }
	void setSeed( u_int s ) { /* TODO add different seeds and new backend random generator for threads - radiance */ }
    virtual RandomSampler* clone() const; // Lux (copy) constructor for multithreading

    static Sampler *CreateSampler(const ParamSet &params, const Film *film);
private:
    // RandomSampler Private Data
    bool jitterSamples;
    int xPos, yPos, xPixelSamples, yPixelSamples;
    float *imageSamples, *lensSamples, *timeSamples;
    int samplePos;
	bool fs_progressive;
};

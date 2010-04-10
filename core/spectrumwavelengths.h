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

#ifndef LUX_SPECTRUMWAVELENGTHS_H
#define LUX_SPECTRUMWAVELENGTHS_H
// Spectrumwavelengths.h*
#include "lux.h"
#include "spectrum.h"
#include "spd.h"

#include "data/xyzbasis.h"

namespace lux
{

class	SpectrumWavelengths {
public:

	// SpectrumWavelengths Public Methods
	SpectrumWavelengths();
	~SpectrumWavelengths();

	inline void Sample(float u1) {
		single = false;
		u1 *= WAVELENGTH_SAMPLES;
		single_w = Floor2UInt(u1);
		u1 -= single_w;

		// Sample new stratified wavelengths and precompute RGB/XYZ data
		const float offset = float(WAVELENGTH_END - WAVELENGTH_START) * inv_WAVELENGTH_SAMPLES;
		const float scale = 683.f * offset;
		float waveln = WAVELENGTH_START + u1 * offset;
		for (u_int i = 0; i < WAVELENGTH_SAMPLES; ++i) {
			// Interpolate RGB Conversion SPDs
			spect_w.c[i] = spd_w->sample(waveln);
			spect_c.c[i] = spd_c->sample(waveln);
			spect_m.c[i] = spd_m->sample(waveln);
			spect_y.c[i] = spd_y->sample(waveln);
			spect_r.c[i] = spd_r->sample(waveln);
			spect_g.c[i] = spd_g->sample(waveln);
			spect_b.c[i] = spd_b->sample(waveln);
			// Interpolate XYZ Conversion weights
			const float w0 = waveln - CIEstart;
			int i0 = max(Floor2Int(w0), 0);
			const float b0 = w0 - i0;
			cie_X[i] = Lerp(b0, CIE_X[i0], CIE_X[i0 + 1]) * scale;
			cie_Y[i] = Lerp(b0, CIE_Y[i0], CIE_Y[i0 + 1]) * scale;
			cie_Z[i] = Lerp(b0, CIE_Z[i0], CIE_Z[i0 + 1]) * scale;
			w[i] = waveln;
			waveln += offset;
		}
	}

	inline float SampleSingle() {
		if (!single) {
			single = true;
			for (int i = 0; i < WAVELENGTH_SAMPLES; ++i) {
				cie_X[i] *= WAVELENGTH_SAMPLES;
				cie_Y[i] *= WAVELENGTH_SAMPLES;
				cie_Z[i] *= WAVELENGTH_SAMPLES;
			}
		}
		return w[single_w];
	}

	float w[WAVELENGTH_SAMPLES]; // Wavelengths in nm

	u_int  single_w; // Chosen single wavelength bin
	bool single; // Split to single

	float cie_X[WAVELENGTH_SAMPLES], cie_Y[WAVELENGTH_SAMPLES], cie_Z[WAVELENGTH_SAMPLES]; // CIE XYZ weights
	SWCSpectrum spect_w, spect_c, spect_m;	// white, cyan, magenta
	SWCSpectrum spect_y, spect_r, spect_g;	// yellow, red, green
	SWCSpectrum spect_b;	// blue


private:
	SPD *spd_w, *spd_c, *spd_m, *spd_y,
		*spd_r, *spd_g, *spd_b;
};


}//namespace lux

#endif // LUX_SPECTRUMWAVELENGTHS_H
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

// beckmann.cpp*
#include "beckmann.h"
#include "geometry/vector.h"

using namespace lux;

Beckmann::Beckmann(float rms) {
  r = rms;
}

float Beckmann::D(const Vector &wh) const {
  float costhetah = CosTheta(wh);
  float theta = acosf(costhetah);
  float tanthetah = tanf(theta);

  float dfac = tanthetah / r;

  return expf(-(dfac * dfac)) / (r * r * powf(costhetah, 4.f));
}

void Beckmann::Sample_f(const Vector &wo, Vector *wi, float u1, float u2, float *pdf) const {
  // Compute sampled half-angle vector $\wh$ for Beckmann distribution
  // Adapted from B. Walter et al, Microfacet Models for Refraction, Eurographics Symposium on Rendering, 2007, page 7

  float theta = atanf(sqrtf(-(r * r) * logf(1.f - u1)));
  float costheta = cosf(theta);
  float sintheta = sqrtf(max(0.f, 1.f - costheta * costheta));
  float phi = u2 * 2.f * M_PI;

  Vector H = SphericalDirection(sintheta, costheta, phi);

  if (!SameHemisphere(wo, H))
    H.z *= -1.f;

  // Compute incident direction by reflecting about $\wh$
  *wi = -wo + 2.f * Dot(wo, H) * H;

  // Compute PDF for \wi from Beckmann distribution - note that the inverse of the integral over
  // the Beckmann distribution is not available in closed form, so this is not really correct
  // (see Kelemen and Szirmay-Kalos / Microfacet Based BRDF Model, Eurographics 2001)

  float conversion_factor = 1.f / (4.f * Dot(wo, H));
  float beckmann_pdf = conversion_factor * D(H);

  *pdf = beckmann_pdf;
}

// NB: See note above!
float Beckmann::Pdf(const Vector &wo, const Vector &wi) const {
  Vector H = Normalize(wo + wi);
  float conversion_factor = 1.f / 4.f * Dot(wo, H);
  float beckmann_pdf = conversion_factor * D(H);

  return beckmann_pdf;
}

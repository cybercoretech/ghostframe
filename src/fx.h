#pragma once

#include "editrecipe.h"

#include <vips/vips8>

// Ghostframe's Cyber FX and text layers. Every function takes and returns a
// 3-band float image with values in 0..1 covering the WHOLE output frame
// (positions, centres and grids depend on it). `scale` is image pixels per
// full-resolution pixel, so pixel-sized effects match between preview and
// export. Randomness uses fixed seeds, so renders are repeatable.
namespace Fx {

// Gradient map → LUT → bloom → chromatic aberration → glitch → pixel sort →
// halftone → scanlines → grain → vignette.
vips::VImage apply(const vips::VImage &rgb, const EditRecipe &recipe, double scale);

// Text / watermark layers, composited in order.
vips::VImage applyText(const vips::VImage &rgb, const QList<EditRecipe::TextLayer> &layers);

// The gradient-map table for these colours (hex, dark → light): 256×1,
// 3-band float. Exposed for tests.
vips::VImage gradientTable(const QStringList &colours);

}  // namespace Fx

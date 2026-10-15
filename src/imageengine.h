#pragma once

#include "editrecipe.h"

#include <QImage>
#include <QSize>
#include <QString>

#include <vips/vips8>

// Ghostframe's image pipeline on libvips. Everything here is synchronous and
// thread-safe; ImageEditor calls it from worker threads. Errors are reported
// as vips::VError exceptions (callers catch them) unless noted otherwise.
namespace ImageEngine {

// Call once at startup, before any other function. Returns false (with a
// message) if libvips cannot start.
bool initialize(const char *argv0, QString *error);
void shutdown();

// Pixel size of the original as displayed, i.e. after EXIF auto-rotation.
// Reads only the header.
QSize sourceSize(const QString &path);

// The full-resolution original, auto-rotated, in sRGB/RGB16/B_W/GREY16.
vips::VImage loadOriginal(const QString &path);

// A small in-memory working copy (fits within maxSize, never upscaled) that
// previews are rendered from. Uses shrink-on-load, so it's fast even for
// very large files.
vips::VImage loadProxy(const QString &path, QSize maxSize = {1600, 1200});

// Runs a recipe. Works at any scale because crop is normalised.
vips::VImage apply(const vips::VImage &image, const EditRecipe &recipe);

// 8-bit sRGB QImage (RGB888 or RGBA8888) for display.
QImage toQImage(const vips::VImage &image);

// Renders the recipe on the full-resolution original and writes `destination`;
// the format comes from its suffix. JPEG and WebP are written at quality 95.
// Never throws; returns false and fills `error` instead.
bool exportImage(const QString &source, const EditRecipe &recipe, const QString &destination, QString *error);

}  // namespace ImageEngine

#pragma once

#include "editrecipe.h"

#include <QImage>
#include <QRectF>
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

struct ApplyOptions {
    // Image pixels per full-resolution pixel (e.g. proxy width / original
    // width). Scales pixel radii (sharpen, noise reduction) so a preview
    // looks like the export.
    double scale = 1.0;
    // If valid: only this part of the output frame (0..1, after crop) is
    // rendered. Used by 1:1 zoom.
    QRectF region;
};

// Runs a recipe. Geometry is resolution-independent (normalised), so this
// works on the proxy and the original alike.
vips::VImage apply(const vips::VImage &image, const EditRecipe &recipe, const ApplyOptions &options = {});

// Size of the frame after rotate/flip/straighten/perspective but before crop,
// for an input of `size`. Crop rectangles are fractions of this frame.
QSize geometryFrame(QSize size, const EditRecipe &recipe);

// The per-channel tone curve (levels, blacks/whites, shadows/highlights,
// brightness/contrast, curves) at x in 0..1. channel 0..2 = R, G, B.
double toneValue(const EditRecipe &recipe, int channel, double x);

// Monotone cubic (Fritsch–Carlson) interpolation through sorted points:
// no overshoot, so a curve never inverts tones. Empty = identity.
double curveValue(const QList<QPointF> &points, double x);

// 8-bit sRGB QImage (RGB888 or RGBA8888) for display.
QImage toQImage(const vips::VImage &image);

// Paints clipped pixels: any channel at 255 → pink, all channels at 0 → cyan.
vips::VImage clippingOverlay(const vips::VImage &image);

// RGB + luminance histogram of a display image, drawn into a width×height
// QImage in Cybercore colours.
QImage histogramImage(const vips::VImage &image, QSize size = {256, 90});

struct ExportOptions {
    // Target pixel size; invalid = the recipe's own output size.
    QSize size;
    // Fill: scale to cover, centre-crop. FitBlur: scale to fit over a
    // blurred, darkened fill (vertical Shorts from landscape photos).
    // FitPad: scale to fit on `padColor`.
    enum Fit { Fill, FitBlur, FitPad } fit = Fill;
    QString padColor = QStringLiteral("080a0d");
    int quality = 0;  // JPEG / WebP / AVIF quality; 0 = 95
};

// Renders the recipe on the full-resolution original, resizes per `options`
// and writes `destination`; the format comes from its suffix (jpg, png,
// webp, avif, tif). Never throws; returns false and fills `error` instead.
bool exportImage(const QString &source, const EditRecipe &recipe, const QString &destination,
                 const ExportOptions &options, QString *error);
bool exportImage(const QString &source, const EditRecipe &recipe, const QString &destination, QString *error);

}  // namespace ImageEngine

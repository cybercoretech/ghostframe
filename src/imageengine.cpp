#include "imageengine.h"

#include <QFileInfo>

#include <cmath>
#include <vector>

using vips::VImage;

namespace {

// Keep the colour spaces the pipeline understands; convert anything else
// (CMYK, Lab, scRGB, ...) to sRGB once, at load time.
VImage normalise(VImage image) {
    switch (image.interpretation()) {
    case VIPS_INTERPRETATION_sRGB:
    case VIPS_INTERPRETATION_RGB16:
    case VIPS_INTERPRETATION_B_W:
    case VIPS_INTERPRETATION_GREY16:
        return image;
    default:
        return image.colourspace(VIPS_INTERPRETATION_sRGB);
    }
}

double maxValue(const VImage &image) {
    return image.format() == VIPS_FORMAT_USHORT ? 65535.0 : 255.0;
}

std::vector<double> repeat(int bands, double value) { return std::vector<double>(size_t(bands), value); }

// Tone operations touch colour bands only; alpha is split off and rejoined.
VImage applyTone(const VImage &image, const EditRecipe &r) {
    const bool brightnessContrast = r.brightness != 0 || r.contrast != 0;
    const bool saturation = r.saturation != 100;
    if (!brightnessContrast && !saturation) return image;

    const bool alpha = image.has_alpha();
    const int colourBands = alpha ? image.bands() - 1 : image.bands();
    VImage colour = alpha ? image.extract_band(0, VImage::option()->set("n", colourBands)) : image;
    const VipsBandFormat format = image.format();
    const VipsInterpretation interpretation = image.interpretation();

    if (brightnessContrast) {
        // ImageMagick 7.1's -brightness-contrast, so recipes look the same as
        // exports from earlier Ghostframe versions (which ran `magick`). On
        // 0..1 values, out = slope * in + intercept, pivoting on mid-grey.
        const double contrast = qMin(r.contrast, 99.9);
        const double slope = contrast < 0 ? 1.0 + contrast / 100.0 : 100.0 / (100.0 - contrast);
        const double intercept = (r.brightness / 100.0 - 0.5) * slope + 0.5;
        colour = colour.linear(repeat(colourBands, slope), repeat(colourBands, intercept * maxValue(image)))
                     .cast(format);
    }
    if (saturation && colourBands >= 3) {
        // Scale chroma in LCh: 0% gives a neutral grey, and hue stays put.
        VImage lch = colour.colourspace(VIPS_INTERPRETATION_LCH);
        lch = lch.linear({1.0, r.saturation / 100.0, 1.0}, {0.0, 0.0, 0.0});
        colour = lch.colourspace(interpretation).cast(format);
    }
    return alpha ? colour.bandjoin(image.extract_band(colourBands)) : colour;
}

}  // namespace

namespace ImageEngine {

bool initialize(const char *argv0, QString *error) {
    if (VIPS_INIT(argv0) != 0) {
        if (error) *error = QString::fromUtf8(vips_error_buffer());
        vips_error_clear();
        return false;
    }
    return true;
}

void shutdown() { vips_shutdown(); }

QSize sourceSize(const QString &path) {
    const VImage header = VImage::new_from_file(path.toUtf8().constData());
    int orientation = 1;
    if (header.get_typeof(VIPS_META_ORIENTATION) != 0) orientation = header.get_int(VIPS_META_ORIENTATION);
    // EXIF orientations 5-8 include a 90° turn.
    return orientation >= 5 ? QSize(header.height(), header.width()) : QSize(header.width(), header.height());
}

VImage loadOriginal(const QString &path) {
    return normalise(VImage::new_from_file(path.toUtf8().constData()).autorot());
}

VImage loadProxy(const QString &path, QSize maxSize) {
    VImage proxy = VImage::thumbnail(path.toUtf8().constData(), maxSize.width(),
                                     VImage::option()->set("height", maxSize.height())->set("size", VIPS_SIZE_DOWN));
    return normalise(proxy).copy_memory();
}

VImage apply(const VImage &image, const EditRecipe &r) {
    VImage out = image;
    switch (r.rotation) {
    case 90: out = out.rot90(); break;
    case 180: out = out.rot180(); break;
    case 270: out = out.rot270(); break;
    default: break;
    }
    if (r.hasCrop) {
        const int w = out.width(), h = out.height();
        const int x = qBound(0, int(std::lround(r.cropX * w)), w - 1);
        const int y = qBound(0, int(std::lround(r.cropY * h)), h - 1);
        const int cw = qBound(1, int(std::lround(r.cropWidth * w)), w - x);
        const int ch = qBound(1, int(std::lround(r.cropHeight * h)), h - y);
        out = out.extract_area(x, y, cw, ch);
    }
    return applyTone(out, r);
}

QImage toQImage(const VImage &image) {
    VImage display = image.interpretation() == VIPS_INTERPRETATION_sRGB ? image : image.colourspace(VIPS_INTERPRETATION_sRGB);
    if (display.format() != VIPS_FORMAT_UCHAR) display = display.cast(VIPS_FORMAT_UCHAR);
    if (display.bands() > 4) display = display.extract_band(0, VImage::option()->set("n", 4));
    const int bands = display.bands();
    size_t size = 0;
    void *pixels = display.write_to_memory(&size);
    const QImage::Format format = bands == 4 ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    return QImage(static_cast<uchar *>(pixels), display.width(), display.height(), qsizetype(display.width()) * bands,
                  format, [](void *data) { g_free(data); }, pixels);
}

bool exportImage(const QString &source, const EditRecipe &recipe, const QString &destination, QString *error) {
    try {
        VImage out = apply(loadOriginal(source), recipe);
        const QString suffix = QFileInfo(destination).suffix().toLower();
        vips::VOption *options = VImage::option();
        if (suffix == "jpg" || suffix == "jpeg" || suffix == "webp") options->set("Q", 95);
        out.write_to_file(destination.toUtf8().constData(), options);
        return true;
    } catch (const vips::VError &e) {
        if (error) *error = QString::fromUtf8(e.what()).trimmed();
        return false;
    }
}

}  // namespace ImageEngine

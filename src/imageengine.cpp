#include "imageengine.h"

#include "fx.h"

#include <QColor>
#include <QFileInfo>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
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

vips::VInterpolate bilinear() { return vips::VInterpolate::new_from_name("bilinear"); }

VImage clamp01(const VImage &x) {
    return x.clamp(VImage::option()->set("min", 0.0)->set("max", 1.0));
}

// sRGB transfer function on 0..1 floats (3-band colour). Routed through
// 16-bit RGB so libvips uses its lookup tables instead of pow() per sample
// (about 5x faster on the preview proxy).
VImage toLinear(const VImage &display) {
    return (clamp01(display) * 65535.0 + 0.5).cast(VIPS_FORMAT_USHORT)
        .copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_RGB16))
        .colourspace(VIPS_INTERPRETATION_scRGB);
}
VImage toDisplay(const VImage &linear) {
    return linear.copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_scRGB))
               .colourspace(VIPS_INTERPRETATION_RGB16)
               .cast(VIPS_FORMAT_FLOAT) / 65535.0;
}
// Straighten: the largest rectangle with the frame's aspect ratio that fits
// inside the frame rotated by `degrees`, as a scale factor.
double straightenScale(double w, double h, double degrees) {
    const double t = std::abs(degrees) * std::numbers::pi / 180.0;
    const double c = std::cos(t), s = std::sin(t);
    return std::min(w / (w * c + h * s), h / (w * s + h * c));
}

// Monotone cubic (Fritsch–Carlson) interpolation: tangents are computed once,
// so evaluating it for every LUT entry stays cheap.
class MonotoneCurve {
public:
    explicit MonotoneCurve(const QList<QPointF> &points) : m_p(points) {
        const qsizetype n = m_p.size();
        if (n < 2) return;
        std::vector<double> d(static_cast<size_t>(n - 1));
        m_m.assign(static_cast<size_t>(n), 0.0);
        for (qsizetype i = 0; i < n - 1; ++i) d[size_t(i)] = (m_p[i + 1].y() - m_p[i].y()) / (m_p[i + 1].x() - m_p[i].x());
        m_m[0] = d[0];
        m_m[size_t(n - 1)] = d[size_t(n - 2)];
        for (qsizetype i = 1; i < n - 1; ++i)
            m_m[size_t(i)] = (d[size_t(i - 1)] * d[size_t(i)] <= 0) ? 0 : (d[size_t(i - 1)] + d[size_t(i)]) / 2;
        for (qsizetype i = 0; i < n - 1; ++i) {
            if (d[size_t(i)] == 0) { m_m[size_t(i)] = m_m[size_t(i + 1)] = 0; continue; }
            const double a = m_m[size_t(i)] / d[size_t(i)], b = m_m[size_t(i + 1)] / d[size_t(i)];
            const double sum = a * a + b * b;
            if (sum > 9) {
                const double t = 3 / std::sqrt(sum);
                m_m[size_t(i)] = t * a * d[size_t(i)];
                m_m[size_t(i + 1)] = t * b * d[size_t(i)];
            }
        }
    }
    bool identity() const { return m_p.size() < 2; }
    double value(double x) const {
        if (m_p.size() < 2) return x;
        if (x <= m_p.first().x()) return m_p.first().y();
        if (x >= m_p.last().x()) return m_p.last().y();
        const qsizetype n = m_p.size();
        qsizetype k = 0;
        while (k < n - 2 && x > m_p[k + 1].x()) ++k;
        const double h = m_p[k + 1].x() - m_p[k].x(), t = (x - m_p[k].x()) / h;
        const double t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * m_p[k].y() + (t3 - 2 * t2 + t) * h * m_m[size_t(k)]
               + (-2 * t3 + 3 * t2) * m_p[k + 1].y() + (t3 - t2) * h * m_m[size_t(k + 1)];
    }

private:
    QList<QPointF> m_p;
    std::vector<double> m_m;
};

// The channel-independent part of the tone curve (everything before the
// per-channel curves).
double toneBase(const EditRecipe &r, double x) {
    auto clamp = [](double v) { return std::clamp(v, 0.0, 1.0); };
    // Levels: input black/white points, then gamma.
    x = clamp((x - r.levelsBlack) / std::max(0.01, r.levelsWhite - r.levelsBlack));
    if (r.levelsGamma != 1) x = std::pow(x, 1.0 / r.levelsGamma);
    // Blacks / whites move the end points (up to 15% of the range).
    const double k = r.blacks / 100.0 * 0.15;
    if (k < 0) x = clamp((x + k) / (1 + k));
    else if (k > 0) x = k + x * (1 - k);
    const double w = r.whites / 100.0 * 0.15;
    if (w > 0) x = clamp(x / (1 - w));
    else if (w < 0) x = x * (1 + w);
    // Shadows / highlights: smooth bumps peaking at 1/3 and 2/3. |a| < 1
    // keeps the curve monotonic.
    if (r.shadows != 0) x += r.shadows / 100.0 * 0.9 * x * (1 - x) * (1 - x);
    if (r.highlights != 0) x += r.highlights / 100.0 * 0.9 * x * x * (1 - x);
    x = clamp(x);
    // ImageMagick 7.1 -brightness-contrast (see the Phase 0 notes).
    if (r.brightness != 0 || r.contrast != 0) {
        const double contrast = std::min(r.contrast, 99.9);
        const double slope = contrast < 0 ? 1.0 + contrast / 100.0 : 100.0 / (100.0 - contrast);
        x = clamp(slope * x + (r.brightness / 100.0 - 0.5) * slope + 0.5);
    }
    return x;
}

bool toneActive(const EditRecipe &r) {
    return r.levelsBlack != 0 || r.levelsWhite != 1 || r.levelsGamma != 1 || r.blacks != 0 || r.whites != 0
           || r.shadows != 0 || r.highlights != 0 || r.brightness != 0 || r.contrast != 0
           || !r.curveMaster.isEmpty() || !r.curveRed.isEmpty() || !r.curveGreen.isEmpty() || !r.curveBlue.isEmpty();
}
bool lightActive(const EditRecipe &r) { return r.temperature != 0 || r.tint != 0 || r.exposure != 0; }
bool detailActive(const EditRecipe &r) {
    return r.sharpenAmount > 0 || r.noiseLuminance > 0 || r.noiseColor > 0;
}

// ── geometry ──────────────────────────────────────────────────────────

VImage applyGeometry(VImage out, const EditRecipe &r) {
    switch (r.rotation) {
    case 90: out = out.rot90(); break;
    case 180: out = out.rot180(); break;
    case 270: out = out.rot270(); break;
    default: break;
    }
    if (r.flipHorizontal) out = out.flip(VIPS_DIRECTION_HORIZONTAL);
    if (r.flipVertical) out = out.flip(VIPS_DIRECTION_VERTICAL);

    if (r.straighten != 0) {
        const int w = out.width(), h = out.height();
        const double s = straightenScale(w, h, r.straighten);
        const VImage rotated = out.rotate(r.straighten, VImage::option()->set("interpolate", bilinear()));
        const int cw = std::max(1, int(std::floor(w * s))), ch = std::max(1, int(std::floor(h * s)));
        out = rotated.extract_area((rotated.width() - cw) / 2, (rotated.height() - ch) / 2, cw, ch);
    }

    if (r.perspectiveVertical != 0 || r.perspectiveHorizontal != 0) {
        // Keystone: scale rows (vertical) or columns (horizontal) towards the
        // centre by up to 40% at one edge. Scales are <= 1, so samples never
        // leave the image and no empty borders appear.
        const int w = out.width(), h = out.height();
        const VImage xy = VImage::xyz(w, h).cast(VIPS_FORMAT_FLOAT);
        const VImage x = xy[0], y = xy[1];
        const double cx = (w - 1) / 2.0, cy = (h - 1) / 2.0;
        auto edgeScale = [](const VImage &t, double extent, double amount) {
            const double k = std::abs(amount) / 100.0 * 0.4;
            // amount > 0 stretches the start edge (top / left), < 0 the end edge.
            return amount > 0 ? t.linear(k / std::max(1.0, extent - 1), 1.0 - k)
                              : t.linear(-k / std::max(1.0, extent - 1), 1.0);
        };
        VImage xs = x, ys = y;
        if (r.perspectiveVertical != 0) xs = (x - cx) * edgeScale(y, h, r.perspectiveVertical) + cx;
        if (r.perspectiveHorizontal != 0) ys = (y - cy) * edgeScale(x, w, r.perspectiveHorizontal) + cy;
        out = out.mapim(xs.bandjoin(ys), VImage::option()->set("interpolate", bilinear()));
    }

    if (r.hasCrop) {
        const int w = out.width(), h = out.height();
        const int x = qBound(0, int(std::lround(r.cropX * w)), w - 1);
        const int y = qBound(0, int(std::lround(r.cropY * h)), h - 1);
        const int cw = qBound(1, int(std::lround(r.cropWidth * w)), w - x);
        const int ch = qBound(1, int(std::lround(r.cropHeight * h)), h - y);
        out = out.extract_area(x, y, cw, ch);
    }
    return out;
}

// ── light + tone: one per-channel lookup table ───────────────────────
//
// White balance, exposure, levels, blacks/whites, shadows/highlights,
// brightness/contrast and curves are all per-channel functions of the input
// value, so they compose into a single table applied straight to the 8- or
// 16-bit pixels. Output is 16-bit, which keeps precision for later stages.

// Linear-light gain per channel (white balance with luminance kept, times
// exposure). Grey images get exposure only.
std::array<double, 3> lightGains(const EditRecipe &r, bool rgb) {
    const double ev = std::pow(2.0, r.exposure);
    if (!rgb) return {ev, ev, ev};
    const double t = r.temperature / 100.0, m = r.tint / 100.0;
    const double gr = std::exp(0.35 * t), gg = std::exp(-0.3 * m), gb = std::exp(-0.35 * t);
    const double luminance = 0.2126 * gr + 0.7152 * gg + 0.0722 * gb;
    return {gr * ev / luminance, gg * ev / luminance, gb * ev / luminance};
}

double srgbToLinear(double x) { return x <= 0.04045 ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4); }
double linearToSrgb(double x) { return x <= 0.0031308 ? x * 12.92 : 1.055 * std::pow(x, 1.0 / 2.4) - 0.055; }

// Only the fields the table depends on; equal keys share a table.
EditRecipe lutKey(const EditRecipe &r) {
    EditRecipe k;
    k.temperature = r.temperature; k.tint = r.tint; k.exposure = r.exposure;
    k.levelsBlack = r.levelsBlack; k.levelsWhite = r.levelsWhite; k.levelsGamma = r.levelsGamma;
    k.blacks = r.blacks; k.whites = r.whites; k.shadows = r.shadows; k.highlights = r.highlights;
    k.brightness = r.brightness; k.contrast = r.contrast;
    k.curveMaster = r.curveMaster; k.curveRed = r.curveRed; k.curveGreen = r.curveGreen; k.curveBlue = r.curveBlue;
    return k;
}

VImage buildLut(const EditRecipe &r, int entries, int bands) {
    const bool light = r.temperature != 0 || r.tint != 0 || r.exposure != 0;
    const auto gains = lightGains(r, bands >= 3);
    const MonotoneCurve master(r.curveMaster);
    const MonotoneCurve channels[3] = {MonotoneCurve(r.curveRed), MonotoneCurve(r.curveGreen), MonotoneCurve(r.curveBlue)};
    std::vector<quint16> data(size_t(entries) * size_t(bands));
    for (int i = 0; i < entries; ++i) {
        const double in = double(i) / (entries - 1);
        for (int b = 0; b < bands; ++b) {
            double x = in;
            if (light) x = std::clamp(linearToSrgb(gains[size_t(std::min(b, 2))] * srgbToLinear(x)), 0.0, 1.0);
            x = std::clamp(master.value(toneBase(r, x)), 0.0, 1.0);
            if (bands >= 3 && b < 3) x = std::clamp(channels[b].value(x), 0.0, 1.0);
            data[size_t(i) * size_t(bands) + size_t(b)] = quint16(std::lround(x * 65535.0));
        }
    }
    // _copy: libvips owns its own buffer (plain new_from_memory would point at
    // `data`, which dies with this function).
    return VImage::new_from_memory_copy(data.data(), data.size() * sizeof(quint16), entries, 1, bands, VIPS_FORMAT_USHORT);
}

// Slider drags re-render many times with the same light/tone settings (e.g.
// while only saturation moves), so keep the last table.
VImage cachedLut(const EditRecipe &r, int entries, int bands) {
    static QMutex mutex;
    static EditRecipe key;
    static int keyEntries = 0, keyBands = 0;
    static VImage lut;
    const EditRecipe wanted = lutKey(r);
    QMutexLocker lock(&mutex);
    if (keyEntries != entries || keyBands != bands || !(key == wanted)) {
        lut = buildLut(r, entries, bands);
        key = wanted; keyEntries = entries; keyBands = bands;
    }
    return lut;
}

// ── colour (LCh) ──────────────────────────────────────────────────────

// LCh hue (degrees) at the centre of each HSL band, in band order.
constexpr double kHslCentres[EditRecipe::HslBandCount] = {30, 62, 98, 138, 196, 258, 298, 336};
constexpr double kHslWidth = 42;  // fade to zero this many degrees from the centre

// Vibrance and saturation as blends against luminance on 0..1 display
// values: 0% saturation is an exact neutral grey.
VImage applySaturation(const VImage &d, const EditRecipe &r) {
    constexpr double wr = 0.2126, wg = 0.7152, wb = 0.0722;
    const double s = r.saturation / 100.0;
    if (r.vibrance == 0) {
        // A constant blend is a 3×3 colour matrix: one cheap recomb, no
        // per-pixel band broadcasting.
        const double k = 1.0 - s;
        return d.recomb(VImage::new_matrixv(3, 3,
                                            s + k * wr, k * wg, k * wb,
                                            k * wr, s + k * wg, k * wb,
                                            k * wr, k * wg, s + k * wb));
    }
    // Vibrance: muted colours (small max - min) move most, saturated ones
    // barely, so the factor varies per pixel.
    const VImage y = d.recomb(VImage::new_matrixv(3, 1, wr, wg, wb));
    const VImage chroma = d[0].maxpair(d[1]).maxpair(d[2]) - d[0].minpair(d[1]).minpair(d[2]);
    VImage t = chroma / 0.6;
    t = (t > 1.0).ifthenelse(1.0, t);
    const VImage factor = (1.0 - t).linear(r.vibrance / 100.0 * s, s);
    return y + (d - y) * factor;
}

bool hslActive(const EditRecipe &r) {
    return std::any_of(r.hsl.begin(), r.hsl.end(), [](const EditRecipe::HslBand &b) { return b != EditRecipe::HslBand{}; });
}

// HSL bands in LCh (needs real hue angles). Input and output: 0..1 display.
VImage applyHsl(const VImage &display, const EditRecipe &r) {
    const VImage lch = toLinear(display).colourspace(VIPS_INTERPRETATION_LCH);
    VImage L = lch[0], C = lch[1], h = lch[2];
    VImage hueShift = C * 0.0, satScale = C * 0.0, lumShift = C * 0.0;
    VImage chromaWeight = C / 25.0;
    chromaWeight = (chromaWeight > 1.0).ifthenelse(1.0, chromaWeight);
    for (int i = 0; i < EditRecipe::HslBandCount; ++i) {
        const EditRecipe::HslBand &band = r.hsl[size_t(i)];
        if (band == EditRecipe::HslBand{}) continue;
        const VImage distance = (((h - kHslCentres[i] + 540.0) % 360.0) - 180.0).abs();
        VImage w = 1.0 - distance / kHslWidth;
        w = (w < 0.0).ifthenelse(0.0, w);
        w = w * w * (3.0 - w * 2.0);  // smoothstep
        if (band.hue != 0) hueShift = hueShift + w * (band.hue / 100.0 * 30.0);
        if (band.saturation != 0) satScale = satScale + w * (band.saturation / 100.0);
        if (band.luminance != 0) lumShift = lumShift + w * chromaWeight * (band.luminance / 100.0 * 15.0);
    }
    h = h + hueShift;
    C = C * (1.0 + satScale);
    C = (C < 0.0).ifthenelse(0.0, C);
    L = L + lumShift;
    const VImage out = L.bandjoin(C).bandjoin(h).copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_LCH));
    return toDisplay(out.colourspace(VIPS_INTERPRETATION_scRGB));
}

// ── detail ────────────────────────────────────────────────────────────

VImage blur(const VImage &x, double sigma) {
    return x.gaussblur(std::max(0.3, sigma), VImage::option()->set("precision", VIPS_PRECISION_FLOAT));
}

VImage applyDetail(VImage display, const EditRecipe &r, double scale) {
    const bool colour = display.bands() >= 3;
    if (r.noiseColor > 0 && colour) {
        // Blur chroma (colour difference from the mean) but not luminance.
        const VImage y = display.bandmean();
        const VImage chroma = display - y;
        const VImage smooth = blur(chroma, (1.0 + r.noiseColor / 100.0 * 4.0) * scale);
        display = y + chroma + (smooth - chroma) * (r.noiseColor / 100.0);
    }
    if (r.noiseLuminance > 0) {
        // Edge-aware: smooth flat areas, keep edges.
        const VImage smooth = blur(display, (0.6 + r.noiseLuminance / 100.0 * 2.0) * scale);
        VImage edges = display.bandmean().sobel() / 0.15;
        edges = (edges > 1.0).ifthenelse(1.0, edges);
        display = display + (smooth - display) * ((1.0 - edges) * (r.noiseLuminance / 100.0));
    }
    if (r.sharpenAmount > 0) {
        // Unsharp mask. Per channel (no band broadcasting); at these radii
        // colour fringing is negligible.
        VImage detail = display - blur(display, r.sharpenRadius * scale);
        if (r.sharpenThreshold > 0) {
            const double t = r.sharpenThreshold / 100.0 * 0.1;
            detail = (detail.abs() > t).ifthenelse(detail, 0.0);
        }
        display = display + detail * (r.sharpenAmount / 100.0);
    }
    return display;
}

// Everything after geometry. Alpha is split off and rejoined; the result has
// the input's format and interpretation, clipped (never wrapped).
VImage applyAdjustments(VImage image, const EditRecipe &r, double scale) {
    const bool lut = lightActive(r) || toneActive(r);
    const bool saturation = r.saturation != 100 || r.vibrance != 0;
    const bool hsl = hslActive(r), detail = detailActive(r);
    const bool fx = r.hasFx(), text = !r.textLayers.isEmpty();
    if (!lut && !saturation && !hsl && !detail && !fx && !text) return image;

    // FX and text are colour: promote grey images once.
    if ((fx || text) && image.bands() - (image.has_alpha() ? 1 : 0) < 3)
        image = image.colourspace(image.format() == VIPS_FORMAT_USHORT ? VIPS_INTERPRETATION_RGB16 : VIPS_INTERPRETATION_sRGB);

    const bool alpha = image.has_alpha();
    const int colourBands = alpha ? image.bands() - 1 : image.bands();
    const bool rgb = colourBands >= 3;
    const VipsInterpretation interpretation = image.interpretation();
    const bool sixteen = image.format() == VIPS_FORMAT_USHORT;
    VImage colour = alpha ? image.extract_band(0, VImage::option()->set("n", colourBands)) : image;

    // 1. Light + tone: integer table, 16-bit out.
    VImage u16;
    if (lut) u16 = colour.maplut(cachedLut(r, sixteen ? 65536 : 256, colourBands));
    else u16 = sixteen ? colour : colour.cast(VIPS_FORMAT_USHORT) * 257;

    // 2-3. Colour and detail on 0..1 floats, only if needed.
    if (((saturation || hsl) && rgb) || detail || fx || text) {
        VImage d = u16.cast(VIPS_FORMAT_FLOAT) / 65535.0;
        if (saturation && rgb) d = applySaturation(d, r);
        if (hsl && rgb) d = applyHsl(d, r);
        if (detail) d = applyDetail(d, r, scale);
        // 4. Cyber FX, then text layers (whole frame; see Fx).
        if (fx) d = Fx::apply(clamp01(d), r, scale);
        if (text) d = Fx::applyText(clamp01(d), r.textLayers);
        u16 = (clamp01(d) * 65535.0 + 0.5).cast(VIPS_FORMAT_USHORT);
    }

    VImage out = sixteen ? u16 : (u16.cast(VIPS_FORMAT_FLOAT) / 257.0 + 0.5).cast(VIPS_FORMAT_UCHAR);
    out = out.copy(VImage::option()->set("interpretation", interpretation));
    return alpha ? out.bandjoin(image.extract_band(colourBands)) : out;
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

QSize geometryFrame(QSize size, const EditRecipe &r) {
    double w = size.width(), h = size.height();
    if (r.rotation == 90 || r.rotation == 270) std::swap(w, h);
    if (r.straighten != 0) {
        const double s = straightenScale(w, h, r.straighten);
        w = std::max(1.0, std::floor(w * s));
        h = std::max(1.0, std::floor(h * s));
    }
    return QSize(int(w), int(h));
}

VImage apply(const VImage &image, const EditRecipe &r, const ApplyOptions &options) {
    VImage out = applyAdjustments(applyGeometry(image, r), r, options.scale);
    // The region is cut last: effects depend on the whole frame, and libvips
    // only computes the pixels (plus filter margins) the region needs.
    if (options.region.isValid()) {
        const int w = out.width(), h = out.height();
        const int x = qBound(0, int(std::floor(options.region.x() * w)), w - 1);
        const int y = qBound(0, int(std::floor(options.region.y() * h)), h - 1);
        const int rw = qBound(1, int(std::ceil(options.region.width() * w)), w - x);
        const int rh = qBound(1, int(std::ceil(options.region.height() * h)), h - y);
        out = out.extract_area(x, y, rw, rh);
    }
    return out;
}

double curveValue(const QList<QPointF> &points, double x) { return MonotoneCurve(points).value(x); }

double toneValue(const EditRecipe &r, int channel, double x) {
    x = std::clamp(MonotoneCurve(r.curveMaster).value(toneBase(r, x)), 0.0, 1.0);
    const QList<QPointF> *curves[] = {&r.curveRed, &r.curveGreen, &r.curveBlue};
    if (channel >= 0 && channel < 3) x = MonotoneCurve(*curves[channel]).value(x);
    return std::clamp(x, 0.0, 1.0);
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

VImage clippingOverlay(const VImage &image) {
    VImage display = image.interpretation() == VIPS_INTERPRETATION_sRGB ? image : image.colourspace(VIPS_INTERPRETATION_sRGB);
    if (display.format() != VIPS_FORMAT_UCHAR) display = display.cast(VIPS_FORMAT_UCHAR);
    const bool alpha = display.has_alpha();
    const VImage rgb = alpha ? display.extract_band(0, VImage::option()->set("n", 3)) : display;
    const VImage high = (rgb >= 255.0).bandor();
    const VImage low = (rgb <= 0.0).bandand();
    const VImage painted = high.ifthenelse(std::vector<double>{255, 54, 168},
                                           low.ifthenelse(std::vector<double>{53, 244, 255}, rgb))
                               .cast(VIPS_FORMAT_UCHAR).copy(VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
    return alpha ? painted.bandjoin(display.extract_band(3)) : painted;
}

QImage histogramImage(const VImage &image, QSize size) {
    VImage display = image.interpretation() == VIPS_INTERPRETATION_sRGB ? image : image.colourspace(VIPS_INTERPRETATION_sRGB);
    if (display.format() != VIPS_FORMAT_UCHAR) display = display.cast(VIPS_FORMAT_UCHAR);
    const VImage rgb = display.extract_band(0, VImage::option()->set("n", 3));
    const VImage luma = rgb.colourspace(VIPS_INTERPRETATION_B_W);
    const VImage hist = rgb.bandjoin(luma).hist_find().cast(VIPS_FORMAT_DOUBLE);  // 256×1, 4 bands
    size_t bytes = 0;
    auto *counts = static_cast<double *>(hist.write_to_memory(&bytes));
    double peak = 1;
    // Ignore the end bins when scaling, so clipped pixels don't flatten the rest.
    for (int i = 1; i < 255; ++i)
        for (int b = 0; b < 4; ++b) peak = std::max(peak, counts[i * 4 + b]);

    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    const QColor colours[4] = {QColor(255, 54, 168, 110), QColor(180, 243, 58, 110), QColor(53, 244, 255, 110),
                               QColor(237, 241, 244, 70)};
    for (int b = 0; b < 4; ++b) {
        QPainterPath path;
        path.moveTo(0, size.height());
        for (int i = 0; i < 256; ++i) {
            const double v = std::min(1.0, std::sqrt(counts[i * 4 + b] / peak));  // sqrt: shows small counts
            path.lineTo(i * (size.width() - 1) / 255.0, size.height() - v * (size.height() - 2));
        }
        path.lineTo(size.width() - 1, size.height());
        path.closeSubpath();
        painter.setCompositionMode(b == 3 ? QPainter::CompositionMode_SourceOver : QPainter::CompositionMode_Plus);
        painter.fillPath(path, colours[b]);
    }
    painter.end();
    g_free(counts);
    return out;
}

VImage fitToSize(const VImage &image, const ExportOptions &o) {
    if (!o.size.isValid() || o.size.isEmpty()) return image;
    const int tw = o.size.width(), th = o.size.height();
    const double sx = double(tw) / image.width(), sy = double(th) / image.height();
    auto scaled = [&](double s) { return image.resize(s, VImage::option()->set("kernel", VIPS_KERNEL_LANCZOS3)); };
    auto centreCrop = [&](const VImage &big) {
        return big.extract_area(std::max(0, (big.width() - tw) / 2), std::max(0, (big.height() - th) / 2),
                                std::min(tw, big.width()), std::min(th, big.height()));
    };
    if (o.fit == ExportOptions::Fill) return centreCrop(scaled(std::max(sx, sy)));
    const VImage front = scaled(std::min(sx, sy));
    const int left = (tw - front.width()) / 2, top = (th - front.height()) / 2;
    if (o.fit == ExportOptions::FitPad) {
        const QColor c("#" + o.padColor);
        std::vector<double> background{double(c.red()), double(c.green()), double(c.blue())};
        if (image.format() == VIPS_FORMAT_USHORT) for (auto &v : background) v *= 257;
        if (image.has_alpha()) background.push_back(image.format() == VIPS_FORMAT_USHORT ? 65535 : 255);
        background.resize(size_t(image.bands()), background.back());
        return front.embed(left, top, tw, th, VImage::option()->set("extend", VIPS_EXTEND_BACKGROUND)->set("background", background));
    }
    // FitBlur: the frame filled with a blurred, darkened copy (vertical
    // Shorts from landscape photos without black bars).
    VImage back = centreCrop(scaled(std::max(sx, sy)));
    const VipsBandFormat format = back.format();
    back = (back.gaussblur(std::max(8.0, tw / 40.0)) * 0.55).cast(format);
    return back.insert(front, left, top);
}

bool exportImage(const QString &source, const EditRecipe &recipe, const QString &destination, QString *error) {
    return exportImage(source, recipe, destination, ExportOptions{}, error);
}

bool exportImage(const QString &source, const EditRecipe &recipe, const QString &destination,
                 const ExportOptions &exportOptions, QString *error) {
    try {
        VImage out = fitToSize(apply(loadOriginal(source), recipe), exportOptions);
        const QString suffix = QFileInfo(destination).suffix().toLower();
        if ((suffix == "jpg" || suffix == "jpeg") && out.has_alpha()) out = out.flatten();
        vips::VOption *options = VImage::option();
        const int quality = exportOptions.quality > 0 ? exportOptions.quality : 95;
        if (suffix == "jpg" || suffix == "jpeg" || suffix == "webp" || suffix == "avif" || suffix == "heic")
            options->set("Q", quality);
        out.write_to_file(destination.toUtf8().constData(), options);
        return true;
    } catch (const vips::VError &e) {
        if (error) *error = QString::fromUtf8(e.what()).trimmed();
        return false;
    }
}

}  // namespace ImageEngine

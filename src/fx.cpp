#include "fx.h"

#include "cubelut.h"

#include <QColor>
#include <QtConcurrent/QtConcurrentMap>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

using vips::VImage;

namespace {

constexpr double kLumaR = 0.2126, kLumaG = 0.7152, kLumaB = 0.0722;

VImage clamp01(const VImage &x) { return x.clamp(VImage::option()->set("min", 0.0)->set("max", 1.0)); }
VImage luma(const VImage &rgb) { return rgb.recomb(VImage::new_matrixv(3, 1, kLumaR, kLumaG, kLumaB)); }
VImage blur(const VImage &x, double sigma) {
    return x.gaussblur(std::max(0.3, sigma), VImage::option()->set("precision", VIPS_PRECISION_FLOAT));
}
// A 1-band image as 3 identical bands (explicit, so later maths stays 3-band).
VImage triple(const VImage &one) { return one.bandjoin(one).bandjoin(one); }

// Interleaved float pixels for effects that need plain loops (LUT, sorting).
struct Pixels {
    int width = 0, height = 0;
    std::vector<float> data;
};
Pixels toPixels(const VImage &rgb) {
    Pixels p;
    const VImage f = rgb.cast(VIPS_FORMAT_FLOAT);
    p.width = f.width();
    p.height = f.height();
    size_t bytes = 0;
    auto *raw = static_cast<float *>(f.write_to_memory(&bytes));
    p.data.assign(raw, raw + bytes / sizeof(float));
    g_free(raw);
    return p;
}
VImage fromPixels(const Pixels &p) {
    return VImage::new_from_memory_copy(const_cast<float *>(p.data.data()), p.data.size() * sizeof(float),
                                        p.width, p.height, 3, VIPS_FORMAT_FLOAT);
}
// Runs `rowFn(row)` over all rows on the thread pool.
template <typename Fn> void forRows(int rows, Fn rowFn) {
    std::vector<int> index(static_cast<size_t>(rows));
    std::iota(index.begin(), index.end(), 0);
    QtConcurrent::blockingMap(index, [&](int y) { rowFn(y); });
}

VImage mix(const VImage &from, const VImage &to, double amount) { return from + (to - from) * amount; }

// Resizes to exactly w×h (resize can land a pixel short).
VImage toSize(const VImage &image, int w, int h) {
    VImage out = image.resize(double(w) / image.width(),
                              VImage::option()->set("vscale", double(h) / image.height())->set("kernel", VIPS_KERNEL_LINEAR));
    if (out.width() != w || out.height() != h)
        out = out.embed(0, 0, std::max(w, out.width()), std::max(h, out.height()), VImage::option()->set("extend", VIPS_EXTEND_COPY))
                  .extract_area(0, 0, w, h);
    return out;
}

// A wide blur done at reduced size (soft results look the same, and cost a
// fraction of a full-size kernel).
VImage softBlur(const VImage &x, double sigma) {
    const double factor = std::clamp(sigma / 3.0, 1.0, 16.0);
    if (factor <= 1.0) return blur(x, sigma);
    const int w = x.width(), h = x.height();
    const VImage small = x.resize(1.0 / factor, VImage::option()->set("kernel", VIPS_KERNEL_LINEAR));
    return toSize(blur(small, sigma / factor), w, h);
}

// ── effects ───────────────────────────────────────────────────────────

VImage gradientMap(const VImage &d, const EditRecipe &r) {
    const VImage index = (clamp01(luma(d)) * 255.0 + 0.5).cast(VIPS_FORMAT_UCHAR);
    return mix(d, index.maplut(Fx::gradientTable(r.gradientColors)), r.gradientAmount / 100.0);
}

VImage cubeLut(const VImage &d, const EditRecipe &r) {
    const auto lut = LutLibrary::load(r.lutName);
    if (!lut) return d;  // a missing LUT renders as no LUT (the UI reports it)
    Pixels p = toPixels(clamp01(d));
    const size_t rowPixels = size_t(p.width);
    forRows(p.height, [&](int y) { lut->apply(p.data.data() + size_t(y) * rowPixels * 3, rowPixels); });
    return mix(d, fromPixels(p), r.lutAmount / 100.0);
}

VImage bloom(const VImage &d, const EditRecipe &r, double scale) {
    // Bright pass (colour kept), blurred, then screened back on: a neon glow.
    const double t = r.bloomThreshold / 100.0 * 0.95;
    VImage mask = (luma(d) - t) / (1.0 - t);
    mask = clamp01(mask);
    const VImage glow = softBlur(d * triple(mask), r.bloomRadius * scale) * (r.bloomAmount / 100.0 * 2.5);
    return 1.0 - (1.0 - d) * clamp01(1.0 - glow);
}

VImage chromaticAberration(const VImage &d, const EditRecipe &r) {
    // Red scaled up and blue scaled down around the centre (lens-like).
    const int w = d.width(), h = d.height();
    const double k = r.aberrationAmount / 100.0 * 0.012;
    auto scaledAboutCentre = [&](const VImage &band, double factor) {
        // Centre the resized band in w×h. Each dimension may come out
        // larger or smaller (rounding), so pad to cover, then cut.
        const VImage s = band.resize(factor, VImage::option()->set("kernel", VIPS_KERNEL_LINEAR));
        const int cw = std::max(w, s.width()), ch = std::max(h, s.height());
        const VImage covered = s.embed((cw - s.width()) / 2, (ch - s.height()) / 2, cw, ch,
                                       VImage::option()->set("extend", VIPS_EXTEND_COPY));
        return covered.extract_area((cw - w) / 2, (ch - h) / 2, w, h);
    };
    return scaledAboutCentre(d[0], 1.0 + k).bandjoin(d[1]).bandjoin(scaledAboutCentre(d[2], 1.0 - k));
}

VImage glitch(VImage d, const EditRecipe &r) {
    // Horizontal bands shifted sideways (wrapping), some with a red/blue
    // split. Positions are fractions of the frame, so preview == export.
    const int w = d.width(), h = d.height();
    std::mt19937 rng(uint32_t(r.glitchSeed) * 2654435761u + 17u);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    const double strength = r.glitchAmount / 100.0;
    const int bands = 2 + int(strength * 14);
    for (int i = 0; i < bands; ++i) {
        const double y0 = unit(rng), height = 0.004 + unit(rng) * 0.06 * (0.4 + strength);
        const double shift = (unit(rng) - 0.5) * 0.25 * strength;
        const bool split = unit(rng) < 0.5;
        const int top = std::clamp(int(y0 * h), 0, h - 1);
        const int rows = std::clamp(int(height * h), 1, h - top);
        const int dx = int(std::lround(shift * w));
        VImage band = d.extract_area(0, top, w, rows);
        if (dx != 0) band = band.wrap(VImage::option()->set("x", ((dx % w) + w) % w)->set("y", 0));
        if (split) {
            const int sx = std::max(1, int(std::lround(w * 0.006 * (0.5 + strength))));
            const VImage red = band[0].wrap(VImage::option()->set("x", sx)->set("y", 0));
            const VImage blue = band[2].wrap(VImage::option()->set("x", w - sx)->set("y", 0));
            band = red.bandjoin(band[1]).bandjoin(blue);
        }
        d = d.insert(band, 0, top);
    }
    return d;
}

VImage pixelSort(const VImage &d, const EditRecipe &r) {
    // Within each row (or column), runs of pixels brighter than a threshold
    // are sorted by brightness: the classic "melting" glitch.
    const VImage source = r.pixelSortVertical ? d.rot90() : d;
    Pixels p = toPixels(clamp01(source));
    const float threshold = float(1.0 - r.pixelSortAmount / 100.0 * 0.9);
    forRows(p.height, [&](int y) {
        float *row = p.data.data() + size_t(y) * size_t(p.width) * 3;
        auto lum = [](const float *px) { return float(kLumaR * px[0] + kLumaG * px[1] + kLumaB * px[2]); };
        int x = 0;
        while (x < p.width) {
            while (x < p.width && lum(row + x * 3) < threshold) ++x;
            const int start = x;
            while (x < p.width && lum(row + x * 3) >= threshold) ++x;
            if (x - start < 2) continue;
            std::vector<std::array<float, 3>> run(size_t(x - start));
            for (int i = start; i < x; ++i) run[size_t(i - start)] = {row[i * 3], row[i * 3 + 1], row[i * 3 + 2]};
            std::sort(run.begin(), run.end(), [&](const auto &a, const auto &b) { return lum(a.data()) < lum(b.data()); });
            for (int i = start; i < x; ++i) for (int c = 0; c < 3; ++c) row[i * 3 + c] = run[size_t(i - start)][size_t(c)];
        }
    });
    const VImage sorted = fromPixels(p);
    return r.pixelSortVertical ? sorted.rot270() : sorted;
}

VImage halftone(const VImage &d, const EditRecipe &r, double scale) {
    // Round dots on a square grid; dot area follows the cell's darkness.
    const int w = d.width(), h = d.height();
    const int cell = std::max(2, int(std::lround(r.halftoneSize * scale)));
    // Average brightness per cell, back up to full size (nearest).
    const VImage small = luma(d).shrink(cell, cell);
    VImage cells = small.zoom(cell, cell);
    cells = cells.embed(0, 0, w, h, VImage::option()->set("extend", VIPS_EXTEND_COPY)).extract_area(0, 0, w, h);
    const VImage xy = VImage::xyz(w, h).cast(VIPS_FORMAT_FLOAT);
    const double half = cell / 2.0;
    const VImage fx = (xy[0] % double(cell)) - half, fy = (xy[1] % double(cell)) - half;
    const VImage distance = (fx * fx + fy * fy).pow(0.5);
    const VImage radius = (1.0 - clamp01(cells)).pow(0.5) * (cell * 0.72);
    const VImage ink = distance < radius;  // 255 where the dot is
    const VImage printed = ink.ifthenelse(d * 0.12, clamp01(d * 1.08 + 0.04));
    return mix(d, printed, r.halftoneAmount / 100.0);
}

VImage scanlines(const VImage &d, const EditRecipe &r, double scale) {
    const int w = d.width(), h = d.height();
    const double spacing = std::max(2.0, r.scanlineSpacing * scale);
    const VImage row = VImage::xyz(w, h)[1].cast(VIPS_FORMAT_FLOAT);
    const VImage dark = (row % spacing) < (spacing / 2.0);
    const double depth = r.scanlineAmount / 100.0 * 0.6;
    return d * triple(dark.ifthenelse(1.0 - depth, 1.0));
}

VImage grain(const VImage &d, const EditRecipe &r, double scale) {
    const int w = d.width(), h = d.height();
    const double size = std::max(1.0, r.grainSize * scale);
    const int gw = std::max(1, int(std::ceil(w / size))), gh = std::max(1, int(std::ceil(h / size)));
    VImage noise = VImage::gaussnoise(gw, gh, VImage::option()->set("sigma", 1.0)->set("mean", 0.0)->set("seed", 42));
    if (gw != w || gh != h)
        noise = noise.resize(double(w) / gw, VImage::option()->set("vscale", double(h) / gh)->set("kernel", VIPS_KERNEL_LINEAR))
                    .embed(0, 0, w, h, VImage::option()->set("extend", VIPS_EXTEND_COPY)).extract_area(0, 0, w, h);
    // Strongest in the midtones, like film.
    const VImage y = luma(d);
    const VImage weight = y * (1.0 - y) * 4.0;
    return d + triple(noise * weight * (r.grainAmount / 100.0 * 0.11));
}

VImage vignette(const VImage &d, const EditRecipe &r) {
    // The mask is smooth, so compute it small and enlarge it.
    const int w = d.width(), h = d.height();
    const int mw = std::max(2, w / 8), mh = std::max(2, h / 8);
    const VImage xy = VImage::xyz(mw, mh).cast(VIPS_FORMAT_FLOAT);
    const double cx = (mw - 1) / 2.0, cy = (mh - 1) / 2.0;
    const VImage nx = (xy[0] - cx) / std::max(1.0, cx), ny = (xy[1] - cy) / std::max(1.0, cy);
    const VImage radius = ((nx * nx + ny * ny) * 0.5).pow(0.5);  // 0 centre → 1 corners
    const double start = r.vignetteMidpoint / 100.0 * 0.85;
    VImage t = clamp01((radius - start) / std::max(0.05, 1.0 - start));
    t = toSize(t * t * (3.0 - t * 2.0), w, h);  // smoothstep
    const double a = r.vignetteAmount / 100.0;
    return a < 0 ? d * triple(1.0 + t * a) : d + (1.0 - d) * triple(t * a);
}

}  // namespace

namespace Fx {

VImage gradientTable(const QStringList &colours) {
    std::vector<QColor> stops;
    for (const QString &hex : colours) stops.emplace_back(QColor("#" + hex));
    if (stops.empty()) stops = {QColor(Qt::black), QColor(Qt::white)};
    if (stops.size() == 1) stops.insert(stops.begin(), QColor(Qt::black));
    std::vector<float> table(256 * 3);
    for (int i = 0; i < 256; ++i) {
        const double pos = i / 255.0 * double(stops.size() - 1);
        const int a = std::min(int(pos), int(stops.size()) - 2);
        const double f = pos - a;
        const QColor &c0 = stops[size_t(a)], &c1 = stops[size_t(a + 1)];
        table[size_t(i) * 3 + 0] = float(c0.redF() + (c1.redF() - c0.redF()) * f);
        table[size_t(i) * 3 + 1] = float(c0.greenF() + (c1.greenF() - c0.greenF()) * f);
        table[size_t(i) * 3 + 2] = float(c0.blueF() + (c1.blueF() - c0.blueF()) * f);
    }
    return VImage::new_from_memory_copy(table.data(), table.size() * sizeof(float), 256, 1, 3, VIPS_FORMAT_FLOAT);
}

VImage apply(const VImage &rgb, const EditRecipe &r, double scale) {
    VImage d = rgb;
    if (r.gradientAmount > 0 && r.gradientColors.size() >= 2) d = gradientMap(d, r);
    if (!r.lutName.isEmpty() && r.lutAmount > 0) d = cubeLut(d, r);
    if (r.bloomAmount > 0) d = bloom(d, r, scale);
    if (r.aberrationAmount > 0) d = chromaticAberration(d, r);
    if (r.glitchAmount > 0) d = glitch(d, r);
    if (r.pixelSortAmount > 0) d = pixelSort(d, r);
    if (r.halftoneAmount > 0) d = halftone(d, r, scale);
    if (r.scanlineAmount > 0) d = scanlines(d, r, scale);
    if (r.grainAmount > 0) d = grain(d, r, scale);
    if (r.vignetteAmount != 0) d = vignette(d, r);
    return d;
}

VImage applyText(const VImage &rgb, const QList<EditRecipe::TextLayer> &layers) {
    VImage d = rgb;
    const int w = d.width(), h = d.height();
    for (const auto &t : layers) {
        if (t.text.trimmed().isEmpty() || t.opacity <= 0) continue;
        // Pango sizes are in points; at 72 dpi a point is a pixel.
        const int px = std::max(4, int(std::lround(t.size * h)));
        const QString markup = QString("<span foreground=\"#%1\">%2</span>").arg(t.color, t.text.toHtmlEscaped());
        const QString font = QString("%1%2 %3").arg(t.font, t.bold ? " Bold" : "").arg(px);
        VImage text;
        try {
            text = VImage::text(markup.toUtf8().constData(),
                                VImage::option()->set("font", font.toUtf8().constData())->set("rgba", true)->set("dpi", 72));
        } catch (const vips::VError &) {
            continue;  // unrenderable text is skipped rather than failing the whole render
        }
        if (text.width() < 1 || text.height() < 1) continue;
        const int tw = std::min(text.width(), w), th = std::min(text.height(), h);
        text = text.extract_area(0, 0, tw, th).cast(VIPS_FORMAT_FLOAT) / 255.0;
        const int margin = int(std::lround(t.margin * h));
        const int col = t.anchor % 3, row = t.anchor / 3;
        const int x = col == 0 ? margin : col == 1 ? (w - tw) / 2 : w - tw - margin;
        const int y = row == 0 ? margin : row == 1 ? (h - th) / 2 : h - th - margin;
        const int left = std::clamp(x, 0, w - tw), top = std::clamp(y, 0, h - th);
        // Place on a full-frame transparent canvas.
        const VImage placed = text.embed(left, top, w, h);
        const VImage colour = placed.extract_band(0, VImage::option()->set("n", 3));
        VImage alpha = placed[3] * t.opacity;
        if (t.glow > 0) {
            // Neon glow: the text's own colour, blurred and screened underneath.
            // Only the area around the text is blurred.
            const double sigma = std::max(1.0, px * (0.08 + t.glow / 100.0 * 0.35));
            const int pad = int(std::ceil(sigma * 3));
            const int gx = std::max(0, left - pad), gy = std::max(0, top - pad);
            const int gw = std::min(w, left + tw + pad) - gx, gh = std::min(h, top + th + pad) - gy;
            const VImage local = (colour * triple(alpha)).extract_area(gx, gy, gw, gh);
            const VImage halo = softBlur(local, sigma).embed(gx, gy, w, h) * (t.glow / 100.0 * 2.2);
            d = 1.0 - (1.0 - d) * clamp01(1.0 - halo);
        }
        alpha = triple(alpha);
        d = d * (1.0 - alpha) + colour * alpha;
    }
    return d;
}

}  // namespace Fx

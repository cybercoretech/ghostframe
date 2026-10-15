#pragma once

#include <QJsonObject>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVariantMap>

#include <array>

// The complete, non-destructive description of an edit. Preview and export
// both render from a recipe, and it is stored in the export's XMP sidecar
// (and per photo in the RecipeStore) so an edit can be reopened.
//
// Render order (see ImageEngine::apply):
//   geometry  rotate 90° → flip → straighten → perspective → crop
//   light     white balance + exposure (linear light)
//   tone      levels → blacks/whites → shadows/highlights → brightness/contrast
//             → curves (one per-channel lookup table)
//   colour    vibrance → saturation → HSL bands (LCh)
//   detail    noise reduction → sharpen
//   fx        gradient map → LUT → bloom → chromatic aberration → glitch →
//             pixel sort → halftone → scanlines → grain → vignette
//   text      text / watermark layers, last
struct EditRecipe {
    static constexpr int Version = 1;

    struct HslBand {
        double hue = 0, saturation = 0, luminance = 0;  // each -100..100
        friend bool operator==(const HslBand &, const HslBand &) = default;
    };
    static constexpr int HslBandCount = 8;
    // Band names, in hue order. Used in JSON and in QML parameter keys.
    static const std::array<const char *, HslBandCount> &hslBandNames();

    // A text or watermark layer, positioned by anchor (0..8 = top-left …
    // bottom-right, row by row) and a margin; sizes are fractions of the
    // output height, so they hold at any resolution.
    struct TextLayer {
        QString text;
        QString font = QStringLiteral("JetBrains Mono");
        QString color = QStringLiteral("35f4ff");  // hex, no '#'
        double size = 0.06;      // text height / output height, 0.01..0.5
        int anchor = 8;          // bottom-right
        double margin = 0.04;    // fraction of the output height
        double opacity = 1.0;    // 0..1
        double glow = 0;         // 0..100, neon glow in the text colour
        bool bold = true;
        friend bool operator==(const TextLayer &, const TextLayer &) = default;
    };

    // ── geometry ──
    int rotation = 0;              // clockwise degrees: 0, 90, 180, 270
    bool flipHorizontal = false, flipVertical = false;
    double straighten = 0;         // degrees, -45..45, clockwise; auto-crops the corners
    double perspectiveVertical = 0, perspectiveHorizontal = 0;  // -100..100 keystone
    bool hasCrop = false;
    double cropX = 0, cropY = 0, cropWidth = 1, cropHeight = 1;  // 0..1 of the frame after the steps above

    // ── light ──
    double temperature = 0, tint = 0;  // -100..100: cooler/warmer, green/magenta
    double exposure = 0;               // EV, -5..5
    double highlights = 0, shadows = 0, whites = 0, blacks = 0;  // -100..100
    double brightness = 0;             // -100..100, ImageMagick 7.1 -brightness-contrast semantics
    double contrast = 0;               // -100..100
    double levelsBlack = 0, levelsWhite = 1, levelsGamma = 1;  // input black/white 0..1, gamma 0.1..10
    // Point curves on 0..1; empty means identity. Interpolated monotonically.
    QList<QPointF> curveMaster, curveRed, curveGreen, curveBlue;

    // ── colour ──
    double vibrance = 0;           // -100..100, favours muted colours
    double saturation = 100;       // percent, 0..200
    std::array<HslBand, HslBandCount> hsl{};

    // ── fx ── (pixel sizes are at full resolution)
    // Gradient map: luminance → these colours (hex, dark to light), e.g.
    // generated from a CYBERGRID palette; name is for display only.
    QStringList gradientColors;
    QString gradientName;
    double gradientAmount = 0;      // 0..100
    QString lutName;                // a .cube file in the LUT library
    double lutAmount = 100;         // 0..100
    double bloomAmount = 0, bloomThreshold = 70, bloomRadius = 20;  // 0..100, 0..100, 1..120 px
    double aberrationAmount = 0;    // 0..100
    double glitchAmount = 0, glitchSeed = 1;  // 0..100, 0..999
    double pixelSortAmount = 0;     // 0..100: how much of the tonal range gets sorted
    bool pixelSortVertical = false;
    double halftoneAmount = 0, halftoneSize = 8;  // 0..100, 3..60 px
    double scanlineAmount = 0, scanlineSpacing = 4;  // 0..100, 2..24 px
    double grainAmount = 0, grainSize = 1.5;  // 0..100, 0.5..4 px
    double vignetteAmount = 0, vignetteMidpoint = 50;  // -100..100, 0..100
    QList<TextLayer> textLayers;

    // ── detail ── (pixel radii are at full resolution)
    double sharpenAmount = 0;      // 0..300 %
    double sharpenRadius = 1.0;    // 0.5..3 px
    double sharpenThreshold = 0;   // 0..100
    double noiseLuminance = 0, noiseColor = 0;  // 0..100

    bool isIdentity() const { return *this == EditRecipe{}; }
    bool hasFx() const;
    bool hasGeometry() const;
    // Everything except geometry is at its default (for "before" views).
    EditRecipe geometryOnly() const;
    // A "look": everything except geometry and text layers.
    QJsonObject lookJson() const;
    // Applies a look's ops on top of this recipe's geometry and text.
    EditRecipe withLook(const QJsonObject &look) const;

    // {"version":1,"ops":[{"op":"rotate","degrees":90}, ...]}; identity ops are omitted.
    QJsonObject toJson() const;
    QString toJsonString() const;
    // Unknown ops are ignored and out-of-range values are clamped, so a recipe
    // written by a newer Ghostframe still opens. Returns false only for
    // malformed input or a newer major version.
    static bool fromJson(const QJsonObject &json, EditRecipe *out);

    // Flat parameter map for QML: "exposure", "hsl.red.hue", "curve.master"
    // (a list of [x, y] pairs), ...
    QVariantMap toVariantMap() const;
    // Sets one parameter by its QML key, with clamping. Returns false for an
    // unknown key.
    bool setParameter(const QString &key, const QVariant &value);

    friend bool operator==(const EditRecipe &, const EditRecipe &) = default;
};

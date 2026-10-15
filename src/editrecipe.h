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
struct EditRecipe {
    static constexpr int Version = 1;

    struct HslBand {
        double hue = 0, saturation = 0, luminance = 0;  // each -100..100
        friend bool operator==(const HslBand &, const HslBand &) = default;
    };
    static constexpr int HslBandCount = 8;
    // Band names, in hue order. Used in JSON and in QML parameter keys.
    static const std::array<const char *, HslBandCount> &hslBandNames();

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

    // ── detail ── (pixel radii are at full resolution)
    double sharpenAmount = 0;      // 0..300 %
    double sharpenRadius = 1.0;    // 0.5..3 px
    double sharpenThreshold = 0;   // 0..100
    double noiseLuminance = 0, noiseColor = 0;  // 0..100

    bool isIdentity() const { return *this == EditRecipe{}; }
    bool hasGeometry() const;
    // Everything except geometry is at its default (for "before" views).
    EditRecipe geometryOnly() const;

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

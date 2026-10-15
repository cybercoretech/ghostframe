#pragma once

#include <QJsonObject>
#include <QString>

// The complete, non-destructive description of an edit. Preview and export
// both render from a recipe, and it is stored in the export's XMP sidecar so
// an edit can be reopened. Geometry runs before tone: rotate, then crop
// (normalised to the rotated frame), then brightness/contrast, then saturation.
struct EditRecipe {
    static constexpr int Version = 1;

    int rotation = 0;            // clockwise degrees: 0, 90, 180, 270
    bool hasCrop = false;
    double cropX = 0, cropY = 0, cropWidth = 1, cropHeight = 1;  // 0..1
    double brightness = 0;       // -100..100, ImageMagick 7.1 -brightness-contrast semantics
    double contrast = 0;         // -100..100
    double saturation = 100;     // percent, 0..200

    bool isIdentity() const { return *this == EditRecipe{}; }

    // {"version":1,"ops":[{"op":"rotate","degrees":90}, ...]}; identity ops are omitted.
    QJsonObject toJson() const;
    QString toJsonString() const;
    // Unknown ops are ignored and out-of-range values are clamped, so a recipe
    // written by a newer Ghostframe still opens. Returns false only for
    // malformed input or a newer major version.
    static bool fromJson(const QJsonObject &json, EditRecipe *out);

    friend bool operator==(const EditRecipe &, const EditRecipe &) = default;
};

#include "editrecipe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtGlobal>

#include <algorithm>

namespace {

// Every plain numeric parameter, in render order. One table drives JSON
// (op + field), the QML map (key) and clamping, so adding an adjustment is
// one line here plus its rendering.
struct Field {
    const char *key;    // QML parameter key
    const char *op;     // JSON op name; consecutive fields share an op
    const char *field;  // JSON field inside the op
    double EditRecipe::*member;
    double min, max, fallback;
};

const Field kFields[] = {
    {"straighten", "straighten", "angle", &EditRecipe::straighten, -45, 45, 0},
    {"perspectiveVertical", "perspective", "vertical", &EditRecipe::perspectiveVertical, -100, 100, 0},
    {"perspectiveHorizontal", "perspective", "horizontal", &EditRecipe::perspectiveHorizontal, -100, 100, 0},
    {"temperature", "whiteBalance", "temperature", &EditRecipe::temperature, -100, 100, 0},
    {"tint", "whiteBalance", "tint", &EditRecipe::tint, -100, 100, 0},
    {"exposure", "exposure", "ev", &EditRecipe::exposure, -5, 5, 0},
    {"levelsBlack", "levels", "black", &EditRecipe::levelsBlack, 0, 0.99, 0},
    {"levelsWhite", "levels", "white", &EditRecipe::levelsWhite, 0.01, 1, 1},
    {"levelsGamma", "levels", "gamma", &EditRecipe::levelsGamma, 0.1, 10, 1},
    {"blacks", "tone", "blacks", &EditRecipe::blacks, -100, 100, 0},
    {"whites", "tone", "whites", &EditRecipe::whites, -100, 100, 0},
    {"shadows", "tone", "shadows", &EditRecipe::shadows, -100, 100, 0},
    {"highlights", "tone", "highlights", &EditRecipe::highlights, -100, 100, 0},
    {"brightness", "brightnessContrast", "brightness", &EditRecipe::brightness, -100, 100, 0},
    {"contrast", "brightnessContrast", "contrast", &EditRecipe::contrast, -100, 100, 0},
    {"vibrance", "vibrance", "amount", &EditRecipe::vibrance, -100, 100, 0},
    {"saturation", "saturation", "percent", &EditRecipe::saturation, 0, 200, 100},
    {"noiseLuminance", "noiseReduction", "luminance", &EditRecipe::noiseLuminance, 0, 100, 0},
    {"noiseColor", "noiseReduction", "color", &EditRecipe::noiseColor, 0, 100, 0},
    {"sharpenAmount", "sharpen", "amount", &EditRecipe::sharpenAmount, 0, 300, 0},
    {"sharpenRadius", "sharpen", "radius", &EditRecipe::sharpenRadius, 0.5, 3, 1},
    {"sharpenThreshold", "sharpen", "threshold", &EditRecipe::sharpenThreshold, 0, 100, 0},
};

const char *kCurveNames[] = {"master", "red", "green", "blue"};

QList<QPointF> EditRecipe::*curveMember(int i) {
    static QList<QPointF> EditRecipe::*members[] = {&EditRecipe::curveMaster, &EditRecipe::curveRed,
                                                    &EditRecipe::curveGreen, &EditRecipe::curveBlue};
    return members[i];
}

// Points clamped to 0..1, sorted by x, duplicates in x dropped. A curve that
// is just the two default endpoints collapses to empty (identity).
QList<QPointF> normaliseCurve(QList<QPointF> points) {
    for (auto &p : points) p = QPointF(qBound(0.0, p.x(), 1.0), qBound(0.0, p.y(), 1.0));
    std::sort(points.begin(), points.end(), [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
    QList<QPointF> out;
    for (const auto &p : points)
        if (out.isEmpty() || p.x() - out.last().x() > 1e-4) out.append(p);
    if (out.size() < 2) return {};
    if (out.size() == 2 && out[0] == QPointF(0, 0) && out[1] == QPointF(1, 1)) return {};
    return out;
}

QJsonArray curveToJson(const QList<QPointF> &curve) {
    QJsonArray array;
    for (const auto &p : curve) array.append(QJsonArray{p.x(), p.y()});
    return array;
}

QList<QPointF> curveFromVariant(const QVariant &value) {
    QList<QPointF> points;
    for (const auto &item : value.toList()) {
        if (item.canConvert<QPointF>() && item.typeId() == QMetaType::QPointF) { points.append(item.toPointF()); continue; }
        const QVariantList pair = item.toList();
        if (pair.size() == 2) { points.append(QPointF(pair[0].toDouble(), pair[1].toDouble())); continue; }
        const QVariantMap map = item.toMap();
        if (map.contains("x") && map.contains("y")) points.append(QPointF(map["x"].toDouble(), map["y"].toDouble()));
    }
    return normaliseCurve(points);
}

}  // namespace

const std::array<const char *, EditRecipe::HslBandCount> &EditRecipe::hslBandNames() {
    static const std::array<const char *, HslBandCount> names{"red", "orange", "yellow", "green",
                                                              "aqua", "blue", "purple", "magenta"};
    return names;
}

bool EditRecipe::hasGeometry() const {
    return rotation || flipHorizontal || flipVertical || straighten != 0 || perspectiveVertical != 0
           || perspectiveHorizontal != 0 || hasCrop;
}

EditRecipe EditRecipe::geometryOnly() const {
    EditRecipe r;
    r.rotation = rotation;
    r.flipHorizontal = flipHorizontal; r.flipVertical = flipVertical;
    r.straighten = straighten;
    r.perspectiveVertical = perspectiveVertical; r.perspectiveHorizontal = perspectiveHorizontal;
    r.hasCrop = hasCrop; r.cropX = cropX; r.cropY = cropY; r.cropWidth = cropWidth; r.cropHeight = cropHeight;
    return r;
}

QJsonObject EditRecipe::toJson() const {
    QJsonArray ops;
    auto emitFields = [&](const char *opName) {
        QJsonObject op{{"op", opName}};
        bool changed = false;
        for (const auto &f : kFields) {
            if (qstrcmp(f.op, opName) != 0) continue;
            op.insert(f.field, this->*f.member);
            changed |= this->*f.member != f.fallback;
        }
        if (changed) ops.append(op);
    };

    if (rotation) ops.append(QJsonObject{{"op", "rotate"}, {"degrees", rotation}});
    if (flipHorizontal || flipVertical)
        ops.append(QJsonObject{{"op", "flip"}, {"horizontal", flipHorizontal}, {"vertical", flipVertical}});
    emitFields("straighten");
    emitFields("perspective");
    if (hasCrop)
        ops.append(QJsonObject{{"op", "crop"}, {"x", cropX}, {"y", cropY}, {"width", cropWidth}, {"height", cropHeight}});
    emitFields("whiteBalance");
    emitFields("exposure");
    emitFields("levels");
    emitFields("tone");
    emitFields("brightnessContrast");
    QJsonObject curves{{"op", "curves"}};
    bool anyCurve = false;
    for (int i = 0; i < 4; ++i) {
        const auto &curve = this->*curveMember(i);
        if (curve.isEmpty()) continue;
        curves.insert(kCurveNames[i], curveToJson(curve));
        anyCurve = true;
    }
    if (anyCurve) ops.append(curves);
    emitFields("vibrance");
    emitFields("saturation");
    QJsonObject bands;
    for (int i = 0; i < HslBandCount; ++i) {
        const HslBand &b = hsl[size_t(i)];
        if (b == HslBand{}) continue;
        bands.insert(hslBandNames()[size_t(i)], QJsonObject{{"hue", b.hue}, {"saturation", b.saturation}, {"luminance", b.luminance}});
    }
    if (!bands.isEmpty()) ops.append(QJsonObject{{"op", "hsl"}, {"bands", bands}});
    emitFields("noiseReduction");
    emitFields("sharpen");
    return {{"version", Version}, {"ops", ops}};
}

QString EditRecipe::toJsonString() const {
    return QString::fromUtf8(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
}

bool EditRecipe::fromJson(const QJsonObject &json, EditRecipe *out) {
    if (!out || json.value("version").toInt(0) < 1 || json.value("version").toInt() > Version) return false;
    if (!json.value("ops").isArray()) return false;
    EditRecipe r;
    for (const auto &value : json.value("ops").toArray()) {
        const QJsonObject op = value.toObject();
        const QString name = op.value("op").toString();
        if (name == "rotate") {
            const int degrees = ((op.value("degrees").toInt() % 360) + 360) % 360;
            r.rotation = degrees - degrees % 90;
        } else if (name == "flip") {
            r.flipHorizontal = op.value("horizontal").toBool();
            r.flipVertical = op.value("vertical").toBool();
        } else if (name == "crop") {
            r.cropX = qBound(0.0, op.value("x").toDouble(), 1.0);
            r.cropY = qBound(0.0, op.value("y").toDouble(), 1.0);
            r.cropWidth = qBound(0.0, op.value("width").toDouble(1), 1.0 - r.cropX);
            r.cropHeight = qBound(0.0, op.value("height").toDouble(1), 1.0 - r.cropY);
            r.hasCrop = r.cropWidth > 0 && r.cropHeight > 0;
        } else if (name == "curves") {
            for (int i = 0; i < 4; ++i)
                r.*curveMember(i) = curveFromVariant(op.value(kCurveNames[i]).toVariant());
        } else if (name == "hsl") {
            const QJsonObject bands = op.value("bands").toObject();
            for (int i = 0; i < HslBandCount; ++i) {
                const QJsonObject b = bands.value(hslBandNames()[size_t(i)]).toObject();
                r.hsl[size_t(i)] = {qBound(-100.0, b.value("hue").toDouble(), 100.0),
                                    qBound(-100.0, b.value("saturation").toDouble(), 100.0),
                                    qBound(-100.0, b.value("luminance").toDouble(), 100.0)};
            }
        } else {
            for (const auto &f : kFields)
                if (name == QLatin1String(f.op) && op.contains(f.field))
                    r.*f.member = qBound(f.min, op.value(f.field).toDouble(f.fallback), f.max);
        }
    }
    if (r.levelsWhite <= r.levelsBlack) { r.levelsBlack = 0; r.levelsWhite = 1; }
    *out = r;
    return true;
}

QVariantMap EditRecipe::toVariantMap() const {
    QVariantMap map;
    for (const auto &f : kFields) map.insert(f.key, this->*f.member);
    map.insert("rotation", rotation);
    map.insert("flipHorizontal", flipHorizontal);
    map.insert("flipVertical", flipVertical);
    map.insert("hasCrop", hasCrop);
    map.insert("cropX", cropX); map.insert("cropY", cropY);
    map.insert("cropWidth", cropWidth); map.insert("cropHeight", cropHeight);
    for (int i = 0; i < 4; ++i) {
        QVariantList points;
        for (const auto &p : this->*curveMember(i)) points.append(QVariant(QVariantList{p.x(), p.y()}));
        map.insert(QString("curve.") + kCurveNames[i], points);
    }
    for (int i = 0; i < HslBandCount; ++i) {
        const QString band = QString("hsl.") + hslBandNames()[size_t(i)];
        map.insert(band + ".hue", hsl[size_t(i)].hue);
        map.insert(band + ".saturation", hsl[size_t(i)].saturation);
        map.insert(band + ".luminance", hsl[size_t(i)].luminance);
    }
    return map;
}

bool EditRecipe::setParameter(const QString &key, const QVariant &value) {
    for (const auto &f : kFields) {
        if (key != QLatin1String(f.key)) continue;
        this->*f.member = qBound(f.min, value.toDouble(), f.max);
        if (levelsWhite <= levelsBlack + 0.01) {
            if (key == "levelsBlack") levelsBlack = levelsWhite - 0.01;
            else levelsWhite = levelsBlack + 0.01;
        }
        return true;
    }
    if (key == "flipHorizontal") { flipHorizontal = value.toBool(); return true; }
    if (key == "flipVertical") { flipVertical = value.toBool(); return true; }
    if (key.startsWith("curve.")) {
        for (int i = 0; i < 4; ++i)
            if (key == QString("curve.") + kCurveNames[i]) { this->*curveMember(i) = curveFromVariant(value); return true; }
        return false;
    }
    if (key.startsWith("hsl.")) {
        const QStringList parts = key.split('.');
        if (parts.size() != 3) return false;
        for (int i = 0; i < HslBandCount; ++i) {
            if (parts[1] != QLatin1String(hslBandNames()[size_t(i)])) continue;
            HslBand &b = hsl[size_t(i)];
            const double v = qBound(-100.0, value.toDouble(), 100.0);
            if (parts[2] == "hue") b.hue = v;
            else if (parts[2] == "saturation") b.saturation = v;
            else if (parts[2] == "luminance") b.luminance = v;
            else return false;
            return true;
        }
    }
    return false;
}

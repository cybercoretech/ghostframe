#include "editrecipe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtGlobal>

QJsonObject EditRecipe::toJson() const {
    QJsonArray ops;
    if (rotation) ops.append(QJsonObject{{"op", "rotate"}, {"degrees", rotation}});
    if (hasCrop)
        ops.append(QJsonObject{{"op", "crop"}, {"x", cropX}, {"y", cropY}, {"width", cropWidth}, {"height", cropHeight}});
    if (brightness != 0 || contrast != 0)
        ops.append(QJsonObject{{"op", "brightnessContrast"}, {"brightness", brightness}, {"contrast", contrast}});
    if (saturation != 100) ops.append(QJsonObject{{"op", "saturation"}, {"percent", saturation}});
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
        } else if (name == "crop") {
            r.cropX = qBound(0.0, op.value("x").toDouble(), 1.0);
            r.cropY = qBound(0.0, op.value("y").toDouble(), 1.0);
            r.cropWidth = qBound(0.0, op.value("width").toDouble(1), 1.0 - r.cropX);
            r.cropHeight = qBound(0.0, op.value("height").toDouble(1), 1.0 - r.cropY);
            r.hasCrop = r.cropWidth > 0 && r.cropHeight > 0;
        } else if (name == "brightnessContrast") {
            r.brightness = qBound(-100.0, op.value("brightness").toDouble(), 100.0);
            r.contrast = qBound(-100.0, op.value("contrast").toDouble(), 100.0);
        } else if (name == "saturation") {
            r.saturation = qBound(0.0, op.value("percent").toDouble(100), 200.0);
        }
    }
    *out = r;
    return true;
}

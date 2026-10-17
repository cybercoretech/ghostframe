#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVersionNumber>

// Ghostframe's bridge to the cybermeta CLI (cybercoretech/cybermeta), its
// metadata engine. Everything is synchronous and thread-safe: call it from
// worker threads. Needs cybermeta >= 0.2.0 (show --summary, xmp write).
namespace Cybermeta {

// What happens to the camera metadata (EXIF) of an export.
enum class Privacy {
    Keep,       // the original EXIF, unchanged
    StripGps,   // everything except location (the default)
    Anonymous,  // no location, no camera/lens/owner identity
    StripAll,   // structural tags only
};
Privacy privacyFromString(const QString &value);  // "keep" | "gps" | "anonymous" | "all"
QString privacyLabel(Privacy privacy);

// The cybermeta binary: $GHOSTFRAME_CYBERMETA, then PATH, ~/.local/bin,
// ~/.cargo/bin. Empty if not found.
QString executable();
QVersionNumber version();  // null if missing or unreadable
bool available();          // found and new enough

// `cybermeta show <file> --summary` as a map (camera, lens, shutter, iso,
// gps {latitude, longitude, map_url}, …). Empty with `error` on failure.
QVariantMap summary(const QString &imagePath, QString *error = nullptr);

struct XmpContent {
    QString title, description;
    QStringList keywords;
    QMap<QString, QString> fields;  // cybermeta:NAME → value
    QJsonObject recipe;             // empty = none
};
// `cybermeta xmp write -o -`: a complete XMP packet.
QByteArray xmpPacket(const XmpContent &content, QString *error = nullptr);
// Writes the packet to `path` atomically (via cybermeta -o).
bool writeSidecar(const QString &path, const XmpContent &content, QString *error = nullptr);

// Applies `privacy` (and, if non-empty, an EXIF ImageDescription) to an
// EXIF block, returning the cleaned block. Works for every output format:
// the block travels in a tiny JPEG "carrier" that cybermeta edits.
// StripAll with no description returns an empty block (no EXIF at all).
QByteArray cleanExif(const QByteArray &exif, Privacy privacy, const QString &description, QString *error = nullptr);

}  // namespace Cybermeta

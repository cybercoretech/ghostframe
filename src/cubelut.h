#pragma once

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

// A 3D colour lookup table from an Adobe/Resolve .cube file, applied with
// trilinear interpolation.
struct CubeLut {
    int size = 0;                   // N: the table is N×N×N
    float domainMin[3] = {0, 0, 0};
    float domainMax[3] = {1, 1, 1};
    std::vector<float> table;       // r fastest, then g, then b; 3 floats each

    // Parses .cube text. Returns false (with a message) for anything that
    // isn't a valid 3D LUT (1D LUTs are not supported).
    static bool parse(const QByteArray &text, CubeLut *out, QString *error);
    // In-place on interleaved RGB floats in 0..1.
    void apply(float *rgb, size_t pixels) const;
};

// The user's LUT folder (<AppData>/luts by default). LUTs are referenced by
// file name in recipes; parsed tables are cached by name + modification time.
namespace LutLibrary {
void setDirectory(const QString &path);
QString directory();
QStringList names();
// nullptr if missing or invalid.
std::shared_ptr<const CubeLut> load(const QString &name, QString *error = nullptr);
// Copies a .cube file into the library (validating it first). Returns the
// stored name, or empty with `error` set.
QString import(const QString &path, QString *error);
}  // namespace LutLibrary

#pragma once
// OoT3D asset converter: ZAR ("ZAR\1", OoT3D) and GAR ("GAR\2", MM3D) archives.

#include "Oot3dBinary.h"

#include <string>
#include <vector>

namespace Oot3d {

struct ZarFile {
    std::string name; // e.g. "model/gi_sword_1.cmb", "anim/wait.csab"
    View data;        // into the archive's buffer
};

// Keeps the archive bytes alive; files are views into them.
struct Zar {
    Bytes bytes;
    std::vector<ZarFile> files;

    static Zar Parse(Bytes bytes);
    const ZarFile* Find(const std::string& name) const; // case-insensitive, exact path
    std::vector<const ZarFile*> WithExtension(const std::string& ext) const; // ".cmb"
};

// "model/foo_bar.cmb" -> "foo_bar"
std::string StemOf(const std::string& path);

} // namespace Oot3d

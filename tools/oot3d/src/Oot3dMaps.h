#pragma once
// OoT3D asset converter: room geometry (see Oot3dMaps.cpp).

#include "Oot3dExtract.h"
#include "Oot3dTexture.h"
#include "Oot3dZip.h"

#include <atomic>
#include <mutex>
#include <set>
#include <string>

namespace Oot3d {

class RomFS;

// Texture resources shared by everything the mod writes, deduplicated by content and logical size.
class TextureStore {
  public:
    explicit TextureStore(ZipWriter& zip);
    // Returns the resource path of `img` loaded as a logicalW x logicalH RGBA16 N64 texture.
    std::string Add(const Image& img, uint32_t logicalW, uint32_t logicalH);
    uint32_t Count() const {
        return mCount;
    }

  private:
    ZipWriter& mZip;
    std::mutex mLock;
    std::set<std::string> mWritten;
    std::atomic<uint32_t> mCount{ 0 };
};

struct MapsResult {
    uint32_t converted = 0, skipped = 0, warnings = 0;
};

MapsResult ConvertMaps(const RomFS& rom, const ZipReader& oot, ZipWriter& zip, TextureStore& textures,
                       const ExtractOptions& opt);

struct ZsiEnvironment;
struct SceneLight;
SceneLight LightFromEnvironment(const ZsiEnvironment* env);

} // namespace Oot3d

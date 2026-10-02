#pragma once

#include "lambui_export.h"
#include <boost/optional.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace LambUI {

struct TextureAtlasRegion {
    size_t id = 0;
    int x = 0, y = 0, width = 0, height = 0;
    float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
};

class LAMBUI_API TextureAtlas {
public:
    TextureAtlas(int width, int height);
    ~TextureAtlas();
    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    boost::optional<TextureAtlasRegion> AddImage(int width, int height, const std::vector<uint8_t>& rgba);
    bool UpdateImage(size_t id, const std::vector<uint8_t>& rgba);
    boost::optional<TextureAtlasRegion> GetRegion(size_t id) const;
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    const std::vector<uint8_t>& GetPixels() const { return m_pixels; }
    uint64_t GetRevision() const { return m_revision; }

private:
    void WriteImage(const TextureAtlasRegion& region, const std::vector<uint8_t>& rgba);
    int m_width, m_height;
    int m_nextX = 0, m_nextY = 0, m_rowHeight = 0;
    uint64_t m_revision = 1;
    std::vector<uint8_t> m_pixels;
    std::vector<TextureAtlasRegion> m_regions;
};

} // namespace LambUI
#include "LambUI/UITextureAtlas.h"
#include "LambUI/UILog.h"
#include <algorithm>
#include <stdexcept>

namespace {
constexpr const char* TAG = "TextureAtlas";
}

namespace LambUI {

TextureAtlas::TextureAtlas(int width, int height) : m_width(width), m_height(height) {
    LAMBUI_LOGT(TAG, "Construct({}x{})", width, height);
    if (width < 3 || height < 3 || static_cast<size_t>(width) > m_pixels.max_size() / 4 / height)
        throw std::invalid_argument("TextureAtlas dimensions must fit an RGBA page with a one-pixel gutter");
    m_pixels.resize(static_cast<size_t>(width) * height * 4, 0);
}

TextureAtlas::~TextureAtlas() {
    LAMBUI_LOGT(TAG, "Destroy");
}

Optional<TextureAtlasRegion> TextureAtlas::AddImage(int width, int height, const std::vector<uint8_t>& rgba) {
    LAMBUI_LOGT(TAG, "AddImage({}x{})", width, height);
    if (width <= 0 || height <= 0 || width > m_width - 2 || height > m_height - 2 ||
        rgba.size() != static_cast<size_t>(width) * height * 4) return {};
    int nextX = m_nextX;
    int nextY = m_nextY;
    int rowHeight = m_rowHeight;
    if (width + 2 > m_width - nextX) {
        nextX = 0;
        nextY += rowHeight;
        rowHeight = 0;
    }
    if (height + 2 > m_height - nextY) return {};
    TextureAtlasRegion region;
    region.id = m_regions.size();
    region.x = nextX + 1;
    region.y = nextY + 1;
    region.width = width;
    region.height = height;
    region.u0 = static_cast<float>(region.x) / m_width;
    region.v0 = static_cast<float>(region.y) / m_height;
    region.u1 = static_cast<float>(region.x + width) / m_width;
    region.v1 = static_cast<float>(region.y + height) / m_height;
    m_regions.push_back(region);
    WriteImage(region, rgba);
    m_nextX = nextX + width + 2;
    m_nextY = nextY;
    m_rowHeight = std::max(rowHeight, height + 2);
    ++m_revision;
    return region;
}

bool TextureAtlas::UpdateImage(size_t id, const std::vector<uint8_t>& rgba) {
    LAMBUI_LOGT(TAG, "UpdateImage({})", id);
    if (id >= m_regions.size()) return false;
    const auto& region = m_regions[id];
    const size_t rowBytes = static_cast<size_t>(region.width) * 4;
    if (rgba.size() != rowBytes * region.height) return false;
    bool changed = false;
    for (int row = 0; row < region.height && !changed; ++row) {
        const size_t source = static_cast<size_t>(row) * rowBytes;
        const size_t target = (static_cast<size_t>(region.y + row) * m_width + region.x) * 4;
        changed = !std::equal(rgba.begin() + source, rgba.begin() + source + rowBytes, m_pixels.begin() + target);
    }
    if (changed) {
        WriteImage(region, rgba);
        ++m_revision;
    }
    return true;
}

Optional<TextureAtlasRegion> TextureAtlas::GetRegion(size_t id) const {
    if (id >= m_regions.size()) return {};
    return m_regions[id];
}

void TextureAtlas::WriteImage(const TextureAtlasRegion& region, const std::vector<uint8_t>& rgba) {
    LAMBUI_LOGT(TAG, "WriteImage({})", region.id);
    for (int row = -1; row <= region.height; ++row) {
        const int sourceRow = std::max(0, std::min(row, region.height - 1));
        for (int column = -1; column <= region.width; ++column) {
            const int sourceColumn = std::max(0, std::min(column, region.width - 1));
            const size_t source = (static_cast<size_t>(sourceRow) * region.width + sourceColumn) * 4;
            const size_t target = (static_cast<size_t>(region.y + row) * m_width + region.x + column) * 4;
            std::copy_n(rgba.begin() + source, 4, m_pixels.begin() + target);
        }
    }
}

} // namespace LambUI
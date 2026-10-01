#include "LambUI/UIFontAtlas.h"
#include "LambUI/UILog.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace LambUI {

namespace {
constexpr const char* TAG = "FontAtlas";

struct RawGlyph {
    char32_t codepoint = 0;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    int advanceWidth = 0;
};

constexpr int kFirstChar = 32;
constexpr int kLastChar = 126;
constexpr int kAtlasWidth = 512;
constexpr int kGlyphPad = 1;
} // namespace

bool FontAtlas::LoadFromFile(const std::string& ttfPath, int pixelHeight, int padding) {
    std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
    if (!file) {
        LAMBUI_LOGE(TAG, "failed to open font file: {}", ttfPath);
        return false;
    }

    const std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> fileData(static_cast<size_t>(fileSize));
    if (!file.read(reinterpret_cast<char*>(fileData.data()), fileSize)) {
        LAMBUI_LOGE(TAG, "failed to read font file: {}", ttfPath);
        return false;
    }

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fileData.data(), stbtt_GetFontOffsetForIndex(fileData.data(), 0))) {
        LAMBUI_LOGE(TAG, "stbtt_InitFont failed to parse: {}", ttfPath);
        return false;
    }

    LAMBUI_LOGD(TAG, "baking SDF atlas from '{}' at {}px (padding {})", ttfPath, pixelHeight, padding);

    const float scale = stbtt_ScaleForPixelHeight(&font, static_cast<float>(pixelHeight));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    m_ascent = static_cast<float>(ascent) * scale;
    m_lineHeight = static_cast<float>(ascent - descent + lineGap) * scale;

    const uint8_t onEdgeValue = 180;
    const float pixelDistScale = static_cast<float>(onEdgeValue) / static_cast<float>(padding);
    m_onEdgeValue = onEdgeValue;
    m_pixelDistanceScale = pixelDistScale;

    std::vector<RawGlyph> rawGlyphs;
    rawGlyphs.reserve(kLastChar - kFirstChar + 1);
    for (int cp = kFirstChar; cp <= kLastChar; ++cp) {
        RawGlyph glyph;
        glyph.codepoint = static_cast<char32_t>(cp);
        glyph.pixels = stbtt_GetCodepointSDF(&font, scale, cp, padding, onEdgeValue,
                                              pixelDistScale, &glyph.width, &glyph.height,
                                              &glyph.xoff, &glyph.yoff);
        int leftBearing = 0;
        stbtt_GetCodepointHMetrics(&font, cp, &glyph.advanceWidth, &leftBearing);
        rawGlyphs.push_back(glyph);
    }

    // Pass 1: shelf-pack to determine the required atlas height.
    int penX = 0, penY = 0, rowHeight = 0;
    for (const auto& g : rawGlyphs) {
        if (g.width <= 0 || g.height <= 0) continue;
        if (penX + g.width + kGlyphPad > kAtlasWidth) {
            penX = 0;
            penY += rowHeight + kGlyphPad;
            rowHeight = 0;
        }
        rowHeight = std::max(rowHeight, g.height);
        penX += g.width + kGlyphPad;
    }
    m_atlasWidth = kAtlasWidth;
    m_atlasHeight = std::max(penY + rowHeight + kGlyphPad, 1);
    m_atlasPixels.assign(static_cast<size_t>(m_atlasWidth) * static_cast<size_t>(m_atlasHeight), 0);

    // Pass 2: blit each glyph's SDF bitmap into the atlas and record its UVs.
    penX = 0; penY = 0; rowHeight = 0;
    m_glyphs.clear();
    for (auto& g : rawGlyphs) {
        const bool hasBitmap = g.width > 0 && g.height > 0;
        if (hasBitmap && penX + g.width + kGlyphPad > kAtlasWidth) {
            penX = 0;
            penY += rowHeight + kGlyphPad;
            rowHeight = 0;
        }

        GlyphInfo info;
        info.width = static_cast<float>(g.width);
        info.height = static_cast<float>(g.height);
        info.bearingX = static_cast<float>(g.xoff);
        info.bearingY = static_cast<float>(g.yoff);
        info.advance = static_cast<float>(g.advanceWidth) * scale;

        if (hasBitmap) {
            for (int row = 0; row < g.height; ++row) {
                std::memcpy(&m_atlasPixels[static_cast<size_t>(penY + row) * m_atlasWidth + penX],
                            g.pixels + static_cast<size_t>(row) * g.width,
                            static_cast<size_t>(g.width));
            }
            info.u0 = static_cast<float>(penX) / static_cast<float>(m_atlasWidth);
            info.v0 = static_cast<float>(penY) / static_cast<float>(m_atlasHeight);
            info.u1 = static_cast<float>(penX + g.width) / static_cast<float>(m_atlasWidth);
            info.v1 = static_cast<float>(penY + g.height) / static_cast<float>(m_atlasHeight);

            rowHeight = std::max(rowHeight, g.height);
            penX += g.width + kGlyphPad;
        }

        m_glyphs.emplace(g.codepoint, info);
        if (g.pixels) stbtt_FreeSDF(g.pixels, nullptr);
    }

    LAMBUI_LOGI(TAG, "baked {} glyphs into a {}x{} atlas", m_glyphs.size(), m_atlasWidth, m_atlasHeight);
    return true;
}

const GlyphInfo* FontAtlas::FindGlyph(char32_t codepoint) const {
    auto it = m_glyphs.find(codepoint);
    return it != m_glyphs.end() ? &it->second : nullptr;
}

FontAtlasTextMeasurer::FontAtlasTextMeasurer(const FontAtlas& atlas) : m_atlas(atlas) {
    LAMBUI_LOGT(TAG, "FontAtlasTextMeasurer constructed");
}

FontAtlasTextMeasurer::~FontAtlasTextMeasurer() {
    LAMBUI_LOGT(TAG, "FontAtlasTextMeasurer destroyed");
}

bool FontAtlasTextMeasurer::RegisterFont(void* fontHandle, const FontAtlas& atlas) {
    LAMBUI_LOGT(TAG, "RegisterFont({})", fmt::ptr(fontHandle));
    if (!fontHandle) return false;
    return m_fonts.emplace(fontHandle, &atlas).second;
}

const FontAtlas& FontAtlasTextMeasurer::GetFont(void* fontHandle) const {
    const auto found = m_fonts.find(fontHandle);
    return found != m_fonts.end() ? *found->second : m_atlas;
}

void FontAtlasTextMeasurer::MeasureText(const std::string& text, void* fontHandle,
                                         float& outWidth, float& outHeight) const {
    const auto& atlas = GetFont(fontHandle);
    float width = 0.0f;
    for (unsigned char c : text) {
        const GlyphInfo* glyph = atlas.FindGlyph(static_cast<char32_t>(c));
        if (glyph) width += glyph->advance;
    }
    outWidth = width;
    outHeight = atlas.GetLineHeight();
}

} // namespace LambUI

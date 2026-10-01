#pragma once

#include "lambui_export.h"
#include "IRenderer.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace LambUI {

// Resolved placement/metrics for a single glyph inside a FontAtlas bitmap.
struct GlyphInfo {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f; // Atlas UV rect
    float width = 0.0f, height = 0.0f;                 // Glyph quad size, pixels
    float bearingX = 0.0f, bearingY = 0.0f;            // Pen-to-quad-top-left offset, pixels
    float advance = 0.0f;                              // Pen advance to the next glyph, pixels
};

// CPU-side signed-distance-field font atlas (goals.txt item 5). Bakes a
// single-channel SDF bitmap for the printable ASCII range once from a
// TTF/OTF file and exposes glyph metrics/UVs so any IRenderer backend can
// upload the bitmap as a texture and sample it (shader smoothstep, GL alpha
// test, or a CPU-side sharpening curve). The library never touches a
// graphics API here - only raw pixel/metric data crosses the HAL boundary.
class LAMBUI_API FontAtlas {
public:
    // pixelHeight: em size baked before distance-field scaling.
    // padding: SDF spread radius in pixels (larger = smoother scaling range).
    bool LoadFromFile(const std::string& ttfPath, int pixelHeight = 48, int padding = 4);

    const GlyphInfo* FindGlyph(char32_t codepoint) const;

    float GetLineHeight() const { return m_lineHeight; }
    float GetAscent() const { return m_ascent; }

    int GetAtlasWidth() const { return m_atlasWidth; }
    int GetAtlasHeight() const { return m_atlasHeight; }
    // Single-channel (8bpp) distance field bitmap, row-major, top-to-bottom.
    const std::vector<uint8_t>& GetAtlasPixels() const { return m_atlasPixels; }
    // Distance-field byte value that represents the glyph's exact edge.
    uint8_t GetOnEdgeValue() const { return m_onEdgeValue; }
    float GetPixelDistanceScale() const { return m_pixelDistanceScale; }

private:
    std::unordered_map<char32_t, GlyphInfo> m_glyphs;
    std::vector<uint8_t> m_atlasPixels;
    int m_atlasWidth = 0;
    int m_atlasHeight = 0;
    float m_lineHeight = 0.0f;
    float m_ascent = 0.0f;
    uint8_t m_onEdgeValue = 180;
    float m_pixelDistanceScale = 0.0f;
};

// Concrete ITextMeasurer backed by borrowed atlases; registered handles select
// metrics, while null/unknown handles use the constructor's default atlas.
class LAMBUI_API FontAtlasTextMeasurer : public ITextMeasurer {
public:
    explicit FontAtlasTextMeasurer(const FontAtlas& atlas);
    ~FontAtlasTextMeasurer() override;

    bool RegisterFont(void* fontHandle, const FontAtlas& atlas);
    const FontAtlas& GetFont(void* fontHandle) const;

    void MeasureText(const std::string& text, void* fontHandle,
                      float& outWidth, float& outHeight) const override;

private:
    const FontAtlas& m_atlas;
    std::unordered_map<void*, const FontAtlas*> m_fonts;
};

} // namespace LambUI

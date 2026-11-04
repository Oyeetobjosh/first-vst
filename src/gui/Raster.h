#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/*!
    Raster — a small software rasterizer operating on an RGBA8 buffer.

    All drawing primitives (rects, lines, circles, arcs, text) are implemented
    here once, so the headless software renderer and the platform display
    drivers (Windows/macOS) share the exact same rendering code — the drivers
    just wrap a Raster around their own framebuffer.
*/
class Raster
{
public:
    Raster (int width, int height);

    int width () const { return m_w; }
    int height () const { return m_h; }

    void clear (uint8_t r, uint8_t g, uint8_t b);

    void fillRect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void strokeRect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void fillRoundRect (int x, int y, int w, int h, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a = 255);
    void strokeRoundRect (int x, int y, int w, int h, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a = 255);
    void drawLine (int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    void fillCircle (int cx, int cy, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0 = 255,
                     uint8_t r1 = 0, uint8_t g1 = 0, uint8_t b1 = 0, uint8_t a1 = 0);
    void strokeCircle (int cx, int cy, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0 = 255);
    void strokeArc (int cx, int cy, int r, double a0, double a1, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t aa = 255);

    void drawText (const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255, int scale = 1);
    int textWidth (const char* text, int scale = 1) const;
    static int textHeight (int scale = 1);

    // Direct pixel access (used by the platform display drivers to blit,
    // and by tests to inspect the rendered frame)
    const uint8_t* pixels () const { return m_pixels.data (); }
    uint8_t* pixels () { return m_pixels.data (); }
    int pixelIndex (int x, int y) const { return (y * m_w + x) * 4; }

    struct PixelRGBA
    {
        uint8_t r, g, b, a;
    };
    PixelRGBA pixel (int x, int y) const;

    // 5x7 bitmap font (A-Z, 0-9 and common symbols)
    static bool glyphAt (char c, int scale, int gx, int gy);

private:
    void putPixel (int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

    int m_w;
    int m_h;
    std::vector<uint8_t> m_pixels;   // RGBA8
};

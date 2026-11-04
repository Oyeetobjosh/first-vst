#pragma once

#include "DisplayDriver.h"

/*!
    SoftwareDisplayDriver — renders into an in-memory Raster.

    Used by the headless UI tests. Also serves as the reference
    implementation the platform drivers mirror.
*/
class SoftwareDisplayDriver : public DisplayDriver
{
public:
    SoftwareDisplayDriver (int width, int height)
    : m_raster (width, height)
    {
    }

    int width () const override { return m_raster.width (); }
    int height () const override { return m_raster.height (); }

    void clear (uint8_t r, uint8_t g, uint8_t b) override { m_raster.clear (r, g, b); }
    void fillRect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override
    { m_raster.fillRect (x, y, w, h, r, g, b, a); }
    void strokeRect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override
    { m_raster.strokeRect (x, y, w, h, r, g, b, a); }
    void drawLine (int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) override
    { m_raster.drawLine (x0, y0, x1, y1, r, g, b, a); }
    void fillCircle (int cx, int cy, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0 = 255,
                     uint8_t r1 = 0, uint8_t g1 = 0, uint8_t b1 = 0, uint8_t a1 = 0) override
    { m_raster.fillCircle (cx, cy, r, r0, g0, b0, a0, r1, g1, b1, a1); }
    void strokeCircle (int cx, int cy, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0 = 255) override
    { m_raster.strokeCircle (cx, cy, r, r0, g0, b0, a0); }
    void strokeArc (int cx, int cy, int r, double a0, double a1, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t aa = 255) override
    { m_raster.strokeArc (cx, cy, r, a0, a1, r0, g0, b0, aa); }
    void drawText (const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255, int scale = 1) override
    { m_raster.drawText (text, x, y, r, g, b, a, scale); }
    int textWidth (const char* text, int scale = 1) const override { return m_raster.textWidth (text, scale); }
    int textHeight (int scale = 1) const override { return m_raster.textHeight (scale); }

    const Raster& raster () const { return m_raster; }
    Raster& raster () { return m_raster; }

private:
    Raster m_raster;
};

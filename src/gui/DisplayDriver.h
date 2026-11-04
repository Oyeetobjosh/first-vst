#pragma once

#include "Raster.h"

/*!
    DisplayDriver — the abstract rendering surface for the UI.

    The UI draws through this interface. There are two kinds of drivers:

    - SoftwareDisplayDriver: renders into an in-memory Raster (used by the
      headless UI tests and as the reference implementation).
    - Platform drivers (Windows/macOS): wrap a Raster around the platform's
      framebuffer and blit it to the screen after each frame.

    Colors are 8-bit RGBA.
*/
class DisplayDriver
{
public:
    virtual ~DisplayDriver () = default;

    virtual int width () const = 0;
    virtual int height () const = 0;

    virtual void clear (uint8_t r, uint8_t g, uint8_t b) = 0;
    virtual void fillRect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) = 0;
    virtual void strokeRect (int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) = 0;
    virtual void drawLine (int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) = 0;
    virtual void fillCircle (int cx, int cy, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0 = 255,
                             uint8_t r1 = 0, uint8_t g1 = 0, uint8_t b1 = 0, uint8_t a1 = 0) = 0;
    virtual void strokeCircle (int cx, int cy, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a0 = 255) = 0;
    virtual void strokeArc (int cx, int cy, int r, double a0, double a1, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t aa = 255) = 0;
    virtual void drawText (const char* text, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255, int scale = 1) = 0;

    // Round-rect composed from the primitives above (drivers with a native
    // implementation may override).
    void fillRoundRect (int x, int y, int w, int h, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a = 255)
    {
        r = std::max (0, std::min (r, std::min (w, h) / 2));
        fillRect (x + r, y, w - 2 * r, h, r0, g0, b0, a);
        fillRect (x, y + r, w, h - 2 * r, r0, g0, b0, a);
        fillCircle (x + r, y + r, r, r0, g0, b0, a);
        fillCircle (x + w - r - 1, y + r, r, r0, g0, b0, a);
        fillCircle (x + r, y + h - r - 1, r, r0, g0, b0, a);
        fillCircle (x + w - r - 1, y + h - r - 1, r, r0, g0, b0, a);
    }

    void strokeRoundRect (int x, int y, int w, int h, int r, uint8_t r0, uint8_t g0, uint8_t b0, uint8_t a = 255)
    {
        r = std::max (1, std::min (r, std::min (w, h) / 2));
        drawLine (x + r, y, x + w - r - 1, y, r0, g0, b0, a);
        drawLine (x + r, y + h - 1, x + w - r - 1, y + h - 1, r0, g0, b0, a);
        drawLine (x, y + r, x, y + h - r - 1, r0, g0, b0, a);
        drawLine (x + w - 1, y + r, x + w - 1, y + h - r - 1, r0, g0, b0, a);
        strokeArc (x + r, y + r, r, M_PI, 3.0 * M_PI / 2.0, r0, g0, b0, a);
        strokeArc (x + w - r - 1, y + r, r, 3.0 * M_PI / 2.0, 2.0 * M_PI, r0, g0, b0, a);
        strokeArc (x + r, y + h - r - 1, r, M_PI / 2.0, M_PI, r0, g0, b0, a);
        strokeArc (x + w - r - 1, y + h - r - 1, r, 0.0, M_PI / 2.0, r0, g0, b0, a);
    }
    virtual int textWidth (const char* text, int scale = 1) const = 0;
    virtual int textHeight (int scale = 1) const = 0;

    // Present after each full render — the driver blits the frame to the screen.
    virtual void present () {}
};

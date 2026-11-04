#pragma once

//-----------------------------------------------------------------------------------------------------------------------------------------
// CoreGraphics display driver — macOS only.
// Renders into an RGBA software buffer and presents it as a CGImage into a
// CoreGraphics context (provided by the host NSView). Header-only.
//-----------------------------------------------------------------------------------------------------------------------------------------
#if !defined(__APPLE__)
#error "MacDisplayDriver is only for macOS builds"
#endif

#include <CoreGraphics/CoreGraphics.h>
#include <vector>

#include "DisplayDriver.h"

class MacDisplayDriver : public DisplayDriver
{
public:
    MacDisplayDriver (int width, int height)
    : m_width (width)
    , m_height (height)
    , m_image (nullptr)
    , m_ctx (nullptr)
    {
        m_pixels.assign ((size_t) width * (size_t) height * 4, 0);
    }

    ~MacDisplayDriver () override
    {
        if (m_image)
            CFRelease (m_image);
    }

    void setContext (CGContextRef ctx) { m_ctx = ctx; }

    void clear (uint8_t r, uint8_t g, uint8_t b, uint8_t a) override
    {
        memset (m_pixels.data (), 0, m_pixels.size ());
        (void) r; (void) g; (void) b; (void) a;
    }

    void putPixel (int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) override
    {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height)
            return;

        const size_t i = ((size_t) y * (size_t) m_width + (size_t) x) * 4;
        m_pixels[i]     = r;
        m_pixels[i + 1] = g;
        m_pixels[i + 2] = b;
        m_pixels[i + 3] = a;
    }

    void present () override
    {
        if (!m_ctx)
            return;

        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB ();
        CGDataProviderRef provider = CGDataProviderCreateWithData (nullptr, m_pixels.data (), m_pixels.size (), nullptr);

        if (m_image)
            CFRelease (m_image);

        m_image = CGImageCreate (m_width, m_height, 8, 32, m_width * 4, space,
                                 kCGImageAlphaPremultipliedLast, provider, nullptr,
                                 false, kCGImageRenderingIntentDefault);

        CGDataProviderRelease (provider);
        CGColorSpaceRelease (space);

        if (m_image)
        {
            CGContextSaveState (m_ctx);
            CGContextSetAlpha (m_ctx, 1.0);
            CGContextDrawImage (m_ctx, CGRectMake (0, 0, m_width, m_height), m_image);
            CGContextRestoreState (m_ctx);
        }
    }

private:
    int m_width, m_height;
    std::vector<uint8_t> m_pixels;
    CGImageRef m_image;
    CGContextRef m_ctx;
};

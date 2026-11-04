#pragma once

//-----------------------------------------------------------------------------------------------------------------------------------------
// Win32 GDI display driver — Windows only.
// Renders into an offscreen 32-bit DIB section and StretchBlts it onto the
// editor window on present(). Header-only so it is only compiled on Windows.
//-----------------------------------------------------------------------------------------------------------------------------------------
#if !defined(_WIN32)
#error "Win32DisplayDriver is only for Windows builds"
#endif

#include <windows.h>
#include <vector>

#include "DisplayDriver.h"

class Win32DisplayDriver : public DisplayDriver
{
public:
    Win32DisplayDriver (HWND hwnd, int width, int height)
    : m_hwnd (hwnd)
    , m_width (width)
    , m_height (height)
    , m_memDC (nullptr)
    , m_bitmap (nullptr)
    {
        m_pixels.assign ((size_t) width * (size_t) height * 4, 0);

        memset (&m_bmi, 0, sizeof (m_bmi));
        m_bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
        m_bmi.bmiHeader.biWidth       = width;
        m_bmi.bmiHeader.biHeight      = -height;     // top-down
        m_bmi.bmiHeader.biPlanes       = 1;
        m_bmi.bmiHeader.biBitCount    = 32;
        m_bmi.bmiHeader.biCompression  = BI_RGB;

        m_bitmap = CreateDIBSection (nullptr, &m_bmi, DIB_RGB_COLORS, &m_pixels[0], nullptr, 0);
        m_memDC  = CreateCompatibleDC (nullptr);
        SelectObject (m_memDC, m_bitmap);
    }

    ~Win32DisplayDriver () override
    {
        if (m_memDC)
            DeleteDC (m_memDC);
        if (m_bitmap)
            DeleteObject (m_bitmap);
    }

    void clear (uint8_t r, uint8_t g, uint8_t b, uint8_t a) override
    {
        memset (m_pixels.data (), 0, m_pixels.size ());
        (void) r; (void) g; (void) b; (void) a;
    }

    void putPixel (int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) override
    {
        if (x < 0 || y < 0 || x >= m_width || y >= m_height)
            return;

        // DIB section pixels are stored BGRA
        const size_t i = ((size_t) y * (size_t) m_width + (size_t) x) * 4;
        m_pixels[i]     = b;
        m_pixels[i + 1] = g;
        m_pixels[i + 2] = r;
        m_pixels[i + 3] = a;
    }

    void present () override
    {
        HDC dc = GetDC (m_hwnd);
        if (dc)
        {
            StretchBlt (dc, 0, 0, m_width, m_height, m_memDC, 0, 0, m_width, m_height, SRCCOPY);
            ReleaseDC (m_hwnd, dc);
        }
    }

private:
    HWND m_hwnd;
    int m_width, m_height;
    std::vector<uint8_t> m_pixels;
    BITMAPINFO m_bmi;
    HDC m_memDC;
    HBITMAP m_bitmap;
};

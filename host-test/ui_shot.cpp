//----------------------------------------------------------------------------------------------------------------------------------------
// Tobjosh Reverb — UI screenshot tool
//
// Renders the headless editor into the software display driver and writes
// the frame to a PPM image (viewable in any image viewer).
//
// Usage:  ui-shot [output.ppm]
//----------------------------------------------------------------------------------------------------------------------------------------

#include <cstdio>

#include "gui/Editor.h"
#include "gui/SoftwareDisplayDriver.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
class FakeHost : public ReverbUI::Host
{
public:
    float params[ReverbUI::numParams] = { 1.0f, 0.40f, 0.0f, 0.35f };
    bool bypass = false;

    float getParam (int index) const override { return params[index]; }
    void setParam (int index, float value) override { params[index] = value; }
    void setBypassLED (bool on) override { bypass = on; }
    bool isBypassed () const override { return bypass; }
};

//-----------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char** argv)
{
    const char* outPath = (argc > 1) ? argv[1] : "tobjosh_reverb_ui.ppm";

    FakeHost host;
    ReverbEditor editor (nullptr, host);
    SoftwareDisplayDriver display (640, 380);

    editor.render (display);

    const Raster& raster = display.raster ();
    FILE* f = std::fopen (outPath, "wb");
    if (!f)
    {
        std::printf ("Could not write %s\n", outPath);
        return 1;
    }

    std::fprintf (f, "P6\n%d %d\n255\n", raster.width (), raster.height ());
    const uint8_t* pixels = raster.pixels ();
    for (int i = 0; i < raster.width () * raster.height (); ++i)
    {
        const unsigned char rgb[3] = { pixels[i * 4], pixels[i * 4 + 1], pixels[i * 4 + 2] };
        std::fwrite (rgb, 1, 3, f);
    }
    std::fclose (f);

    std::printf ("Wrote %s (%dx%d)\n", outPath, raster.width (), raster.height ());
    return 0;
}

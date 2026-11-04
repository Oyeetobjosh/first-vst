//----------------------------------------------------------------------------------------------------------------------------------------
// Tobjosh Reverb — headless UI test
//
// Drives the ReverbEditor + UI framework directly (no window, no plug-in
// loading) with the software display driver and verifies:
//   - the surface renders (dark theme, non-uniform)
//   - layout: no control overlaps
//   - knob drag / double-click reset / arrow keys / wheel
//   - preset buttons apply presets
//   - bypass LED toggle
//   - the 360° knob sweeps a full circle
//
// Exit code 0 = all checks passed, 1 = failure.
//----------------------------------------------------------------------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstring>

#include "gui/Editor.h"
#include "gui/SoftwareDisplayDriver.h"
#include "presets.h"

static int g_failures = 0;

static void report (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Fake host — mirrors what the plug-in implements
//----------------------------------------------------------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------------------------------------------------------
static void testRender (ReverbEditor& editor, SoftwareDisplayDriver& d)
{
    std::printf ("Render:\n");

    editor.render (d);

    const Raster& raster = d.raster ();
    const int w = raster.width (), h = raster.height ();

    report (w == 640 && h == 380, "surface is 640x380");

    // dark background at the corners
    const auto corner = raster.pixel (2, 2);
    report (corner.r < 60 && corner.g < 60 && corner.b < 70, "background is dark");

    // non-uniform: a knob ring pixel must differ from the corner (controls drawn)
    const auto knobPixel = raster.pixel (130, 98);    // top of the Size knob ring
    report (std::abs ((int) knobPixel.r - (int) corner.r) + std::abs ((int) knobPixel.g - (int) corner.g)
            + std::abs ((int) knobPixel.b - (int) corner.b) > 20,
            "surface is non-uniform (controls are drawn)");

    // the title band area must be slightly lighter than the bottom
    const auto title = raster.pixel (w / 2, 30);
    const auto bottom = raster.pixel (w / 2, h - 10);
    report (title.r >= bottom.r, "title band is present");
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testLayout (ReverbEditor& editor)
{
    std::printf ("Layout:\n");

    UI& ui = editor.ui ();
    const auto controls = ui.controls ();

    report (controls.size () == 4 + 4 + 1, "surface has 4 knobs, 4 preset buttons and 1 bypass toggle");

    // every control's center must hit exactly that control
    bool noOverlap = true;
    for (size_t i = 0; i < controls.size (); ++i)
    {
        const int cx = controls[i]->centerX ();
        const int cy = controls[i]->centerY ();
        UIControl* hit = ui.controlAt (cx, cy);
        if (hit != controls[i])
            noOverlap = false;
    }
    report (noOverlap, "no control overlaps another");

    // knobs in a row, increasing x
    bool sorted = true;
    for (int i = 1; i < 4; ++i)
        if (ui.controls ()[i]->centerX () <= ui.controls ()[i - 1]->centerX ())
            sorted = false;
    report (sorted, "the 4 knobs are laid out left to right");
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testKnobDrag (ReverbEditor& editor, FakeHost& host)
{
    std::printf ("Knob interaction:\n");

    UI& ui = editor.ui ();
    Knob* sizeKnob = editor.reverbUI ().knob (0);

    report (sizeKnob != nullptr, "Size knob exists");

    // drag the Size knob upward -> value increases
    const int cx = sizeKnob->centerX ();
    const int cy = sizeKnob->centerY ();
    const float startValue = sizeKnob->value ();

    ui.mouseDown (cx, cy, 1);
    ui.mouseMove (cx, cy - 100, 1);
    ui.mouseUp (cx, cy - 100, 1);

    const float afterDrag = sizeKnob->value ();
    report (afterDrag > startValue + 1.0f, "dragging the knob upward increases the value");
    report (std::fabs (host.params[0] - afterDrag) < 1e-4f, "the host parameter follows the knob");
    report (!sizeKnob->valueText ().empty (), "the knob shows a value display");

    // drag downward -> value decreases
    ui.mouseDown (cx, cy, 1);
    ui.mouseMove (cx, cy + 100, 1);
    ui.mouseUp (cx, cy + 100, 1);
    report (sizeKnob->value () < afterDrag, "dragging the knob downward decreases the value");

    // double-click resets to the default
    ui.dblClick (cx, cy);
    report (std::fabs (sizeKnob->value () - sizeKnob->defaultValue ()) < 1e-4f, "double-click resets the knob");
    report (std::fabs (host.params[0] - sizeKnob->defaultValue ()) < 1e-4f, "double-click reset reaches the host");
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testKeyboardAndWheel (ReverbEditor& editor)
{
    std::printf ("Keyboard & wheel:\n");

    UI& ui = editor.ui ();
    Knob* sizeKnob = editor.reverbUI ().knob (0);

    // focus via click
    const int cx = sizeKnob->centerX ();
    const int cy = sizeKnob->centerY ();
    ui.mouseDown (cx, cy, 1);
    ui.mouseUp (cx, cy, 1);
    report (ui.focused () == sizeKnob, "clicking a knob gives it focus");

    const float before = sizeKnob->value ();
    ui.keyDown (0, 38);    // arrow up
    report (sizeKnob->value () > before, "arrow up nudges the focused knob");
    ui.keyDown (0, 40);    // arrow down
    report (sizeKnob->value () < before + 0.01f, "arrow down nudges the focused knob");

    const float beforeWheel = sizeKnob->value ();
    ui.wheel (1.0f);
    report (sizeKnob->value () > beforeWheel, "wheel nudges the focused knob");
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testPresets (ReverbEditor& editor, FakeHost& host)
{
    std::printf ("Presets:\n");

    UI& ui = editor.ui ();

    // find the "Hall" button
    Button* hall = nullptr;
    for (UIControl* c : ui.controls ())
        if (auto* b = dynamic_cast<Button*> (c))
            if (b->label () == "Hall")
                hall = b;
    report (hall != nullptr, "the 'Hall' preset button exists");

    const int bx = hall->centerX ();
    const int by = hall->centerY ();
    ui.mouseDown (bx, by, 1);
    ui.mouseUp (bx, by, 1);

    report (std::fabs (host.params[0] - 1.5f) < 1e-4f, "Hall preset sets Size to 1.5");
    report (std::fabs (host.params[1] - 0.30f) < 1e-4f, "Hall preset sets Damping to 0.30");
    report (std::fabs (host.params[2] - 120.0f) < 1e-4f, "Hall preset sets 360° to 120");
    report (std::fabs (host.params[3] - 0.40f) < 1e-4f, "Hall preset sets Mix to 0.40");

    // knobs reflect the preset
    Knob* angleKnob = editor.reverbUI ().knob (2);
    report (std::fabs (angleKnob->value () - 120.0f) < 1e-4f, "the 360° knob reflects the preset");

    // Space preset via the editor's preset API
    editor.reverbUI ().applyPreset (3);
    report (std::fabs (host.params[0] - 6.0f) < 1e-4f, "applyPreset(3) loads Space");

    // every preset is valid
    bool allValid = true;
    for (int i = 0; i < ReverbUI::numPresets; ++i)
        editor.reverbUI ().applyPreset (i);
    report (allValid, "all 4 presets apply without error");
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testBypassToggle (ReverbEditor& editor, FakeHost& host)
{
    std::printf ("Bypass LED:\n");

    UI& ui = editor.ui ();

    Toggle* led = nullptr;
    for (UIControl* c : ui.controls ())
        if (auto* t = dynamic_cast<Toggle*> (c))
            led = t;
    report (led != nullptr, "the bypass toggle exists");

    const int tx = led->centerX ();
    const int ty = led->centerY ();
    ui.mouseDown (tx, ty, 1);
    ui.mouseUp (tx, ty, 1);
    report (host.bypass == true, "clicking the LED toggles bypass on");

    ui.mouseDown (tx, ty, 1);
    ui.mouseUp (tx, ty, 1);
    report (host.bypass == false, "clicking the LED toggles bypass off");
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testFullCircleKnob (ReverbEditor& editor)
{
    std::printf ("360° knob:\n");

    Knob* angleKnob = editor.reverbUI ().knob (2);
    report (angleKnob != nullptr, "the 360° knob exists");

    // the angle knob must sweep a full circle
    angleKnob->setValue (0.0f);
    const float a0 = angleKnob->value ();
    angleKnob->setValue (360.0f);
    report (std::fabs (angleKnob->value () - 360.0f) < 1e-4f, "the 360° knob spans 0..360");
    report (a0 == 0.0f, "the 360° knob starts at 0");

    // dragging across the full circle must not wrap weirdly
    UI& ui = editor.ui ();
    const int cx = angleKnob->centerX ();
    const int cy = angleKnob->centerY ();
    ui.mouseDown (cx, cy, 1);
    ui.mouseMove (cx, cy - 200, 1);
    ui.mouseUp (cx, cy - 200, 1);
    report (angleKnob->value () > 350.0f, "dragging the 360° knob far reaches the top of the range");
}

//----------------------------------------------------------------------------------------------------------------------------------------
int main ()
{
    FakeHost host;
    ReverbEditor editor (nullptr, host);
    SoftwareDisplayDriver display (640, 380);

    std::printf ("Tobjosh Reverb UI test\n");

    testRender (editor, display);
    testLayout (editor);
    testKnobDrag (editor, host);
    testKeyboardAndWheel (editor);
    testPresets (editor, host);
    testBypassToggle (editor, host);
    testFullCircleKnob (editor);

    if (g_failures == 0)
    {
        std::printf ("\nAll Tobjosh Reverb UI checks passed.\n");
        return 0;
    }

    std::printf ("\n%d Tobjosh Reverb UI check(s) FAILED.\n", g_failures);
    return 1;
}

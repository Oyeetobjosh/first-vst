#pragma once

#include <string>

#include "Controls.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
// ReverbUI — the Tobjosh Reverb control surface (4 knobs, presets, bypass LED)
//-----------------------------------------------------------------------------------------------------------------------------------------
class ReverbUI
{
public:
    // Callbacks the plugin implements
    struct Host
    {
        virtual float getParam (int index) const = 0;
        virtual void   setParam (int index, float value) = 0;
        virtual void   setBypassLED (bool on) = 0;
        virtual bool   isBypassed () const = 0;
        virtual ~Host () {}
    };

    static const int numParams  = 4;
    static const int numPresets = 4;

    ReverbUI (Host& host);

    // Builds the layout into a UI container
    void build (UI& ui);

    // Renders the whole surface (syncs knob values from the host first)
    void render (DisplayDriver& d, UI& ui);

    // Presets
    void applyPreset (int index);

    // Knob access (for tests)
    Knob* knob (int index) const { return m_knobs[index]; }

private:
    void onKnobValue (int index, float value);
    void syncKnobs ();

    Host& m_host;
    Knob* m_knobs[numParams];
    Toggle* m_bypassLED;
};

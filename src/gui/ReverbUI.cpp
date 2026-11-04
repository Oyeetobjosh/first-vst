#include "ReverbUI.h"

#include <cmath>

#include "../presets.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
namespace
{
    const int knobRadius   = 52;
    const int knobY        = 150;
    const int knobSpacing  = 130;
    const int knobStartX   = 130;

    const char* paramNames[] = { "SIZE", "DAMPING", "360°", "MIX" };
    const char* paramFormats[] = { "%.2fs", "%.0f%%", "%.0f°", "%.0f%%" };

    const float paramDefaults[] = { 1.0f, 0.40f, 0.0f, 0.35f };
}

//-----------------------------------------------------------------------------------------------------------------------------------------
ReverbUI::ReverbUI (Host& host)
: m_host (host)
{
    for (int i = 0; i < numParams; ++i)
        m_knobs[i] = nullptr;
    m_bypassLED = nullptr;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbUI::build (UI& ui)
{
    // --- knobs -------------------------------------------------------------------------------------------
    const float ranges[numParams][2] = { { 0.2f, 6.0f }, { 0.0f, 1.0f }, { 0.0f, 360.0f }, { 0.0f, 1.0f } };

    for (int i = 0; i < numParams; ++i)
    {
        const int cx = knobStartX + i * knobSpacing;
        m_knobs[i] = new Knob (cx, knobY, knobRadius, paramNames[i],
                               ranges[i][0], ranges[i][1],
                               m_host.getParam (i), paramDefaults[i], paramFormats[i]);
        m_knobs[i]->setFullCircle (i == 2);               // the 360° knob sweeps a full circle
        m_knobs[i]->setValueCallback ([this, i] (float v) { onKnobValue (i, v); });
        ui.add (m_knobs[i]);
    }

    // --- preset buttons ------------------------------------------------------------------------------------
    const int btnW = 100, btnH = 32, btnGap = 20;
    const int totalW = numPresets * btnW + (numPresets - 1) * btnGap;
    const int startX = (ui.width () - totalW) / 2;

    for (int i = 0; i < numPresets; ++i)
    {
        auto* btn = new Button (startX + i * (btnW + btnGap), 248, btnW, btnH, reverbPresets[i].name);
        btn->setCallback ([this, i] { applyPreset (i); });
        ui.add (btn);
    }

    // --- bypass LED -----------------------------------------------------------------------------------------
    m_bypassLED = new Toggle (556, 254, 75, 30, "BYPASS", m_host.isBypassed ());
    m_bypassLED->setToggleCallback ([this] (bool on) { m_host.setBypassLED (on); });
    ui.add (m_bypassLED);

    // --- background: title band, knob panel, footer hint ------------------------------------------------------
    ui.setBackgroundPainter ([&ui] (DisplayDriver& d)
    {
        d.clear (uicolor::bg, uicolor::bgG, uicolor::bgB);

        // top band
        d.fillRect (0, 0, ui.width (), 64, 0x18, 0x1b, 0x22, 255);
        d.drawLine (0, 64, ui.width (), 64, uicolor::accent, uicolor::accentG, uicolor::accentB, 200);

        // title
        const char* title = "TOBJOSH REVERB";
        const int titleW = d.textWidth (title, 3);
        d.drawText (title, ui.width () / 2 - titleW / 2, 14, uicolor::accent, uicolor::accentG, uicolor::accentB, 255, 3);

        // subtitle
        const char* subtitle = "360° ROOM AUDIO";
        const int subW = d.textWidth (subtitle, 1);
        d.drawText (subtitle, ui.width () / 2 - subW / 2, 42, uicolor::dim, uicolor::dimG, uicolor::dimB, 255, 1);

        // knob panel
        d.fillRoundRect (50, 80, 540, 152, 12, uicolor::panel, uicolor::panelG, uicolor::panelB, 255);
        d.strokeRoundRect (50, 80, 540, 152, 12, uicolor::dim, uicolor::dimG, uicolor::dimB, 120);

        // footer hint
        const char* hint = "DRAG KNOBS   ·   DOUBLE-CLICK TO RESET   ·   ARROWS / WHEEL TO NUDGE";
        const int hintW = d.textWidth (hint, 1);
        d.drawText (hint, ui.width () / 2 - hintW / 2, 358, uicolor::dim, uicolor::dimG, uicolor::dimB, 200, 1);
    });
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbUI::render (DisplayDriver& d, UI& ui)
{
    syncKnobs ();
    ui.render (d);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbUI::applyPreset (int index)
{
    if (index < 0 || index >= numPresets)
        return;

    const ReverbPreset& p = reverbPresets[index];
    m_host.setParam (0, p.size);
    m_host.setParam (1, p.damping);
    m_host.setParam (2, p.angle);
    m_host.setParam (3, p.mix);

    syncKnobs ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbUI::onKnobValue (int index, float value)
{
    m_host.setParam (index, value);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbUI::syncKnobs ()
{
    for (int i = 0; i < numParams; ++i)
        if (m_knobs[i])
            m_knobs[i]->setValue (m_host.getParam (i));

    if (m_bypassLED)
        m_bypassLED->setOn (m_host.isBypassed ());
}

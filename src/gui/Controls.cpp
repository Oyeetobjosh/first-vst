#include "Controls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

//-----------------------------------------------------------------------------------------------------------------------------------------
UIControl::UIControl (int x, int y, int w, int h)
: m_x (x)
, m_y (y)
, m_w (w)
, m_h (h)
, m_enabled (true)
, m_focused (false)
, m_hover (false)
{
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool UIControl::hitTest (int x, int y) const
{
    return x >= m_x && y >= m_y && x < m_x + m_w && y < m_y + m_h;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UIControl::mouseDown (int, int, int) {}
void UIControl::mouseMove (int, int, int) {}
void UIControl::mouseUp (int, int, int) {}
void UIControl::dblClick (int, int) {}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UIControl::drawFocusRing (DisplayDriver& d)
{
    if (m_focused)
        d.strokeRect (m_x - 1, m_y - 1, m_w + 2, m_h + 2, uicolor::accent, uicolor::accentG, uicolor::accentB, 160);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// UI
//-----------------------------------------------------------------------------------------------------------------------------------------
UI::UI (int width, int height)
: m_w (width)
, m_h (height)
, m_focused (nullptr)
, m_dragging (nullptr)
, m_dragButton (0)
{
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::add (UIControl* c)
{
    if (c)
        m_controls.push_back (c);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::remove (UIControl* c)
{
    m_controls.erase (std::remove (m_controls.begin (), m_controls.end (), c), m_controls.end ());
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::drawBackground (DisplayDriver& d)
{
    d.clear (uicolor::bg, uicolor::bgG, uicolor::bgB);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::render (DisplayDriver& d)
{
    if (m_backgroundPainter)
        m_backgroundPainter (d);
    else
        drawBackground (d);

    for (UIControl* c : m_controls)
        if (c->enabled ())
            c->draw (d);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
UIControl* UI::controlAt (int x, int y) const
{
    for (auto it = m_controls.rbegin (); it != m_controls.rend (); ++it)
        if ((*it)->enabled () && (*it)->hitTest (x, y))
            return *it;
    return nullptr;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::setFocus (UIControl* c)
{
    if (m_focused && m_focused != c)
        m_focused->setFocus (false);
    m_focused = c;
    if (c)
        c->setFocus (true);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::mouseDown (int x, int y, int button)
{
    UIControl* c = controlAt (x, y);
    if (c && c->enabled ())
    {
        setFocus (c);
        m_dragging = c;
        m_dragButton = button;
        c->mouseDown (x, y, button);
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::mouseMove (int x, int y, int button)
{
    if (m_dragging && m_dragging->enabled ())
    {
        m_dragging->mouseMove (x, y, button);
        return;
    }

    for (UIControl* c : m_controls)
        c->setHover (false);
    UIControl* c = controlAt (x, y);
    if (c)
        c->setHover (true);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::mouseUp (int x, int y, int button)
{
    if (m_dragging && m_dragging->enabled ())
        m_dragging->mouseUp (x, y, button);
    m_dragging = nullptr;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::dblClick (int x, int y)
{
    UIControl* c = controlAt (x, y);
    if (c && c->enabled ())
        c->dblClick (x, y);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::keyDown (int ascii, int virtualKey)
{
    if (!m_focused || !m_focused->enabled ())
        return;

    if (auto* k = dynamic_cast<Knob*> (m_focused))
    {
        const float range = k->max () - k->min ();
        const float step = range / 50.0f;
        if (virtualKey == 38 || virtualKey == 39)          // up / right
            k->setValue (k->value () + step, true);
        else if (virtualKey == 40 || virtualKey == 37)     // down / left
            k->setValue (k->value () - step, true);
    }
    else if (auto* t = dynamic_cast<Toggle*> (m_focused))
    {
        if (ascii == 32 || ascii == 13)                    // space / enter
            t->setOn (!t->isOn ());
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void UI::wheel (float distance)
{
    if (auto* k = dynamic_cast<Knob*> (m_focused))
        k->setValue (k->value () + distance * 0.01f * (k->max () - k->min ()), true);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Knob
//-----------------------------------------------------------------------------------------------------------------------------------------
Knob::Knob (int cx, int cy, int radius, const char* label, float min, float max, float value, float defaultValue, const char* format)
: UIControl (cx - radius, cy - radius, 2 * radius + 1, 2 * radius + 1)
, m_radius (radius)
, m_min (min)
, m_max (max)
, m_value (value)
, m_defaultValue (defaultValue)
, m_label (label ? label : "")
, m_format (format ? format : "%.2f")
, m_fullCircle (false)
, m_dragging (false)
, m_dragStartY (0)
, m_dragStartValue (0.f)
{
    updateValueText ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::updateValueText ()
{
    char text[32];
    std::snprintf (text, sizeof (text), m_format.c_str (), m_value);
    m_valueText = text;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::setLabel (const char* label)
{
    m_label = label ? label : "";
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::setValue (float v, bool notifyCallback)
{
    m_value = std::max (m_min, std::min (m_max, v));
    updateValueText ();
    if (notifyCallback && m_valueCallback)
        m_valueCallback (m_value);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
double Knob::valueAngle () const
{
    if (m_fullCircle)
        return m_value / (m_max - m_min) * 2.0 * M_PI;

    const double t = (m_value - m_min) / (m_max - m_min);
    return -3.0 * M_PI / 4.0 + t * (3.0 * M_PI / 2.0);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::draw (DisplayDriver& d)
{
    const int cx = centerX ();
    const int cy = centerY ();
    const int r = m_radius;

    // soft glow halo
    d.fillCircle (cx, cy, r + 6, uicolor::accent, uicolor::accentG, uicolor::accentB, 34,
                  uicolor::accent, uicolor::accentG, uicolor::accentB, 0);

    // ring
    d.strokeCircle (cx, cy, r, uicolor::accent, uicolor::accentG, uicolor::accentB, 235);

    // sweep arc (accent, from min to current)
    const double va = valueAngle ();
    if (m_fullCircle)
        d.strokeArc (cx, cy, r, 0.0, va, uicolor::accent, uicolor::accentG, uicolor::accentB, 255);
    else
        d.strokeArc (cx, cy, r, -3.0 * M_PI / 4.0, va, uicolor::accent, uicolor::accentG, uicolor::accentB, 255);

    // tick marks
    const int ticks = m_fullCircle ? 12 : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const double a = m_fullCircle ? i * 2.0 * M_PI / ticks
                                       : -3.0 * M_PI / 4.0 + (double) i / (ticks - 1) * (3.0 * M_PI / 2.0);
        d.drawLine (cx + (int) std::lround (std::cos (a) * (r - 3)),
                    cy + (int) std::lround (std::sin (a) * (r - 3)),
                    cx + (int) std::lround (std::cos (a) * (r + 2)),
                    cy + (int) std::lround (std::sin (a) * (r + 2)),
                    uicolor::dim, uicolor::dimG, uicolor::dimB, 255);
    }

    // pointer
    d.drawLine (cx, cy,
                cx + (int) std::lround (std::cos (va) * (r - 5)),
                cy + (int) std::lround (std::sin (va) * (r - 5)),
                uicolor::accentGlowG, uicolor::accentGlowB, 255, 255);

    // hub
    d.fillCircle (cx, cy, 5, uicolor::bg, uicolor::bgG, uicolor::bgB, 255);
    d.strokeCircle (cx, cy, 5, uicolor::accent, uicolor::accentG, uicolor::accentB, 255);

    // label (above)
    const int labelW = d.textWidth (m_label.c_str (), 2);
    d.drawText (m_label.c_str (), cx - labelW / 2, cy - r - 22, uicolor::accent, uicolor::accentG, uicolor::accentB, 255, 2);

    // value (below)
    const int valueW = d.textWidth (m_valueText.c_str (), 1);
    d.drawText (m_valueText.c_str (), cx - valueW / 2, cy + r + 8, uicolor::text, uicolor::textG, uicolor::textB, 255, 1);

    if (m_focused)
        d.strokeCircle (cx, cy, r + 2, uicolor::accent, uicolor::accentG, uicolor::accentB, 120);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::mouseDown (int x, int y, int button)
{
    if (!hitTest (x, y))
        return;
    m_dragging = true;
    m_dragStartY = y;
    m_dragStartValue = m_value;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::mouseMove (int x, int y, int button)
{
    if (!m_dragging)
        return;

    const float range = m_max - m_min;
    const float dv = (float) (m_dragStartY - y) / 200.0f * range;
    setValue (m_dragStartValue + dv, true);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::mouseUp (int, int, int)
{
    m_dragging = false;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Knob::dblClick (int x, int y)
{
    if (hitTest (x, y))
        setValue (m_defaultValue, true);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Toggle
//-----------------------------------------------------------------------------------------------------------------------------------------
Toggle::Toggle (int x, int y, int w, int h, const char* label, bool on)
: UIControl (x, y, w, h)
, m_on (on)
, m_label (label ? label : "")
{
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Toggle::setOn (bool on)
{
    if (m_on == on)
        return;
    m_on = on;
    if (m_toggleCallback)
        m_toggleCallback (m_on);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Toggle::draw (DisplayDriver& d)
{
    // LED capsule
    const int ledR = 7;
    const int ledCx = m_x + ledR + 2;
    const int ledCy = centerY ();

    d.fillRoundRect (m_x, m_y, m_w, m_h, ledR + 2, uicolor::panel, uicolor::panelG, uicolor::panelB, 255);

    if (m_on)
    {
        d.fillCircle (ledCx, ledCy, ledR + 3, uicolor::onGreen, uicolor::onGreenG, uicolor::onGreenB, 60,
                      uicolor::onGreen, uicolor::onGreenG, uicolor::onGreenB, 0);
        d.fillCircle (ledCx, ledCy, ledR, uicolor::onGreen, uicolor::onGreenG, uicolor::onGreenB, 255);
    }
    else
    {
        d.fillCircle (ledCx, ledCy, ledR, uicolor::dim, uicolor::dimG, uicolor::dimB, 255);
    }

    // label
    d.drawText (m_label.c_str (), m_x + 24, centerY () - 4, uicolor::text, uicolor::textG, uicolor::textB, 255, 1);

    drawFocusRing (d);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Toggle::mouseDown (int x, int y, int button)
{
    if (hitTest (x, y))
        setOn (!m_on);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Button
//-----------------------------------------------------------------------------------------------------------------------------------------
Button::Button (int x, int y, int w, int h, const char* label)
: UIControl (x, y, w, h)
, m_label (label ? label : "")
{
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Button::draw (DisplayDriver& d)
{
    const uint8_t br = m_hover ? 0x2a : 0x22;
    const uint8_t bg2 = m_hover ? 0x2e : 0x25;
    const uint8_t bb2 = m_hover ? 0x38 : 0x2e;

    d.fillRoundRect (m_x, m_y, m_w, m_h, 8, br, bg2, bb2, 255);
    d.strokeRoundRect (m_x, m_y, m_w, m_h, 8, uicolor::dim, uicolor::dimG, uicolor::dimB, 255);

    const int tw = d.textWidth (m_label.c_str (), 1);
    d.drawText (m_label.c_str (), centerX () - tw / 2, centerY () - 4,
                m_hover ? uicolor::accent : uicolor::text, uicolor::textG, uicolor::textB, 255, 1);

    drawFocusRing (d);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Button::mouseDown (int x, int y, int button)
{
    if (hitTest (x, y) && m_callback)
        m_callback ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Label
//-----------------------------------------------------------------------------------------------------------------------------------------
Label::Label (int x, int y, const char* text, int scale)
: UIControl (x, y, 0, 0)
, m_text (text ? text : "")
, m_scale (scale)
{
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Label::setText (const char* text)
{
    m_text = text ? text : "";
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void Label::draw (DisplayDriver& d)
{
    d.drawText (m_text.c_str (), m_x, m_y, uicolor::dim, uicolor::dimG, uicolor::dimB, 255, m_scale);
}

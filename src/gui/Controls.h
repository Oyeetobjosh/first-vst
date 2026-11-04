#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "DisplayDriver.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
// Color palette (8-bit RGBA)
//-----------------------------------------------------------------------------------------------------------------------------------------
namespace uicolor
{
    const uint8_t bg        = 0x14;   // 20
    const uint8_t bgG       = 0x16;   // 22
    const uint8_t bgB       = 0x1c;   // 28
    const uint8_t panel     = 0x1c;
    const uint8_t panelG    = 0x1f;
    const uint8_t panelB    = 0x26;
    const uint8_t accent    = 0x40;   // 64
    const uint8_t accentG   = 0xa0;   // 160
    const uint8_t accentB   = 0xff;   // 255
    const uint8_t accentGlowG = 0xc8; // 200
    const uint8_t accentGlowB = 0xff;
    const uint8_t text      = 0xc8;   // 200
    const uint8_t textG     = 0xc9;
    const uint8_t textB     = 0xd7;
    const uint8_t dim       = 0x5a;   // 90
    const uint8_t dimG      = 0x5f;
    const uint8_t dimB      = 0x69;
    const uint8_t onGreen   = 0x50;   // 80
    const uint8_t onGreenG  = 0xdc;   // 220
    const uint8_t onGreenB  = 0x82;   // 130
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// UIControl — base class for all widgets
//-----------------------------------------------------------------------------------------------------------------------------------------
class UIControl
{
public:
    UIControl (int x, int y, int w, int h);
    virtual ~UIControl () = default;

    virtual void draw (DisplayDriver& d) = 0;

    virtual bool hitTest (int x, int y) const;
    virtual void mouseDown (int x, int y, int button);
    virtual void mouseMove (int x, int y, int button);
    virtual void mouseUp (int x, int y, int button);
    virtual void dblClick (int x, int y);

    int x () const { return m_x; }
    int y () const { return m_y; }
    int w () const { return m_w; }
    int h () const { return m_h; }
    int centerX () const { return m_x + m_w / 2; }
    int centerY () const { return m_y + m_h / 2; }

    void setEnabled (bool on) { m_enabled = on; }
    bool enabled () const { return m_enabled; }
    void setFocus (bool on) { m_focused = on; }
    bool focused () const { return m_focused; }
    void setHover (bool on) { m_hover = on; }
    bool hovered () const { return m_hover; }

protected:
    void drawFocusRing (DisplayDriver& d);

    int m_x, m_y, m_w, m_h;
    bool m_enabled;
    bool m_focused;
    bool m_hover;
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// UI — container that owns controls and dispatches input
//-----------------------------------------------------------------------------------------------------------------------------------------
class UI
{
public:
    UI (int width, int height);

    void add (UIControl* c);
    void remove (UIControl* c);
    std::vector<UIControl*> controls () const { return m_controls; }

    void render (DisplayDriver& d);

    void mouseDown (int x, int y, int button);
    void mouseMove (int x, int y, int button);
    void mouseUp (int x, int y, int button);
    void dblClick (int x, int y);
    void keyDown (int ascii, int virtualKey);
    void wheel (float distance);

    UIControl* controlAt (int x, int y) const;
    UIControl* focused () const { return m_focused; }
    void setFocus (UIControl* c);

    using BackgroundPainter = std::function<void (DisplayDriver&)>;
    void setBackgroundPainter (BackgroundPainter cb) { m_backgroundPainter = std::move (cb); }

    virtual void drawBackground (DisplayDriver& d);

    int width () const { return m_w; }
    int height () const { return m_h; }

private:
    void dispatch (UIControl* c, const char* what, int x, int y, int button);

    int m_w, m_h;
    std::vector<UIControl*> m_controls;
    UIControl* m_focused;
    UIControl* m_dragging;
    int m_dragButton;
    BackgroundPainter m_backgroundPainter;
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// Knob — rotary control with drag, double-click reset and value display
//-----------------------------------------------------------------------------------------------------------------------------------------
class Knob : public UIControl
{
public:
    using ValueCallback = std::function<void (float value)>;

    Knob (int cx, int cy, int radius, const char* label, float min, float max, float value, float defaultValue, const char* format);

    void setValue (float v, bool notifyCallback = false);
    float value () const { return m_value; }
    float defaultValue () const { return m_defaultValue; }
    float min () const { return m_min; }
    float max () const { return m_max; }
    const std::string& label () const { return m_label; }
    void setLabel (const char* label);
    const std::string& valueText () const { return m_valueText; }

    void setFullCircle (bool on) { m_fullCircle = on; }   // for the 360° knob
    void setValueCallback (ValueCallback cb) { m_valueCallback = cb; }

    void draw (DisplayDriver& d) override;
    void mouseDown (int x, int y, int button) override;
    void mouseMove (int x, int y, int button) override;
    void mouseUp (int x, int y, int button) override;
    void dblClick (int x, int y) override;

private:
    double valueAngle () const;   // radians for the pointer
    void updateValueText ();

    int m_radius;
    float m_min, m_max, m_value, m_defaultValue;
    std::string m_label;
    std::string m_format;
    std::string m_valueText;
    bool m_fullCircle;
    bool m_dragging;
    int m_dragStartY;
    float m_dragStartValue;
    ValueCallback m_valueCallback;
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// Toggle — LED-style on/off switch
//-----------------------------------------------------------------------------------------------------------------------------------------
class Toggle : public UIControl
{
public:
    using ToggleCallback = std::function<void (bool on)>;

    Toggle (int x, int y, int w, int h, const char* label, bool on);

    void setOn (bool on);
    bool isOn () const { return m_on; }
    void setToggleCallback (ToggleCallback cb) { m_toggleCallback = cb; }

    void draw (DisplayDriver& d) override;
    void mouseDown (int x, int y, int button) override;

private:
    bool m_on;
    std::string m_label;
    ToggleCallback m_toggleCallback;
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// Button — labelled action button
//-----------------------------------------------------------------------------------------------------------------------------------------
class Button : public UIControl
{
public:
    using ButtonCallback = std::function<void ()>;

    Button (int x, int y, int w, int h, const char* label);

    void setCallback (ButtonCallback cb) { m_callback = cb; }
    const std::string& label () const { return m_label; }

    void draw (DisplayDriver& d) override;
    void mouseDown (int x, int y, int button) override;

private:
    std::string m_label;
    ButtonCallback m_callback;
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// Label — static text
//-----------------------------------------------------------------------------------------------------------------------------------------
class Label : public UIControl
{
public:
    Label (int x, int y, const char* text, int scale = 1);

    void setText (const char* text);
    const std::string& text () const { return m_text; }

    void draw (DisplayDriver& d) override;

private:
    std::string m_text;
    int m_scale;
};

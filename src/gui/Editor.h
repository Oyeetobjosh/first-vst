#pragma once

#include <memory>

#include "aeffeditor.h"

#include "Controls.h"
#include "ReverbUI.h"

#if defined(_WIN32)
#include "Win32DisplayDriver.h"
#elif defined(__APPLE__)
#include "EditorMac.h"
#include "MacDisplayDriver.h"
#endif

//-----------------------------------------------------------------------------------------------------------------------------------------
// ReverbEditor — the VST 2.4 editor for Tobjosh Reverb.
//
// The plugin creates its own child window on real hosts (VST 2.4 has no
// effEditMouse opcode — the host only hands us the parent window handle).
// On Linux (audio-only build) there is no window, but the whole UI logic
// still runs and is exercised by the headless UI tests.
//-----------------------------------------------------------------------------------------------------------------------------------------
class ReverbEditor : public AEffEditor
{
public:
    explicit ReverbEditor (AudioEffect* effect, ReverbUI::Host& host);
    ~ReverbEditor () override;

    //--- AEffEditor ---------------------------------------------------------------------------
    bool getRect (ERect** rect) override;
    bool open (void* ptr) override;
    void close () override;
    void idle () override;
    bool onKeyDown (VstKeyCode& keyCode) override;
    bool onKeyUp (VstKeyCode& keyCode) override { return false; }
    bool onWheel (float distance) override;

    //--- headless input (tests / windowless hosts) -------------------------------------------
    void mouseDown (int x, int y, int button);
    void mouseMove (int x, int y, int button);
    void mouseUp (int x, int y, int button);
    void dblClick (int x, int y);

    //--- rendering -----------------------------------------------------------------------------
    void render (DisplayDriver& d);
    void paintWindow ();          // renders into the platform driver (WM_PAINT / drawRect)
    void redraw ();               // asks the platform window to repaint

    UI& ui () { return *m_ui; }
    ReverbUI& reverbUI () { return *m_reverbUI; }

private:
    ERect m_rect;
    ReverbUI::Host& m_host;
    std::unique_ptr<UI> m_ui;
    std::unique_ptr<ReverbUI> m_reverbUI;

#if defined(_WIN32)
    HWND m_hwnd;
    std::unique_ptr<Win32DisplayDriver> m_driver;
#elif defined(__APPLE__)
    MacEditorWindow m_mac;
    std::unique_ptr<MacDisplayDriver> m_driver;
#endif
};

#include "Editor.h"

#if defined(_WIN32)

//-----------------------------------------------------------------------------------------------------------------------------------------
// Win32 window procedure — forwards window input to the editor's UI
//-----------------------------------------------------------------------------------------------------------------------------------------
static LRESULT CALLBACK editorWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* editor = reinterpret_cast<ReverbEditor*> (GetWindowLongPtrW (hwnd, GWLP_USERDATA));
    if (!editor)
        return DefWindowProcW (hwnd, msg, wParam, lParam);

    const int x = (int) LOWORD (lParam);
    const int y = (int) HIWORD (lParam);

    switch (msg)
    {
        case WM_PAINT:
            editor->paintWindow ();
            break;
        case WM_MOUSEDOWN:
            editor->mouseDown (x, y, 1);
            break;
        case WM_MOUSEUP:
            editor->mouseUp (x, y, 1);
            break;
        case WM_MOUSEMOVE:
            editor->mouseMove (x, y, 1);
            break;
        case WM_LBUTTONDBLCLK:
            editor->dblClick (x, y);
            break;
        case WM_KEYDOWN:
            editor->ui ().keyDown ((int) wParam, (int) wParam);
            break;
        case WM_MOUSEWHEEL:
            editor->ui ().wheel ((float) (SHORT) HIWORD (wParam) / 120.0f);
            break;
        case WM_DESTROY:
            editor->close ();
            break;
        default:
            return DefWindowProcW (hwnd, msg, wParam, lParam);
    }
    return 0;
}

#elif defined(__APPLE__)

//-----------------------------------------------------------------------------------------------------------------------------------------
// CoreGraphics paint callback invoked from the NSView's drawRect
//-----------------------------------------------------------------------------------------------------------------------------------------
static void paintMacWindow (void* editor, void* driver, int width, int height)
{
    (void) width;
    (void) height;
    auto* ed = static_cast<ReverbEditor*> (editor);
    ed->render (*static_cast<MacDisplayDriver*> (driver));
}

#endif

//-----------------------------------------------------------------------------------------------------------------------------------------
ReverbEditor::ReverbEditor (AudioEffect* effect, ReverbUI::Host& host)
: AEffEditor (effect)
, m_host (host)
, m_ui (new UI (640, 380))
, m_reverbUI (new ReverbUI (host))
#if defined(_WIN32)
, m_hwnd (nullptr)
#endif
{
    m_rect.left   = 0;
    m_rect.top    = 0;
    m_rect.right  = 640;
    m_rect.bottom = 380;

#if defined(__APPLE__)
    m_mac.window = nullptr;
    m_mac.view   = nullptr;
#endif
    m_reverbUI->build (*m_ui);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
ReverbEditor::~ReverbEditor ()
{
    close ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool ReverbEditor::getRect (ERect** rect)
{
    *rect = &m_rect;
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool ReverbEditor::open (void* ptr)
{
    if (!ptr)
        return false;

#if defined(_WIN32)
    const HWND parent = (HWND) ptr;
    if (!parent)
        return false;

    WNDCLASSEXW wc = {};
    wc.cbSize          = sizeof (wc);
    wc.lpfnWndProc     = editorWndProc;
    wc.hInstance       = GetModuleHandleW (nullptr);
    wc.lpszClassName   = L"TobjoshReverbEditor";
    wc.hCursor         = LoadCursor (nullptr, IDC_ARROW);
    RegisterClassExW (&wc);

    m_hwnd = CreateWindowExW (0, wc.lpszClassName, L"Tobjosh Reverb",
                              WS_CHILD | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, 640, 380,
                              parent, nullptr, wc.hInstance, nullptr);
    if (!m_hwnd)
        return false;

    SetWindowLongPtrW (m_hwnd, GWLP_USERDATA, (LONG_PTR) this);
    m_driver = std::make_unique<Win32DisplayDriver> (m_hwnd, 640, 380);
    redraw ();
    return true;
#elif defined(__APPLE__)
    m_driver = std::make_unique<MacDisplayDriver> (640, 380);
    return macEditorCreateWindow (m_mac, ptr, 640, 380, m_driver.get (), this, paintMacWindow);
#else
    return false;   // audio-only build (Linux)
#endif
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEditor::close ()
{
#if defined(_WIN32)
    if (m_hwnd)
    {
        DestroyWindow (m_hwnd);
        m_hwnd = nullptr;
    }
#elif defined(__APPLE__)
    macEditorDestroyWindow (m_mac);
#endif
#if defined(_WIN32) || defined(__APPLE__)
    m_driver.reset ();
#endif
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEditor::idle ()
{
    // redraws happen on demand (param changes call redraw())
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool ReverbEditor::onKeyDown (VstKeyCode& keyCode)
{
    m_ui->keyDown (keyCode.character, keyCode.virt);
    return true;
}

//---------------------------------------------------------------------------------------------------
bool ReverbEditor::onWheel (float distance)
{
    m_ui->wheel (distance);
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEditor::mouseDown (int x, int y, int button) { m_ui->mouseDown (x, y, button); }
void ReverbEditor::mouseMove (int x, int y, int button) { m_ui->mouseMove (x, y, button); }
void ReverbEditor::mouseUp (int x, int y, int button)   { m_ui->mouseUp (x, y, button); }
void ReverbEditor::dblClick (int x, int y)              { m_ui->dblClick (x, y); }

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEditor::render (DisplayDriver& d)
{
    m_reverbUI->render (d, *m_ui);
    d.present ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEditor::paintWindow ()
{
#if defined(_WIN32) || defined(__APPLE__)
    if (m_driver)
        render (*m_driver);
#endif
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEditor::redraw ()
{
#if defined(_WIN32)
    if (m_hwnd)
        InvalidateRect (m_hwnd, nullptr, TRUE);
#elif defined(__APPLE__)
    macEditorRedraw (m_mac);
#endif
}

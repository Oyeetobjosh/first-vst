#import <Cocoa/Cocoa.h>

#include "EditorMac.h"
#include "MacDisplayDriver.h"

namespace
{
    class EditorView : public NSView
    {
    public:
        EDITOR_VIEW_META_DATA

        MacPaintCallback m_paint;
        void* m_editor;
        void* m_driver;

        bool initWithFrame (CGRect frame, MacPaintCallback paint, void* editor, void* driver)
        {
            if (!NSView::initWithFrame (frame))
                return false;
            m_paint = paint;
            m_editor = editor;
            m_driver = driver;
            setAutoresizingMask (NSViewAutoresizingMaskWidthSizable | NSViewAutoresizingMaskHeightSizable);
            return true;
        }

        void drawRect (CGRect) override
        {
            CGContextRef ctx = (CGContextRef) NSGraphicsContext::currentContext ()->CGContext;
            if (auto* driver = static_cast<MacDisplayDriver*> (m_driver))
            {
                driver->setContext (ctx);
                if (m_paint)
                    m_paint (m_editor, driver, (int) bounds.size.width, (int) bounds.size.height);
            }
        }
    };
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool macEditorCreateWindow (MacEditorWindow& w, void* parentWindow, int width, int height,
                            void* driver, void* editor, MacPaintCallback paint)
{
    (void) parentWindow;   // Cocoa hosts: window is created standalone (common VST practice)

    NSWindow* window = [[NSWindow alloc] initWithContentRect: NSMakeRect (0, 0, width, height)
                                                   styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                     backing: NSWindowBackingBuffered
                                                       defer: NO];
    if (!window)
        return false;

    [window setTitle: @ "Tobjosh Reverb"];
    [window setReleasesWhenClosed: YES];

    EditorView* view = [[EditorView alloc] initWithFrame: NSMakeRect (0, 0, width, height)
                                                   paint: paint
                                                 editor: editor
                                                   driver: driver];
    [window setContentView: view];
    [window makeKeyAndOrderFront: nil];

    w.window = (__bridge void*) window;
    w.view   = (__bridge void*) view;
    return true;
}

//---------------------------------------------------------------------------------------------------
void macEditorDestroyWindow (MacEditorWindow& w)
{
    if (auto* window = (__bridge NSWindow*) w.window)
        [window close];
    w.window = nullptr;
    w.view   = nullptr;
}

//---------------------------------------------------------------------------------------------------
void macEditorRedraw (MacEditorWindow& w)
{
    if (auto* view = (__bridge EditorView*) w.view)
        [view setNeedsDisplay: YES];
}

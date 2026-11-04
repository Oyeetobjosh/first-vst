#pragma once

//-----------------------------------------------------------------------------------------------------------------------------------------
// Bridge to the macOS Cocoa editor window (implemented in EditorMac.mm).
// Kept as a plain C interface so the C++ editor core stays ObjC-free.
//-----------------------------------------------------------------------------------------------------------------------------------------
#if !defined(__APPLE__)
#error "EditorMac is only for macOS builds"
#endif

struct MacEditorWindow
{
    void* window;   // NSWindow*
    void* view;     // NSView*
};

// Called by the NSView's drawRect with the driver's CoreGraphics context already attached
using MacPaintCallback = void (*) (void* editor, void* driver, int width, int height);

bool macEditorCreateWindow (MacEditorWindow& w, void* parentWindow, int width, int height,
                            void* driver, void* editor, MacPaintCallback paint);
void macEditorDestroyWindow (MacEditorWindow& w);
void macEditorRedraw (MacEditorWindow& w);

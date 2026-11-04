//----------------------------------------------------------------------------------------------------------------------------------------
// FirstVST Delay — VST 2.4 plug-in entry point
//----------------------------------------------------------------------------------------------------------------------------------------
// The VST 2.4 host-loading protocol is:
//
//   1. The host loads the shared library and looks for the exported symbol
//      "VSTPluginMain" (provided by the SDK's vstplugmain.cpp).
//   2. The host calls it with its audioMasterCallback and receives the
//      AEffect struct of the plug-in.
//
// VSTPluginMain calls createEffectInstance() below, which instantiates the
// plug-in class.
//----------------------------------------------------------------------------------------------------------------------------------------

#include "FirstVSTDelay.h"

//----------------------------------------------------------------------------------------------------------------------------------------
/** Creates the plug-in instance. Called by the SDK's VSTPluginMain(). */
//----------------------------------------------------------------------------------------------------------------------------------------
AudioEffect* createEffectInstance (audioMasterCallback audioMaster)
{
    return new FirstVSTDelay (audioMaster);
}

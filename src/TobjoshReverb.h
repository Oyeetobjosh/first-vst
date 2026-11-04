#pragma once

#include <vector>

#include "audioeffectx.h"

#include "gui/Editor.h"
#include "gui/ReverbUI.h"
#include "reverb/ReverbEngine.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
// Tobjosh Reverb — a 360° room-audio reverb VST 2.4 plugin.
//
// Parameters (in UI order):
//   0  SIZE      decay time of the diffuse tail (0.2 .. 6.0 s)
//   1  DAMPING   low-pass amount on the tail (0 .. 1)
//   2  360°      azimuth of the reverb position in the room (0 .. 360)
//   3  MIX       wet/dry mix (0 .. 1)
//
// Programs (presets): Rooms, Hall, Cathedral, Space
//-----------------------------------------------------------------------------------------------------------------------------------------
class TobjoshReverb : public AudioEffectX, public ReverbUI::Host
{
public:
    TobjoshReverb (audioMasterCallback audioMaster);

    //--- AudioEffect ---------------------------------------------------------------------
    void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) override;
    void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames) override;
    void setSampleRate (float sampleRate) override;
    void setBlockSize (VstInt32 blockSize) override;
    void setParameter (VstInt32 index, float value) override;
    float getParameter (VstInt32 index) override;
    void getParameterName (VstInt32 index, char* text) override;
    void getParameterDisplay (VstInt32 index, char* text) override;
    void getParameterLabel (VstInt32 index, char* text) override;

    //--- programs ------------------------------------------------------------------------
    void setProgram (VstInt32 program) override;
    void setProgramName (char* name) override;
    void getProgramName (char* name) override;

    //--- identity ------------------------------------------------------------------------
    bool getEffectName (char* name) override;
    bool getVendorString (char* text) override;
    bool getProductString (char* text) override;
    VstPlugCategory getPlugCategory () override { return kPlugCategEffect; }
    VstInt32 getVendorVersion () override { return 1; }

    //--- state chunks -------------------------------------------------------------------
    VstInt32 getChunk (void** data, bool isPreset = false) override;
    VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset = false) override;

    //--- ReverbUI::Host ------------------------------------------------------------------
    float getParam (int index) const override;
    void setParam (int index, float value) override;
    void setBypassLED (bool on) override;
    bool isBypassed () const override;

    //--- VST soft bypass (effSetBypass) ---------------------------------------------------
    bool setBypass (bool on) override;

    ReverbEditor* editor () const { return m_editor; }

private:
    void updateEngine ();
    void applyProgram (int index);
    bool matchProgram (const char* name);

    static const int numParams  = ReverbUI::numParams;
    static const int numPrograms = ReverbUI::numPresets;

    float m_params[numParams];
    bool m_bypass;

    ReverbEngine m_engine;
    ReverbEditor* m_editor;

    // double-replacing conversion buffers
    std::vector<float> m_tmpIn[2];
    std::vector<float> m_tmpOut[2];
};

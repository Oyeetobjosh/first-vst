#include "TobjoshReverb.h"

#include <cmath>
#include <cstring>

#include "presets.h"

//-----------------------------------------------------------------------------------------------------------------------------------------
TobjoshReverb::TobjoshReverb (audioMasterCallback audioMaster)
: AudioEffectX (audioMaster, ReverbUI::numPresets, ReverbUI::numParams)
, m_bypass (false)
, m_editor (nullptr)
{
    setUniqueID (CCONST ('T', 'B', 'R', 'b'));
    setNumInputs (2);
    setNumOutputs (2);
    canProcessReplacing ();
    canDoubleReplacing ();
    programsAreChunks (false);

    // default parameters
    m_params[0] = 1.0f;    // size
    m_params[1] = 0.40f;   // damping
    m_params[2] = 0.0f;    // angle
    m_params[3] = 0.35f;   // mix

    updateEngine ();

    m_editor = new ReverbEditor (this, *this);
    setEditor (m_editor);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
//--- AudioEffect ---------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------
bool TobjoshReverb::getEffectName (char* name)
{
    if (name)
    {
        std::strncpy (name, "Tobjosh Reverb", kVstMaxEffectNameLen);
        name[kVstMaxEffectNameLen - 1] = 0;
    }
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool TobjoshReverb::getVendorString (char* text)
{
    if (text)
    {
        std::strncpy (text, "Oyeetobjosh", kVstMaxVendorStrLen);
        text[kVstMaxVendorStrLen - 1] = 0;
    }
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool TobjoshReverb::getProductString (char* text)
{
    if (text)
    {
        std::strncpy (text, "Tobjosh Reverb 1.0.0", kVstMaxProductStrLen);
        text[kVstMaxProductStrLen - 1] = 0;
    }
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
{
    m_engine.process (inputs[0], inputs[1], outputs[0], outputs[1], sampleFrames);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames)
{
    for (int c = 0; c < 2; ++c)
    {
        m_tmpIn[c].resize (sampleFrames);
        m_tmpOut[c].resize (sampleFrames);
        for (VstInt32 i = 0; i < sampleFrames; ++i)
            m_tmpIn[c][i] = (float) inputs[c][i];
    }

    float* inPtrs[2] = { m_tmpIn[0].data (), m_tmpIn[1].data () };
    float* outPtrs[2] = { m_tmpOut[0].data (), m_tmpOut[1].data () };
    processReplacing (inPtrs, outPtrs, sampleFrames);

    for (int c = 0; c < 2; ++c)
        for (VstInt32 i = 0; i < sampleFrames; ++i)
            outputs[c][i] = (double) m_tmpOut[c][i];
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setSampleRate (float sampleRate)
{
    m_engine.setSampleRate (sampleRate);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setBlockSize (VstInt32 blockSize)
{
    for (int c = 0; c < 2; ++c)
    {
        m_tmpIn[c].resize (blockSize);
        m_tmpOut[c].resize (blockSize);
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setParameter (VstInt32 index, float value)
{
    setParam (index, value);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
float TobjoshReverb::getParameter (VstInt32 index)
{
    return getParam (index);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::getParameterName (VstInt32 index, char* text)
{
    if (!text)
        return;
    const char* names[] = { "Size", "Damping", "360°", "Mix" };
    std::strncpy (text, (index >= 0 && index < numParams) ? names[index] : "", kVstMaxParamStrLen);
    text[kVstMaxParamStrLen - 1] = 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::getParameterLabel (VstInt32 index, char* text)
{
    if (!text)
        return;
    const char* labels[] = { "s", "%", "°", "%" };
    std::strncpy (text, (index >= 0 && index < numParams) ? labels[index] : "", kVstMaxParamStrLen);
    text[kVstMaxParamStrLen - 1] = 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::getParameterDisplay (VstInt32 index, char* text)
{
    if (!text)
        return;

    if (index == 0)
        std::snprintf (text, kVstMaxParamStrLen, "%.2f s", m_params[0]);
    else if (index == 1)
        std::snprintf (text, kVstMaxParamStrLen, "%.0f %%", m_params[1] * 100.0f);
    else if (index == 2)
        std::snprintf (text, kVstMaxParamStrLen, "%.0f °", m_params[2]);
    else if (index == 3)
        std::snprintf (text, kVstMaxParamStrLen, "%.0f %%", m_params[3] * 100.0f);
    else
        text[0] = 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
//--- programs --------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::getProgramName (char* name)
{
    if (!name)
        return;
    const int cur = getProgram ();
    const char* p = (cur >= 0 && cur < numPrograms) ? reverbPresets[cur].name : "Tobjosh Reverb";
    std::strncpy (name, p, kVstMaxProgNameLen);
    name[kVstMaxProgNameLen - 1] = 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setProgram (VstInt32 program)
{
    AudioEffect::setProgram (program);
    applyProgram (program);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setProgramName (char* name)
{
    if (matchProgram (name))
        applyProgram (getProgram ());
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool TobjoshReverb::matchProgram (const char* name)
{
    if (!name)
        return false;
    for (int i = 0; i < numPrograms; ++i)
        if (std::strcmp (name, reverbPresets[i].name) == 0)
        {
            setProgram (i);
            return true;
        }
    return false;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::applyProgram (int index)
{
    if (index < 0 || index >= numPrograms)
        return;

    const ReverbPreset& p = reverbPresets[index];
    m_params[0] = p.size;
    m_params[1] = p.damping;
    m_params[2] = p.angle;
    m_params[3] = p.mix;
    updateEngine ();

    if (m_editor)
        m_editor->redraw ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
//--- state chunks ----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------
VstInt32 TobjoshReverb::getChunk (void** data, bool /*isPreset*/)
{
    const VstInt32 chunkSize = (VstInt32) (sizeof (float) * 6);   // version + 4 params + bypass
    *data = std::malloc (chunkSize);
    if (*data)
    {
        float* f = (float*) *data;
        f[0] = 1.0f;                          // version
        f[1] = m_params[0];
        f[2] = m_params[1];
        f[3] = m_params[2];
        f[4] = m_params[3];
        f[5] = m_bypass ? 1.0f : 0.0f;
    }
    return chunkSize;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
VstInt32 TobjoshReverb::setChunk (void* data, VstInt32 byteSize, bool /*isPreset*/)
{
    if (!data || byteSize < (VstInt32) (sizeof (float) * 6))
        return 0;

    const float* f = (const float*) data;
    if (f[0] < 1.0f)
        return 0;

    m_params[0] = std::max (0.2f, std::min (6.0f, f[1]));
    m_params[1] = std::max (0.0f, std::min (1.0f, f[2]));
    m_params[2] = std::fmod (f[3], 360.0f);
    if (m_params[2] < 0.0f)
        m_params[2] += 360.0f;
    m_params[3] = std::max (0.0f, std::min (1.0f, f[4]));
    m_bypass = f[5] >= 0.5f;
    m_engine.setBypass (m_bypass);

    updateEngine ();

    if (m_editor)
        m_editor->redraw ();

    return (VstInt32) (sizeof (float) * 6);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
//--- ReverbUI::Host -----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------------------
float TobjoshReverb::getParam (int index) const
{
    if (index < 0 || index >= numParams)
        return 0.0f;
    return m_params[index];
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setParam (int index, float value)
{
    if (index < 0 || index >= numParams)
        return;

    m_params[index] = value;
    updateEngine ();

    if (m_editor)
        m_editor->redraw ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::setBypassLED (bool on)
{
    m_bypass = on;
    m_engine.setBypass (on);

    if (m_editor)
        m_editor->redraw ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool TobjoshReverb::setBypass (bool on)
{
    setBypassLED (on);
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool TobjoshReverb::isBypassed () const
{
    return m_bypass;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void TobjoshReverb::updateEngine ()
{
    m_engine.setSize (m_params[0]);
    m_engine.setDamping (m_params[1]);
    m_engine.setAngle (m_params[2]);
    m_engine.setMix (m_params[3]);
}

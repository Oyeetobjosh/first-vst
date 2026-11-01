#pragma once

#include <vector>

#include "audioeffectx.h"    // VST SDK base class (AudioEffectX)

/*!
    FirstVST Delay — a stereo ping-pong delay implemented as a VST 2.4 plug-in.

    This is the "hello world" of this repository: a real, loadable VST effect
    written from scratch against the Steinberg VST SDK 2.4. It demonstrates the
    core parts of the VST 2.4 plug-in API:

    - parameters (name / display / label) with automation support
    - preset persistence through chunks (effGetChunk / effSetChunk)
    - audio processing in 32- and 64-bit (processReplacing /
      processDoubleReplacing)
    - latency reporting (getGetTailSize — the SDK's intentionally misspelled
      method name, kept for API compatibility)
    - speaker arrangement negotiation (stereo)

    Parameters (all normalized 0..1):
      0: Time        delay time, logarithmic 0.1 ms .. 1000 ms
      1: Feedback    echo feedback 0 .. 0.95
      2: Mix         dry/wet blend 0 .. 1
*/
class FirstVSTDelay : public AudioEffectX
{
public:
    FirstVSTDelay (audioMasterCallback audioMaster);
    ~FirstVSTDelay () override;

    // --- state transitions ------------------------------------------------------
    void open () override;
    void close () override;
    void suspend () override;
    void resume () override;
    void setSampleRate (float sampleRate) override;
    void setBlockSize (VstInt32 blockSize) override;

    // --- parameters ------------------------------------------------------------
    void setParameter (VstInt32 index, float value) override;
    float getParameter (VstInt32 index) override;
    void getParameterLabel (VstInt32 index, char* label) override;
    void getParameterDisplay (VstInt32 index, char* text) override;
    void getParameterName (VstInt32 index, char* text) override;

    // --- programs / persistence -------------------------------------------------
    void setProgramName (char* name) override;
    void getProgramName (char* name) override;
    VstInt32 getChunk (void** data, bool isPreset = false) override;
    VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset = false) override;

    // --- processing ------------------------------------------------------------
    void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) override;
    void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames) override;

    // --- identification / misc --------------------------------------------------
    bool getEffectName (char* name) override;
    bool getVendorString (char* text) override;
    bool getProductString (char* text) override;
    VstInt32 getVendorVersion () override;
    VstPlugCategory getPlugCategory () override;
    bool getSpeakerArrangement (VstSpeakerArrangement** pluginInput, VstSpeakerArrangement** pluginOutput) override;
    VstInt32 getGetTailSize () override;   // (misspelled in the SDK — see audioeffectx.h)

private:
    enum ParamIndex
    {
        kParamTime = 0,
        kParamFeedback = 1,
        kParamMix = 2,
        kNumParams = 3
    };

    // DSP helpers
    void allocateDelayBuffers ();
    double delayTimeMs () const;
    static double sampleAt (const double* buf, double pos, size_t len);

    // Shared audio core; instantiated for float and double sample types.
    template <typename SampleType>
    void processImpl (SampleType** inputs, SampleType** outputs, VstInt32 sampleFrames);

    float m_params[kNumParams];                 // normalized parameter values (0..1)
    char m_programName[64];                     // current preset name

    // Ping-pong delay lines: buffer L is fed by the left input (plus the
    // right delay line's echo), buffer R by the right input (plus the left
    // delay line's echo). Read positions lag the write positions by the
    // delay length.
    std::vector<double> m_delayL;
    std::vector<double> m_delayR;
    size_t m_bufferLen;
    size_t m_writePosL;
    size_t m_writePosR;
    double m_readPosL;
    double m_readPosR;

    VstSpeakerArrangement m_inputArrangement;
    VstSpeakerArrangement m_outputArrangement;
};

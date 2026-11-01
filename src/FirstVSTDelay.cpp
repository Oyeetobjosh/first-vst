#include "FirstVSTDelay.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

//-----------------------------------------------------------------------------------------------------------------------------------------
// Chunk format for preset persistence (saved by hosts via effGetChunk/effSetChunk)
//-----------------------------------------------------------------------------------------------------------------------------------------
#define kFirstVSTChunkVersion 1

struct DelayChunkData
{
    VstInt32 version;
    float time;       // normalized (0..1)
    float feedback;   // normalized (0..1)
    float mix;        // normalized (0..1)
};

//-----------------------------------------------------------------------------------------------------------------------------------------
// Time parameter mapping (logarithmic): 0.1 ms .. 1000 ms
//-----------------------------------------------------------------------------------------------------------------------------------------
static const double kMinTimeMs = 0.1;
static const double kMaxTimeMs = 1000.0;

//-----------------------------------------------------------------------------------------------------------------------------------------
FirstVSTDelay::FirstVSTDelay (audioMasterCallback audioMaster)
: AudioEffectX (audioMaster, 1, kNumParams)
{
    setUniqueID (CCONST ('F', 'V', 'S', '1'));
    setNumInputs (2);
    setNumOutputs (2);
    canProcessReplacing ();
    canDoubleReplacing ();
    programsAreChunks (true);

    m_params[kParamTime] = 0.6f;       // ~250 ms
    m_params[kParamFeedback] = 0.35f;
    m_params[kParamMix] = 0.4f;

    std::snprintf (m_programName, sizeof (m_programName), "FirstVST Delay");

    m_bufferLen = 0;
    m_writePosL = 0;
    m_writePosR = 0;
    m_readPosL = 0;
    m_readPosR = 0;

    memset (&m_inputArrangement, 0, sizeof (m_inputArrangement));
    memset (&m_outputArrangement, 0, sizeof (m_outputArrangement));
}

//-----------------------------------------------------------------------------------------------------------------------------------------
FirstVSTDelay::~FirstVSTDelay ()
{
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::open ()
{
    allocateDelayBuffers ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::close ()
{
    m_delayL.clear ();
    m_delayR.clear ();
    m_bufferLen = 0;
    m_writePosL = m_writePosR = 0;
    m_readPosL = m_readPosR = 0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::suspend () {}
void FirstVSTDelay::resume () {}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::setSampleRate (float sampleRate)
{
    AudioEffect::setSampleRate (sampleRate);
    allocateDelayBuffers ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::setBlockSize (VstInt32 blockSize)
{
    AudioEffect::setBlockSize (blockSize);
    // The delay buffers are sized by the delay length, not the block size,
    // so there is nothing to reallocate here.
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::setParameter (VstInt32 index, float value)
{
    if (index < 0 || index >= kNumParams)
        return;

    m_params[index] = value;

    if (index == kParamTime)
        allocateDelayBuffers ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
float FirstVSTDelay::getParameter (VstInt32 index)
{
    if (index < 0 || index >= kNumParams)
        return 0.f;

    return m_params[index];
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::getParameterLabel (VstInt32 index, char* label)
{
    if (!label)
        return;

    switch (index)
    {
        case kParamTime:     std::snprintf (label, 32, "ms"); break;
        case kParamFeedback: std::snprintf (label, 32, "%"); break;
        case kParamMix:      std::snprintf (label, 32, "%"); break;
        default:             *label = 0; break;
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::getParameterDisplay (VstInt32 index, char* text)
{
    if (!text)
        return;

    switch (index)
    {
        case kParamTime:
            std::snprintf (text, 32, "%.1f", delayTimeMs ());
            break;
        case kParamFeedback:
            std::snprintf (text, 32, "%.0f %%", m_params[kParamFeedback] * 100.0f);
            break;
        case kParamMix:
            std::snprintf (text, 32, "%.0f %%", m_params[kParamMix] * 100.0f);
            break;
        default:
            *text = 0;
            break;
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::getParameterName (VstInt32 index, char* text)
{
    if (!text)
        return;

    switch (index)
    {
        case kParamTime:     std::snprintf (text, 32, "Time"); break;
        case kParamFeedback: std::snprintf (text, 32, "Feedback"); break;
        case kParamMix:      std::snprintf (text, 32, "Mix"); break;
        default:             *text = 0; break;
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::setProgramName (char* name)
{
    if (!name)
        return;

    std::snprintf (m_programName, sizeof (m_programName), "%s", name);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::getProgramName (char* name)
{
    if (!name)
        return;

    std::snprintf (name, 64, "%s", m_programName);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
VstInt32 FirstVSTDelay::getChunk (void** data, bool isPreset)
{
    DelayChunkData chunk;
    chunk.version = kFirstVSTChunkVersion;
    chunk.time = m_params[kParamTime];
    chunk.feedback = m_params[kParamFeedback];
    chunk.mix = m_params[kParamMix];

    // The plug-in allocates the buffer; the host is responsible for freeing it.
    *data = std::malloc (sizeof (DelayChunkData));
    if (!*data)
        return 0;

    std::memcpy (*data, &chunk, sizeof (chunk));
    return (VstInt32) sizeof (chunk);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
VstInt32 FirstVSTDelay::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    if (!data || byteSize < (VstInt32) sizeof (DelayChunkData))
        return 0;

    DelayChunkData chunk;
    std::memcpy (&chunk, data, sizeof (chunk));

    if (chunk.version != kFirstVSTChunkVersion)
        return 0;

    setParameter (kParamTime, chunk.time);
    setParameter (kParamFeedback, chunk.feedback);
    setParameter (kParamMix, chunk.mix);
    return 1;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Audio processing (shared core for float and double sample types)
//-----------------------------------------------------------------------------------------------------------------------------------------
template <typename SampleType>
void FirstVSTDelay::processImpl (SampleType** inputs, SampleType** outputs, VstInt32 sampleFrames)
{
    if (sampleFrames <= 0)
        return;

    // Without allocated buffers (e.g. before open()) pass the audio through.
    if (m_bufferLen < 2 || m_delayL.empty () || m_delayR.empty ())
    {
        for (VstInt32 i = 0; i < sampleFrames; ++i)
        {
            outputs[0][i] = inputs[0][i];
            outputs[1][i] = inputs[1][i];
        }
        return;
    }

    const double delayLen = delayTimeMs () / 1000.0 * sampleRate;
    const double mix = m_params[kParamMix];
    const double feedback = m_params[kParamFeedback];
    const size_t len = m_bufferLen;

    double* bufL = m_delayL.data ();
    double* bufR = m_delayR.data ();
    size_t wl = m_writePosL;
    size_t wr = m_writePosR;
    double rl = m_readPosL;
    double rr = m_readPosR;

    for (VstInt32 i = 0; i < sampleFrames; ++i)
    {
        const double inL = (double) inputs[0][i];
        const double inR = (double) inputs[1][i];

        // Ping-pong readout: left delay line feeds the left output, right
        // delay line feeds the right output.
        const double delayedL = sampleAt (bufL, rl, len);
        const double delayedR = sampleAt (bufR, rr, len);

        outputs[0][i] = (SampleType) (inL * (1.0 - mix) + delayedL * mix);
        outputs[1][i] = (SampleType) (inR * (1.0 - mix) + delayedR * mix);

        // Cross-feed the echoes: each delay line is driven by its own input
        // plus the echo coming back from the other delay line.
        bufL[wl] = inL + feedback * delayedR;
        bufR[wr] = inR + feedback * delayedL;

        wl = (wl + 1) % len;
        wr = (wr + 1) % len;
        rl += 1.0;
        rr += 1.0;
        if (rl >= (double) len) rl -= (double) len;
        if (rr >= (double) len) rr -= (double) len;
    }

    m_writePosL = wl;
    m_writePosR = wr;
    m_readPosL = rl;
    m_readPosR = rr;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames)
{
    processImpl (inputs, outputs, sampleFrames);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames)
{
    processImpl (inputs, outputs, sampleFrames);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
double FirstVSTDelay::sampleAt (const double* buf, double pos, size_t len)
{
    const size_t idx = (size_t) pos;                 // pos is in [0, len)
    const size_t next = (idx + 1) % len;
    const double frac = pos - (double) idx;
    return buf[idx] + (buf[next] - buf[idx]) * frac;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
double FirstVSTDelay::delayTimeMs () const
{
    const double v = m_params[kParamTime];           // 0..1
    return kMinTimeMs * std::pow (kMaxTimeMs / kMinTimeMs, v);
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Delay buffers are sized to the current delay length (plus headroom for
// fractional read positions). They only ever grow within one delay time;
// growing resets the buffers to silence (a preset load, not a hot path).
//-----------------------------------------------------------------------------------------------------------------------------------------
void FirstVSTDelay::allocateDelayBuffers ()
{
    const double delayLen = delayTimeMs () / 1000.0 * sampleRate;
    size_t newLen = (size_t) std::ceil (delayLen) + 2;

    if (newLen < m_bufferLen)
        newLen = m_bufferLen;

    if (newLen != m_bufferLen)
    {
        m_delayL.assign (newLen, 0.0);
        m_delayR.assign (newLen, 0.0);
        m_bufferLen = newLen;
        m_writePosL = 0;
        m_writePosR = 0;
    }

    // Keep the read positions lagging the write positions by the current
    // delay length (in fractional samples), even when the buffer size did
    // not change.
    m_readPosL = std::fmod ((double) m_writePosL - delayLen, (double) m_bufferLen);
    m_readPosR = std::fmod ((double) m_writePosR - delayLen, (double) m_bufferLen);
    if (m_readPosL < 0.0)
        m_readPosL += (double) m_bufferLen;
    if (m_readPosR < 0.0)
        m_readPosR += (double) m_bufferLen;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Identification
//-----------------------------------------------------------------------------------------------------------------------------------------
bool FirstVSTDelay::getEffectName (char* name)
{
    if (!name)
        return false;
    std::snprintf (name, 64, "FirstVST Delay");
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool FirstVSTDelay::getVendorString (char* text)
{
    if (!text)
        return false;
    std::snprintf (text, 64, "First VST");
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool FirstVSTDelay::getProductString (char* text)
{
    if (!text)
        return false;
    std::snprintf (text, 64, "FirstVST Delay");
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
VstInt32 FirstVSTDelay::getVendorVersion ()
{
    return 1000;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
VstPlugCategory FirstVSTDelay::getPlugCategory ()
{
    return kPlugCategRoomFx;   // delays and reverbs
}

//-----------------------------------------------------------------------------------------------------------------------------------------
bool FirstVSTDelay::getSpeakerArrangement (VstSpeakerArrangement** pluginInput, VstSpeakerArrangement** pluginOutput)
{
    m_inputArrangement.type = kSpeakerArrStereo;
    m_inputArrangement.numChannels = 2;
    m_inputArrangement.speakers[0].type = kSpeakerL;
    m_inputArrangement.speakers[1].type = kSpeakerR;

    m_outputArrangement.type = kSpeakerArrStereo;
    m_outputArrangement.numChannels = 2;
    m_outputArrangement.speakers[0].type = kSpeakerL;
    m_outputArrangement.speakers[1].type = kSpeakerR;

    *pluginInput = &m_inputArrangement;
    *pluginOutput = &m_outputArrangement;
    return true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
// Latency: the delay line reports its delay length (in samples) so the host
// can compensate for the added latency.
//-----------------------------------------------------------------------------------------------------------------------------------------
VstInt32 FirstVSTDelay::getGetTailSize ()
{
    return (VstInt32) std::ceil (delayTimeMs () / 1000.0 * sampleRate);
}

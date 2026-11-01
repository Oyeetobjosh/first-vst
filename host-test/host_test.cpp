//----------------------------------------------------------------------------------------------------------------------------------------
// FirstVST Delay — headless VST 2.4 test host
//
// Loads the built plug-in like a real DAW would (dlopen/LoadLibrary +
// VSTPluginMain) and exercises it:
//   - open / setup / flags
//   - parameter names, displays and labels
//   - DSP: impulse response of the ping-pong delay (float + double)
//   - latency reporting (effGetTailSize)
//   - chunk persistence (preset save/restore)
//   - programs and identification strings
//
// Usage:  vst-host-test [path-to-plugin]
// Exit code 0 = all checks passed, 1 = failure.
//----------------------------------------------------------------------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE lib_handle;
    #define HOST_OPEN(path) LoadLibraryA (path)
    #define HOST_SYM(h, n) GetProcAddress (h, n)
    #define HOST_CLOSE(h)   FreeLibrary (h)
#else
    #include <dlfcn.h>
    typedef void* lib_handle;
    #define HOST_OPEN(path) dlopen (path, RTLD_NOW)
    #define HOST_SYM(h, n)  dlsym (h, n)
    #define HOST_CLOSE(h)   dlclose (h)
#endif

#include "audioeffectx.h"    // for AudioEffect (to delete the instance cleanly)

typedef AEffect* (*VstPluginMainFn) (audioMasterCallback);

static int g_failures = 0;

static void report (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Minimal host-side callback (the plug-in calls back into the host)
//----------------------------------------------------------------------------------------------------------------------------------------
static VstIntPtr hostCallback (AEffect* effect, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
    (void) effect;
    (void) index;
    (void) value;
    (void) ptr;
    (void) opt;

    if (opcode == audioMasterVersion)
        return (VstIntPtr) 2400;      // this host is VST 2.4
    return (VstIntPtr) 0;
}

//----------------------------------------------------------------------------------------------------------------------------------------
static VstIntPtr dispatch (AEffect* effect, VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void* ptr = nullptr, float opt = 0.0f)
{
    return effect->dispatcher (effect, opcode, index, value, ptr, opt);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Delay-time mapping must match FirstVSTDelay.cpp: 0.1 ms .. 1000 ms, log scale
//----------------------------------------------------------------------------------------------------------------------------------------
static double timeParamFor (double targetMs)
{
    const double kMinTimeMs = 0.1;
    const double kMaxTimeMs = 1000.0;
    return std::log (targetMs / kMinTimeMs) / std::log (kMaxTimeMs / kMinTimeMs);
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testParameters (AEffect* effect)
{
    std::printf ("Parameters:\n");

    report (effect->numParams == 3, "AEffect::numParams reports 3 parameters");

    char text[64];
    dispatch (effect, effGetParamName, 0, 0, text);
    report (std::strcmp (text, "Time") == 0, "param 0 name is 'Time'");
    dispatch (effect, effGetParamName, 1, 0, text);
    report (std::strcmp (text, "Feedback") == 0, "param 1 name is 'Feedback'");
    dispatch (effect, effGetParamName, 2, 0, text);
    report (std::strcmp (text, "Mix") == 0, "param 2 name is 'Mix'");

    dispatch (effect, effGetParamLabel, 0, 0, text);
    report (std::strcmp (text, "ms") == 0, "param 0 label is 'ms'");

    // Display for a known value: set Time to 250 ms and expect "250.0"
    effect->setParameter (effect, 0, timeParamFor (250.0));
    dispatch (effect, effGetParamDisplay, 0, 0, text);
    report (std::strcmp (text, "250.0") == 0, "param 0 displays '250.0' for 250 ms");

    effect->setParameter (effect, 1, 0.35f);
    dispatch (effect, effGetParamDisplay, 1, 0, text);
    report (std::strcmp (text, "35 %") == 0, "param 1 displays '35 %'");

    report (fabs (effect->getParameter (effect, 0) - timeParamFor (250.0)) < 1e-6f,
            "param 0 round-trips through getParameter");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Feed an impulse through the delay and verify the echo positions/amplitudes
//----------------------------------------------------------------------------------------------------------------------------------------
static void testDelayDsp (AEffect* effect, bool useDouble)
{
    const double targetMs = 200.0;
    const VstInt32 sampleRate = 44100;
    const VstInt32 delaySamples = (VstInt32) std::round (targetMs / 1000.0 * sampleRate);   // 8820

    effect->setParameter (effect, 0, timeParamFor (targetMs));   // Time
    effect->setParameter (effect, 1, 0.9f);                      // Feedback
    effect->setParameter (effect, 2, 1.0f);                      // Mix (fully wet)

    const VstInt32 neededSamples = 3 * delaySamples + 256;
    const VstInt32 blockSize = 128;
    const VstInt32 totalSamples = ((neededSamples + blockSize - 1) / blockSize) * blockSize;
    const VstInt32 numBlocks = totalSamples / blockSize;

    std::vector<double> outL (totalSamples, 0.0), outR (totalSamples, 0.0);
    std::vector<float> outLf (totalSamples, 0.0f), outRf (totalSamples, 0.0f);

    const char* tag = useDouble ? "64-bit" : "32-bit";
    std::printf ("Delay DSP (%s):\n", tag);

    for (VstInt32 b = 0; b < numBlocks; ++b)
    {
        if (b == 0)
        {
            if (useDouble)
            {
                double inL[blockSize] = { 1.0 };
                double inR[blockSize] = { 0.0 };
                double* inputs[2] = { inL, inR };
                double* outputs[2] = { outL.data () + b * blockSize, outR.data () + b * blockSize };
                effect->processDoubleReplacing (effect, inputs, outputs, blockSize);
            }
            else
            {
                float inL[blockSize] = { 1.0f };
                float inR[blockSize] = { 0.0f };
                float* inputs[2] = { inL, inR };
                float* outputs[2] = { outLf.data () + b * blockSize, outRf.data () + b * blockSize };
                effect->processReplacing (effect, inputs, outputs, blockSize);
            }
        }
        else
        {
            if (useDouble)
            {
                double zeroL[blockSize] = { 0.0 };
                double zeroR[blockSize] = { 0.0 };
                double* inputs[2] = { zeroL, zeroR };
                double* outputs[2] = { outL.data () + b * blockSize, outR.data () + b * blockSize };
                effect->processDoubleReplacing (effect, inputs, outputs, blockSize);
            }
            else
            {
                float zeroL[blockSize] = { 0.0f };
                float zeroR[blockSize] = { 0.0f };
                float* inputs[2] = { zeroL, zeroR };
                float* outputs[2] = { outLf.data () + b * blockSize, outRf.data () + b * blockSize };
                effect->processReplacing (effect, inputs, outputs, blockSize);
            }
        }
    }

    const double* l = outL.data ();
    const double* r = outR.data ();
    const float* lf = outLf.data ();
    const float* rf = outRf.data ();

    // Before the first echo there must be silence (fully wet).
    const double silent = useDouble ? l[8000] : (double) lf[8000];
    report (std::fabs (silent) < 0.01, "output is silent before the first echo");

    // First echo: L at 1*delay, amplitude 1.0
    const double e1 = useDouble ? l[delaySamples] : (double) lf[delaySamples];
    report (std::fabs (e1 - 1.0) < 0.05, "first echo (L) at 200 ms with amplitude ~1.0");

    // Second echo: R at 2*delay, amplitude 0.9 (feedback)
    const double e2 = useDouble ? r[2 * delaySamples] : (double) rf[2 * delaySamples];
    report (std::fabs (e2 - 0.9) < 0.05, "second echo (R) at 400 ms with amplitude ~0.9");

    // Third echo: L at 3*delay, amplitude 0.81
    const double e3 = useDouble ? l[3 * delaySamples] : (double) lf[3 * delaySamples];
    report (std::fabs (e3 - 0.81) < 0.05, "third echo (L) at 600 ms with amplitude ~0.81");

    // Dry/wet: with mix = 0 the output equals the input.
    effect->setParameter (effect, 2, 0.0f);
    float dryInL[blockSize] = { 0.5f };
    float dryInR[blockSize] = { 0.25f };
    float dryOutL[blockSize] = { 0.0f };
    float dryOutR[blockSize] = { 0.0f };
    float* inputs[2] = { dryInL, dryInR };
    float* outputs[2] = { dryOutL, dryOutR };
    effect->processReplacing (effect, inputs, outputs, blockSize);
    report (std::fabs (dryOutL[0] - 0.5f) < 0.001f && std::fabs (dryOutR[0] - 0.25f) < 0.001f,
            "mix 0% passes the input through");
    effect->setParameter (effect, 2, 1.0f);
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testChunk (AEffect* effect)
{
    std::printf ("Chunk persistence:\n");

    void* chunk = nullptr;
    const VstInt32 chunkSize = (VstInt32) dispatch (effect, effGetChunk, 0, 0, &chunk);
    report (chunk != nullptr && chunkSize > 0, "effGetChunk returns a non-empty chunk");

    // Change a parameter, then restore from the chunk.
    effect->setParameter (effect, 1, 0.5f);
    report (fabs (effect->getParameter (effect, 1) - 0.5f) < 1e-6f, "parameter changed before restore");

    const VstInt32 ret = (VstInt32) dispatch (effect, effSetChunk, 0, (VstIntPtr) chunkSize, chunk, 0.0f);
    report (ret == 1, "effSetChunk restores successfully");
    report (fabs (effect->getParameter (effect, 1) - 0.9f) < 1e-6f, "parameter restored to chunked value (0.9)");

    std::free (chunk);
}

//----------------------------------------------------------------------------------------------------------------------------------------
static void testProgramsAndId (AEffect* effect)
{
    std::printf ("Programs / identification:\n");

    char text[64];
    dispatch (effect, effGetEffectName, 0, 0, text);
    report (std::strcmp (text, "FirstVST Delay") == 0, "effect name is 'FirstVST Delay'");

    dispatch (effect, effGetVendorString, 0, 0, text);
    report (std::strcmp (text, "First VST") == 0, "vendor string is 'First VST'");

    report ((VstInt32) dispatch (effect, effGetPlugCategory) == kPlugCategRoomFx, "plug category is 'room fx'");

    dispatch (effect, effGetProgramName, 0, 0, text);
    report (std::strcmp (text, "FirstVST Delay") == 0, "default program name is 'FirstVST Delay'");

    std::snprintf (text, 64, "My Preset");
    dispatch (effect, effSetProgramName, 0, 0, text);
    dispatch (effect, effGetProgramName, 0, 0, text);
    report (std::strcmp (text, "My Preset") == 0, "program name round-trips");

    report ((VstInt32) dispatch (effect, effGetProgram) == 0, "current program is 0");
}

//----------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char** argv)
{
    const char* pluginPath = (argc > 1) ? argv[1] : "FirstVSTDelay.so";

    std::printf ("FirstVST Delay — headless VST 2.4 host test\n");
    std::printf ("Plugin: %s\n\n", pluginPath);

    lib_handle lib = (lib_handle) HOST_OPEN (pluginPath);
    if (!lib)
    {
        std::printf ("FAIL: cannot open plugin library '%s'\n", pluginPath);
        return 1;
    }

    VstPluginMainFn mainFn = (VstPluginMainFn) HOST_SYM (lib, "VSTPluginMain");
    if (!mainFn)
    {
        std::printf ("FAIL: library does not export VSTPluginMain\n");
        HOST_CLOSE (lib);
        return 1;
    }

    AEffect* effect = mainFn (hostCallback);
    if (!effect || effect->magic != kEffectMagic)
    {
        std::printf ("FAIL: VSTPluginMain returned an invalid AEffect\n");
        HOST_CLOSE (lib);
        return 1;
    }

    std::printf ("Loading:\n");
    report (true, "library exports VSTPluginMain and returns a valid AEffect");
    report (effect->numInputs == 2 && effect->numOutputs == 2, "plugin declares 2 inputs and 2 outputs");

    const VstInt32 flags = (VstInt32) effect->flags;
    report ((flags & effFlagsCanReplacing) != 0, "supports processReplacing (32-bit)");
    report ((flags & effFlagsCanDoubleReplacing) != 0, "supports processDoubleReplacing (64-bit)");
    report ((flags & effFlagsProgramChunks) != 0, "programs are stored as chunks");
    report ((flags & effFlagsHasEditor) == 0, "has no custom editor (GUI)");

    dispatch (effect, effOpen);
    dispatch (effect, effSetSampleRate, 0, 0, nullptr, 44100.0f);
    dispatch (effect, effSetBlockSize, 0, 128);
    dispatch (effect, effMainsChanged, 0, 1);

    testParameters (effect);

    // Latency: 200 ms at 44100 Hz == 8820 samples
    effect->setParameter (effect, 0, timeParamFor (200.0));
    report ((VstInt32) dispatch (effect, effGetTailSize) == 8820,
            "effGetTailSize reports the delay latency (8820 samples @ 200 ms)");

    testDelayDsp (effect, false);
    testDelayDsp (effect, true);
    testChunk (effect);
    testProgramsAndId (effect);

    dispatch (effect, effMainsChanged, 0, 0);   // off

    // The SDK's dispatchEffectClass deletes the plug-in instance when
    // effClose is dispatched — the host must not delete it again.
    dispatch (effect, effClose);

    HOST_CLOSE (lib);

    std::printf ("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED",
                 g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}

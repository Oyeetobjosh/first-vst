//----------------------------------------------------------------------------------------------------------------------------------------
// Tobjosh Reverb — headless VST 2.4 test host
//
// Loads the built plug-in like a real DAW would (dlopen/LoadLibrary +
// VSTPluginMain) and exercises it:
//   - open / setup / flags / identification
//   - parameters (names, labels, displays)
//   - programs (Rooms / Hall / Cathedral / Space)
//   - DSP: reverb decay time (T60 ~= Size)
//   - DSP: 360° azimuth panning (front / right / left / back folding)
//   - DSP: bypass passthrough
//   - chunk persistence (state save/restore)
//   - float + double processing paths
//
// Usage:  tobjosh-host-test [path-to-plugin]
// Exit code 0 = all checks passed, 1 = failure.
//----------------------------------------------------------------------------------------------------------------------------------------

#include <algorithm>
#include <array>
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
// Parameters and identification
//----------------------------------------------------------------------------------------------------------------------------------------
static void testParameters (AEffect* effect)
{
    std::printf ("Parameters & identification:\n");

    report (effect->numParams == 4, "AEffect::numParams reports 4 parameters");
    report (effect->numPrograms == 4, "AEffect::numPrograms reports 4 presets");
    report ((effect->flags & effFlagsHasEditor) != 0, "effFlagsHasEditor is set");

    char text[64];
    dispatch (effect, effGetParamName, 0, 0, text);
    report (std::strcmp (text, "Size") == 0, "param 0 name is 'Size'");
    dispatch (effect, effGetParamName, 1, 0, text);
    report (std::strcmp (text, "Damping") == 0, "param 1 name is 'Damping'");
    dispatch (effect, effGetParamName, 2, 0, text);
    report (std::strcmp (text, "360°") == 0, "param 2 name is '360°'");
    dispatch (effect, effGetParamName, 3, 0, text);
    report (std::strcmp (text, "Mix") == 0, "param 3 name is 'Mix'");

    dispatch (effect, effGetParamLabel, 0, 0, text);
    report (std::strcmp (text, "s") == 0, "param 0 label is 's'");
    dispatch (effect, effGetParamLabel, 2, 0, text);
    report (std::strcmp (text, "°") == 0, "param 2 label is '°'");

    effect->setParameter (effect, 0, 2.5f);
    dispatch (effect, effGetParamDisplay, 0, 0, text);
    report (std::strcmp (text, "2.50 s") == 0, "param 0 displays '2.50 s' for 2.5");

    effect->setParameter (effect, 2, 120.0f);
    dispatch (effect, effGetParamDisplay, 2, 0, text);
    report (std::strcmp (text, "120 °") == 0, "param 2 displays '120 °' for 120");

    effect->setParameter (effect, 3, 0.5f);
    dispatch (effect, effGetParamDisplay, 3, 0, text);
    report (std::strcmp (text, "50 %") == 0, "param 3 displays '50 %' for 0.5");

    report (std::fabs (effect->getParameter (effect, 0) - 2.5f) < 1e-6f,
            "param 0 round-trips through getParameter");

    dispatch (effect, effGetEffectName, 0, 0, text);
    report (std::strcmp (text, "Tobjosh Reverb") == 0, "plug-in name is 'Tobjosh Reverb'");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Programs
//----------------------------------------------------------------------------------------------------------------------------------------
static void testPrograms (AEffect* effect)
{
    std::printf ("Programs:\n");

    char name[64];
    dispatch (effect, effGetProgramName, 0, 0, name);
    report (std::strcmp (name, "Rooms") == 0, "program 0 is 'Rooms'");

    dispatch (effect, effSetProgram, 0, 1, nullptr, 0.0f);
    dispatch (effect, effGetProgramName, 0, 0, name);
    report (std::strcmp (name, "Hall") == 0, "program 1 is 'Hall'");

    // Hall preset values must be applied
    report (std::fabs (effect->getParameter (effect, 0) - 1.5f) < 1e-6f, "Hall sets Size to 1.5");
    report (std::fabs (effect->getParameter (effect, 1) - 0.30f) < 1e-6f, "Hall sets Damping to 0.30");
    report (std::fabs (effect->getParameter (effect, 2) - 120.0f) < 1e-6f, "Hall sets 360° to 120");
    report (std::fabs (effect->getParameter (effect, 3) - 0.40f) < 1e-6f, "Hall sets Mix to 0.40");

    dispatch (effect, effSetProgram, 0, 2, nullptr, 0.0f);
    dispatch (effect, effGetProgramName, 0, 0, name);
    report (std::strcmp (name, "Cathedral") == 0, "program 2 is 'Cathedral'");
    report (std::fabs (effect->getParameter (effect, 0) - 4.0f) < 1e-6f, "Cathedral sets Size to 4.0");

    dispatch (effect, effSetProgram, 0, 3, nullptr, 0.0f);
    dispatch (effect, effGetProgramName, 0, 0, name);
    report (std::strcmp (name, "Space") == 0, "program 3 is 'Space'");
    report (std::fabs (effect->getParameter (effect, 0) - 6.0f) < 1e-6f, "Space sets Size to 6.0");

    dispatch (effect, effSetProgram, 0, 0, nullptr, 0.0f);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Chunk persistence
//----------------------------------------------------------------------------------------------------------------------------------------
static void testChunks (AEffect* effect)
{
    std::printf ("Chunks:\n");

    effect->setParameter (effect, 0, 2.0f);
    effect->setParameter (effect, 1, 0.25f);
    effect->setParameter (effect, 2, 90.0f);
    effect->setParameter (effect, 3, 0.5f);

    void* chunk = nullptr;
    const VstInt32 size = (VstInt32) dispatch (effect, effGetChunk, 0, 0, &chunk);
    report (size > 0 && chunk != nullptr, "effGetChunk returns a non-empty chunk");

    // set some other state, then restore
    effect->setParameter (effect, 0, 0.5f);
    effect->setParameter (effect, 2, 0.0f);

    const VstInt32 restored = (VstInt32) dispatch (effect, effSetChunk, 0, (VstIntPtr) size, chunk, 0.0f);
    report (restored == size, "effSetChunk restores successfully");
    report (std::fabs (effect->getParameter (effect, 0) - 2.0f) < 1e-6f, "setChunk restores Size");
    report (std::fabs (effect->getParameter (effect, 2) - 90.0f) < 1e-6f, "setChunk restores 360°");

    free (chunk);
}

//----------------------------------------------------------------------------------------------------------------------------------------
// DSP helpers
//----------------------------------------------------------------------------------------------------------------------------------------
template <typename SampleType>
static void runImpulse (AEffect* effect, std::vector<SampleType>& outL, std::vector<SampleType>& outR,
                        VstInt32 sampleRate, VstInt32 totalSamples)
{
    constexpr VstInt32 blockSize = 128;
    const VstInt32 numBlocks = totalSamples / blockSize;

    for (VstInt32 b = 0; b < numBlocks; ++b)
    {
        std::array<SampleType, blockSize> inL{}, inR{};
        if (b == 0)
            inL[0] = (SampleType) 1.0;

        SampleType* inputs[2] = { inL.data (), inR.data () };
        SampleType* outputs[2] = { outL.data () + b * blockSize, outR.data () + b * blockSize };

        if (std::is_same<SampleType, double>::value)
            effect->processDoubleReplacing (effect, (double**) inputs, (double**) outputs, blockSize);
        else
            effect->processReplacing (effect, (float**) inputs, (float**) outputs, blockSize);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Reverb decay: with Size = 2.0 s the tail must fall below -40 dB by t ~ 1.8 s
//----------------------------------------------------------------------------------------------------------------------------------------
template <typename SampleType>
static void testReverbDecay (AEffect* effect)
{
    const VstInt32 sampleRate = 48000;
    const VstInt32 totalSamples = (VstInt32) (2.5 * sampleRate);

    effect->setParameter (effect, 0, 2.0f);    // Size -> T60 ~ 2 s
    effect->setParameter (effect, 1, 0.0f);    // Damping off
    effect->setParameter (effect, 2, 0.0f);    // centered
    effect->setParameter (effect, 3, 1.0f);    // fully wet

    std::vector<SampleType> outL (totalSamples, 0.0), outR (totalSamples, 0.0);
    runImpulse (effect, outL, outR, sampleRate, totalSamples);

    const char* tag = std::is_same<SampleType, double>::value ? "64-bit" : "32-bit";
    std::printf ("Reverb decay (%s):\n", tag);

    // peak amplitude in the first 0.6 s (early reflections + first taps)
    double peak = 0.0;
    for (VstInt32 i = 0; i < (VstInt32) (0.6 * sampleRate); ++i)
        peak = std::max (peak, (double) std::fabs (outL[i]));
    report (peak > 0.05, "impulse produces a reverb tail");

    // RMS in windows: [0.6-0.8 s], [1.2-1.4 s], [1.8-2.0 s]
    auto windowRms = [&] (double t0, double t1)
    {
        double sum = 0.0;
        VstInt32 count = 0;
        for (VstInt32 i = (VstInt32) (t0 * sampleRate); i < (VstInt32) (t1 * sampleRate); ++i)
        {
            sum += (double) outL[i] * (double) outL[i];
            ++count;
        }
        return count > 0 ? std::sqrt (sum / count) : 0.0;
    };

    const double rmsEarly = windowRms (0.6, 0.8);
    const double rmsMid   = windowRms (1.2, 1.4);
    const double rmsLate  = windowRms (1.8, 2.0);

    report (rmsEarly > 1e-4, "early tail window has signal");
    report (rmsEarly > rmsMid, "tail decays from early to mid window");
    report (rmsMid > rmsLate, "tail decays from mid to late window");
    report (rmsLate < peak * 0.01, "tail falls below -40 dB by t ~ 1.8 s (T60 ~ Size)");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// 360° panning: the wet signal must swing around the stereo field
//----------------------------------------------------------------------------------------------------------------------------------------
static void test360Panning (AEffect* effect)
{
    const VstInt32 sampleRate = 48000;
    const VstInt32 totalSamples = (VstInt32) (0.6 * sampleRate);

    effect->setParameter (effect, 0, 0.5f);    // short tail
    effect->setParameter (effect, 1, 0.0f);
    effect->setParameter (effect, 3, 1.0f);    // fully wet

    // RMS of L/R in the window [100 ms, 400 ms] — after the early reflections
    auto wetBalance = [&] (float angle)
    {
        effect->setParameter (effect, 2, angle);

        std::vector<float> outL (totalSamples, 0.f), outR (totalSamples, 0.f);
        runImpulse (effect, outL, outR, sampleRate, totalSamples);

        double sumL = 0.0, sumR = 0.0;
        VstInt32 count = 0;
        for (VstInt32 i = (VstInt32) (0.1 * sampleRate); i < (VstInt32) (0.4 * sampleRate); ++i)
        {
            sumL += (double) outL[i] * (double) outL[i];
            sumR += (double) outR[i] * (double) outR[i];
            ++count;
        }
        return std::make_pair (std::sqrt (sumL / count), std::sqrt (sumR / count));
    };

    std::printf ("360° panning:\n");

    const auto bal0   = wetBalance (0.0f);       // front center
    const auto bal90  = wetBalance (90.0f);      // right
    const auto bal270 = wetBalance (270.0f);     // left
    const auto bal180 = wetBalance (180.0f);     // behind

    const double rmL0 = bal0.first,  rmR0 = bal0.second;
    const double rmL90 = bal90.first, rmR90 = bal90.second;
    const double rmL270 = bal270.first, rmR270 = bal270.second;
    const double rmL180 = bal180.first, rmR180 = bal180.second;

    report (std::fabs (rmL0 - rmR0) < 0.15 * std::max (rmL0, rmR0), "angle 0 pans the wet centered");
    report (rmR90 > 3.0 * rmL90, "angle 90 pans the wet to the right");
    report (rmL270 > 3.0 * rmR270, "angle 270 pans the wet to the left");
    report (std::max (rmL180, rmR180) < 0.35 * std::max (rmL0, rmR0), "angle 180 folds the wet back with attenuation");
}

//----------------------------------------------------------------------------------------------------------------------------------------
// Bypass
//----------------------------------------------------------------------------------------------------------------------------------------
static void testBypass (AEffect* effect)
{
    const VstInt32 sampleRate = 48000;
    constexpr VstInt32 blockSize = 128;

    std::printf ("Bypass:\n");

    // bypass via effSetBypass
    effect->setParameter (effect, 0, 4.0f);
    effect->setParameter (effect, 3, 1.0f);
    dispatch (effect, effSetBypass, 0, 1, nullptr, 0.0f);

    bool ok = true;
    for (VstInt32 b = 0; b < 8; ++b)
    {
        std::array<float, blockSize> inL{}, inR{}, outL{}, outR{};
        for (VstInt32 i = 0; i < blockSize; ++i)
        {
            inL[i] = (float) std::sin (2.0 * M_PI * 220.0 * (b * blockSize + i) / sampleRate);
            inR[i] = (float) std::sin (2.0 * M_PI * 330.0 * (b * blockSize + i) / sampleRate);
        }
        float* inputs[2] = { inL.data (), inR.data () };
        float* outputs[2] = { outL.data (), outR.data () };
        effect->processReplacing (effect, inputs, outputs, blockSize);

        for (VstInt32 i = 0; i < blockSize; ++i)
            if (std::fabs (outL[i] - inL[i]) > 1e-6f || std::fabs (outR[i] - inR[i]) > 1e-6f)
                ok = false;
    }
    report (ok, "bypass passes audio through unchanged");

    dispatch (effect, effSetBypass, 0, 0, nullptr, 0.0f);
}

//----------------------------------------------------------------------------------------------------------------------------------------
int main (int argc, char** argv)
{
    const char* pluginPath = (argc > 1) ? argv[1] : "TobjoshReverb.so";

    std::printf ("Tobjosh Reverb host test — loading %s\n", pluginPath);

    lib_handle lib = HOST_OPEN (pluginPath);
    if (!lib)
    {
        std::printf ("FAIL: could not load the plug-in (%s)\n", pluginPath);
        return 1;
    }

    VstPluginMainFn mainFn = (VstPluginMainFn) HOST_SYM (lib, "VSTPluginMain");
    if (!mainFn)
    {
        std::printf ("FAIL: VSTPluginMain not found in the plug-in\n");
        HOST_CLOSE (lib);
        return 1;
    }

    AEffect* effect = mainFn (hostCallback);
    if (!effect)
    {
        std::printf ("FAIL: VSTPluginMain returned no effect\n");
        HOST_CLOSE (lib);
        return 1;
    }

    std::printf ("Plug-in loaded (uniqueID 0x%08x, %d params, %d programs)\n",
                 effect->uniqueID, effect->numParams, effect->numPrograms);

    // open and configure like a real host
    dispatch (effect, effOpen);
    dispatch (effect, effSetSampleRate, 0, 0, nullptr, 48000.0f);
    dispatch (effect, effSetBlockSize, 0, 128);

    testParameters (effect);
    testPrograms (effect);
    testChunks (effect);
    testReverbDecay<float> (effect);
    testReverbDecay<double> (effect);
    test360Panning (effect);
    testBypass (effect);

    // let the plug-in go away cleanly (like a real host on effClose)
    dispatch (effect, effClose);

    HOST_CLOSE (lib);

    if (g_failures == 0)
    {
        std::printf ("\nAll Tobjosh Reverb host checks passed.\n");
        return 0;
    }

    std::printf ("\n%d Tobjosh Reverb host check(s) FAILED.\n", g_failures);
    return 1;
}

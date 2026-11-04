#pragma once

#include <vector>

/*!
    Tobjosh Reverb — audio engine.

    A feedback delay network (FDN) reverb with a 360° room position:

    - Early reflections: 6 fixed taps with room-spread pan angles, giving
      the recognisable "room" character.
    - Diffuse tail: 4-tap feedback delay network with a reciprocal feedback
      matrix and per-tap damping low-passes. The feedback gain is derived
      from the room size so the decay time follows the SIZE knob.
    - 360°: every wet signal (early reflections + diffuse tail) is panned
      with an azimuth that can be rotated 360° around the listener. The
      back hemisphere folds into the front with attenuation so the knob
      really feels like rotating the room.

    The engine is pure DSP — no VST dependencies — so it can be unit-tested
    directly.
*/
class ReverbEngine
{
public:
    ReverbEngine ();

    void setSampleRate (float sampleRate);
    void setSize (float size);       // decay time in seconds (0.2 .. 6.0)
    void setDamping (float damping); // 0 = bright, 1 = dark
    void setAngle (float angle);     // 0 .. 360 degrees
    void setMix (float mix);         // 0 = dry, 1 = wet
    void setBypass (bool on);
    void reset ();

    void process (float* inL, float* inR, float* outL, float* outR, int sampleFrames);

    // Helpers used by tests / UI
    static void panWet (double wet, float angleDeg, double* outL, double* outR);

public:
    static const int kNumTaps = 4;
    static const int kNumReflections = 6;

private:
    void updateFeedbackMatrix ();

    // FDN delay lines (all double for consistency)
    std::vector<double> m_delay[kNumTaps];
    int m_delayLen[kNumTaps];
    int m_pos[kNumTaps];
    double m_fb[kNumTaps][kNumTaps];
    double m_lp[kNumTaps];
    double m_lpCoef[kNumTaps];

    // Early reflection taps (fixed delays/gains/angles, in milliseconds)
    double m_erDelay[kNumReflections];   // delay in samples
    double m_erGain[kNumReflections];
    double m_erAngle[kNumReflections];   // fixed room spread, degrees
    int m_erPos[kNumReflections];
    std::vector<double> m_er[kNumReflections];

    // Parameters
    double m_size;      // seconds
    double m_damping;
    double m_angle;     // degrees
    double m_mix;
    bool m_bypass;

    float m_sampleRate;
    bool m_initialized;
};

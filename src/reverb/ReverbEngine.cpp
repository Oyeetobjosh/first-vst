#include "ReverbEngine.h"

#include <cmath>

//-----------------------------------------------------------------------------------------------------------------------------------------
// FDN tap lengths (seconds) — decorrelated, ~0.25-0.33 s
//-----------------------------------------------------------------------------------------------------------------------------------------
static const double kTapLenSec[ReverbEngine::kNumTaps] = { 0.25, 0.27, 0.30, 0.33 };

//-----------------------------------------------------------------------------------------------------------------------------------------
// Early reflection taps: delay (ms), gain, room spread angle (degrees)
//-----------------------------------------------------------------------------------------------------------------------------------------
static const double kErDelayMs[ReverbEngine::kNumReflections]  = { 4, 9, 14, 20, 27, 34 };
static const double kErGain[ReverbEngine::kNumReflections]     = { 0.70, 0.60, 0.52, 0.45, 0.38, 0.32 };
static const double kErAngleDeg[ReverbEngine::kNumReflections]  = { 30, 90, 150, 210, 270, 330 };

//-----------------------------------------------------------------------------------------------------------------------------------------
ReverbEngine::ReverbEngine ()
: m_size (1.5)
, m_damping (0.35)
, m_angle (0.0)
, m_mix (0.35)
, m_bypass (false)
, m_sampleRate (44100.f)
, m_initialized (false)
{
    for (int i = 0; i < kNumReflections; ++i)
    {
        m_erDelay[i] = kErDelayMs[i] / 1000.0 * (double) m_sampleRate;
        m_erGain[i] = kErGain[i];
        m_erAngle[i] = kErAngleDeg[i];
        m_erPos[i] = 0;
    }
    updateFeedbackMatrix ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::setSampleRate (float sampleRate)
{
    if (sampleRate <= 0.f)
        return;

    m_sampleRate = sampleRate;

    for (int i = 0; i < kNumTaps; ++i)
        m_delayLen[i] = (int) std::ceil (kTapLenSec[i] * sampleRate);

    for (int i = 0; i < kNumReflections; ++i)
        m_erDelay[i] = kErDelayMs[i] / 1000.0 * sampleRate;

    reset ();
    m_initialized = true;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::setSize (float size)
{
    m_size = std::max (0.2, std::min (6.0, (double) size));
    updateFeedbackMatrix ();
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::setDamping (float damping)
{
    m_damping = std::max (0.0, std::min (1.0, (double) damping));
    for (int i = 0; i < kNumTaps; ++i)
        m_lpCoef[i] = 0.9 * m_damping;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::setAngle (float angle)
{
    // keep it in [0, 360)
    m_angle = std::fmod ((double) angle, 360.0);
    if (m_angle < 0.0)
        m_angle += 360.0;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::setMix (float mix)
{
    m_mix = std::max (0.0, std::min (1.0, (double) mix));
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::setBypass (bool on)
{
    m_bypass = on;
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::reset ()
{
    for (int i = 0; i < kNumTaps; ++i)
    {
        if ((int) m_delay[i].size () < m_delayLen[i])
            m_delay[i].assign (m_delayLen[i], 0.0);

        std::fill (m_delay[i].begin (), m_delay[i].end (), 0.0);
        m_pos[i] = 0;
        m_lp[i] = 0.0;
    }

    for (int i = 0; i < kNumReflections; ++i)
    {
        const int len = (int) std::ceil (m_erDelay[i]);
        if ((int) m_er[i].size () < len)
            m_er[i].assign (len, 0.0);

        std::fill (m_er[i].begin (), m_er[i].end (), 0.0);
        m_erPos[i] = 0;
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::updateFeedbackMatrix ()
{
    // Per-tap feedback gain: the tap amplitude decays as g^(t/tapLen), so
    // T60 = tapLen * ln(1000) / ln(1/g). Choosing g = exp(-6.9 * tapLen / size)
    // makes the overall tail decay time T60 ~= size (seconds).
    for (int i = 0; i < kNumTaps; ++i)
    {
        const double g = std::exp (-6.9 * kTapLenSec[i] / m_size);
        for (int j = 0; j < kNumTaps; ++j)
            m_fb[i][j] = (i == j) ? -g : (1.0 - g) / (kNumTaps - 1);
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------
void ReverbEngine::panWet (double wet, float angleDeg, double* outL, double* outR)
{
    const double a = (double) angleDeg * M_PI / 180.0;
    double phi = std::fmod (a + M_PI, 2.0 * M_PI) - M_PI;      // [-pi, pi)

    double gain = 1.0;
    if (phi < -M_PI / 2.0 || phi > M_PI / 2.0)                 // behind the listener
    {
        gain = std::fabs (std::sin (phi));                     // |sin| of the back angle: 1 at the edges, 0 directly behind
        phi = (phi > M_PI / 2.0) ? (M_PI - phi) : (-M_PI - phi);   // fold into the front arc
    }

    *outL = wet * gain * std::cos (phi / 2.0 + M_PI / 4.0);
    *outR = wet * gain * std::sin (phi / 2.0 + M_PI / 4.0);
}

//--------------------------------------------------------------------------------------------------
void ReverbEngine::process (float* inL_, float* inR_, float* outL_, float* outR_, int sampleFrames)
{
    if (m_bypass || sampleFrames <= 0 || !m_initialized)
    {
        for (int n = 0; n < sampleFrames; ++n)
        {
            outL_[n] = inL_[n];
            outR_[n] = inR_[n];
        }
        return;
    }

    const double mix = m_mix;
    const double inMonoScale = 1.0 / kNumTaps;

    for (int n = 0; n < sampleFrames; ++n)
    {
        const double inL = inL_[n];
        const double inR = inR_[n];

        // --- early reflections --------------------------------------------------------------
        double erL = 0.0, erR = 0.0;
        const double erInput = (inL + inR) * 0.5;

        for (int i = 0; i < kNumReflections; ++i)
        {
            const int len = (int) std::ceil (m_erDelay[i]);
            if (len > 1)
            {
                const double s = m_er[i][m_erPos[i]];
                m_er[i][m_erPos[i]] = erInput * m_erGain[i];
                m_erPos[i] = (m_erPos[i] + 1) % len;

                double l = 0.0, r = 0.0;
                panWet (s * 0.5, (float) (m_erAngle[i] + m_angle), &l, &r);
                erL += l;
                erR += r;
            }
        }

        // --- feedback delay network -----------------------------------------------------------
        double tapRead[kNumTaps];
        for (int i = 0; i < kNumTaps; ++i)
            tapRead[i] = m_delay[i][m_pos[i]];

        const double fdnInput = erInput * inMonoScale;
        for (int i = 0; i < kNumTaps; ++i)
        {
            double fb = 0.0;
            for (int j = 0; j < kNumTaps; ++j)
                fb += m_fb[i][j] * tapRead[j];

            m_lp[i] = m_lpCoef[i] * m_lp[i] + (1.0 - m_lpCoef[i]) * fb;
            m_delay[i][m_pos[i]] = fdnInput + m_lp[i];
            m_pos[i] = (m_pos[i] + 1) % m_delayLen[i];
        }

        double wet = 0.0;
        for (int i = 0; i < kNumTaps; ++i)
            wet += tapRead[i];

        double wetL = 0.0, wetR = 0.0;
        panWet (wet, (float) m_angle, &wetL, &wetR);

        outL_[n] = (float) (inL * (1.0 - mix) + (erL + wetL) * mix);
        outR_[n] = (float) (inR * (1.0 - mix) + (erR + wetR) * mix);
    }
}

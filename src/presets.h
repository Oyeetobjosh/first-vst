#pragma once

//-----------------------------------------------------------------------------------------------------------------------------------------
// Tobjosh Reverb presets
//-----------------------------------------------------------------------------------------------------------------------------------------
struct ReverbPreset
{
    const char* name;
    float size;      // decay time in seconds
    float damping;   // 0..1 (brightness of the tail)
    float angle;     // 0..360 (azimuth of the reverb in the room)
    float mix;       // 0..1 (wet amount)
};

static const ReverbPreset reverbPresets[] =
{
    { "Rooms",     0.50f, 0.45f,   0.0f, 0.35f },
    { "Hall",      1.50f, 0.30f, 120.0f, 0.40f },
    { "Cathedral", 4.00f, 0.20f, 240.0f, 0.45f },
    { "Space",     6.00f, 0.08f, 300.0f, 0.50f },
};

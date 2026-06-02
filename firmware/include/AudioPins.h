#pragma once

// MAX98357A I2S wiring.
#define AUDIO_PIN_DIN 25
#define AUDIO_PIN_LRC 26
#define AUDIO_PIN_BCLK 27

// Keep conservative at first; MAX98357A can get loud quickly.
#define AUDIO_DEFAULT_GAIN 0.3f

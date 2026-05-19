#include "water_tank.h"

// Speed of sound in air is approximately 343 m/s, which is 0.034 cm/us.
static constexpr float SPEED_OF_SOUND_CM_PER_US = 0.034f;

float calculateDistanceCm(long echoDurationUs) {
    return (echoDurationUs * SPEED_OF_SOUND_CM_PER_US) / 2.0f;
}

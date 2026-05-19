#ifndef WATER_TANK_H
#define WATER_TANK_H

// Converts the echo pulse duration from an HC-SR04 sensor (microseconds)
// to a distance in centimeters.
float calculateDistanceCm(long echoDurationUs);

#endif // WATER_TANK_H

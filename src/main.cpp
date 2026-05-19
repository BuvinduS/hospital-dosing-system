// This code is for testing the HC-SR04 ultrasonic sensor with an ESP32-S3 microcontroller. 
// It uses the Arduino framework and is written in C++11. 
// The TRIG pin of the sensor is connected to GPIO5, and the ECHO pin is connected to GPIO18. 
// The code sends a pulse to trigger the sensor, measures the duration of the echo pulse, 
// and calculates the distance in centimeters, which is then printed to the serial monitor every second.

#include <Arduino.h>
// ESP32-S3 + HC-SR04 Test Code
// TRIG -> GPIO5
// ECHO -> GPIO18

#define TRIG_PIN 5
#define ECHO_PIN 17

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.println("HC-SR04 Ultrasonic Sensor Test");
}

void loop() {
  long duration;
  float distanceCm;

  // Clear trigger
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  // Send 10us pulse
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10); // Keeps the pin HIGH for 10 microseconds. 
  // HC-SR04 requires a pulse of at least 10 microseconds to trigger the measurement.
  digitalWrite(TRIG_PIN, LOW);

  // Read echo pulse (measures how long the ECHO pin stays HIGH, which corresponds to the time it takes for the ultrasonic pulse to travel to the object and back)
  duration = pulseIn(ECHO_PIN, HIGH);

  // Calculate distance
  // The formula is: distance = (duration * speed of sound) / 2
  // Speed of sound is approximately 34300 cm/s, which is 0.034
  distanceCm = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  delay(1000);
}
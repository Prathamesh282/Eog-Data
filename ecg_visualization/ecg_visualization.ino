#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define SAMPLE_RATE 125 // Hz
#define BAUD_RATE 115200
#define INPUT_CHANNEL 0 // ADS1115 channel (0–3)

// Create an instance of the ADS1115
Adafruit_ADS1115 ads;

void setup() {
  Serial.begin(BAUD_RATE); // Start serial communication

  // Initialize ADS1115
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115!");
    while (1); // Halt execution if initialization fails
  }

  // Set gain to +/-4.096V for full-scale range
  ads.setGain(GAIN_ONE); // Gain of 1 for input range ±4.096V
  Serial.println("ADS1115 initialized.");
}

void loop() {
  static unsigned long lastSampleTime = 0;
  unsigned long currentTime = millis();

  // Sample at the specified rate
  if (currentTime - lastSampleTime >= 1000 / SAMPLE_RATE) {
    lastSampleTime = currentTime;

    // Read raw analog value from ADS1115
    int16_t rawValue = ads.readADC_SingleEnded(INPUT_CHANNEL);

    // Convert raw ADC value to voltage
    float voltage = rawValue * 0.125 / 1000.0; // 0.125 mV per count at GAIN_ONE

    // Send voltage data to Serial (Processing will read this)
    Serial.println(voltage, 6); // Print with high precision
  }
}

#include <Adafruit_NeoPixel.h>

#define RGB_PIN 48      // Most ESP32-S3 boards use GPIO48
#define NUMPIXELS 1

Adafruit_NeoPixel pixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  pixel.begin();
  pixel.setBrightness(50); // 0-255
}

void loop() {
  pixel.setPixelColor(0, pixel.Color(255, 0, 0));   // Red
  pixel.show();
  delay(100);

  pixel.setPixelColor(0, pixel.Color(0, 255, 0));   // Green
  pixel.show();
  delay(100);

  pixel.setPixelColor(0, pixel.Color(0, 0, 255));   // Blue
  pixel.show();
  delay(100);

  pixel.setPixelColor(0, pixel.Color(255, 255, 255)); // White
  pixel.show();
  delay(500);

  pixel.clear();
  pixel.show();
  delay(500);
}
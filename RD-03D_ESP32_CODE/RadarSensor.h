#ifndef RADARSENSOR_H
#define RADARSENSOR_H

#include <Arduino.h>

typedef struct RadarTarget {
  float distance;  // mm
  float angle;     // radians
  float speed;     // cm/s
  int16_t x;       // mm
  int16_t y;       // mm
  bool detected;
} RadarTarget;

class RadarSensor {
  public:
    RadarSensor(HardwareSerial& serial);
    ~RadarSensor();

    void begin(unsigned long baud = 256000);
    bool update();
    RadarTarget getTarget();

  private:
    HardwareSerial& radarSerial;
    RadarTarget target;
    bool parseData(const uint8_t *buffer, size_t len);
};

#endif

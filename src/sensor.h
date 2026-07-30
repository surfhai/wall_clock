// sensor.h — temperature/humidity sensor on the Grove I2C connector
//
// Final sensor model not yet decided (SHT4x / SHT31 / AHT20, see CLAUDE.md
// section 2/10). Selection happens via SENSOR_TYPE in config.h; while
// SENSOR_TYPE_NONE is active, this module returns dummy values so the rest
// of the firmware (display layout etc.) can already be tested.
#pragma once

#include <Arduino.h>

struct SensorReading {
    float temperatureC;
    float humidityPercent;
    bool valid;
};

// Initializes the sensor on the (already running) I2C bus.
bool sensor_init();

// Reads a new measurement. reading.valid == false if the read failed
// (e.g. sensor unreachable).
SensorReading sensor_read();

// Applies the hysteresis from config.h: returns true if the value has
// changed enough compared to the last displayed value to justify a redraw
// (see CLAUDE.md section 7).
bool sensor_reading_changed_significantly(const SensorReading &lastDisplayed,
                                           const SensorReading &current);

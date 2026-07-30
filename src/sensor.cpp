#include "sensor.h"
#include "config.h"

#include <Wire.h>
#include <math.h>

#if SENSOR_TYPE == SENSOR_TYPE_SHT4X
#include <Adafruit_SHT4x.h>
static Adafruit_SHT4x s_sht4x;

#elif SENSOR_TYPE == SENSOR_TYPE_SHT31
#include <Adafruit_SHT31.h>
static Adafruit_SHT31 s_sht31;

#elif SENSOR_TYPE == SENSOR_TYPE_AHT20
#include <Adafruit_AHT20.h>
static Adafruit_AHT20 s_aht20;
#endif

bool sensor_init() {
#if SENSOR_TYPE == SENSOR_TYPE_SHT4X
    if (!s_sht4x.begin(&Wire)) {
        Serial.println("[sensor] SHT4x not found!");
        return false;
    }
    s_sht4x.setPrecision(SHT4X_HIGH_PRECISION);
    return true;

#elif SENSOR_TYPE == SENSOR_TYPE_SHT31
    if (!s_sht31.begin(I2C_ADDR_SHT31)) {
        Serial.println("[sensor] SHT31 not found!");
        return false;
    }
    return true;

#elif SENSOR_TYPE == SENSOR_TYPE_AHT20
    if (!s_aht20.begin(&Wire)) {
        Serial.println("[sensor] AHT20 not found!");
        return false;
    }
    return true;

#else
    // SENSOR_TYPE_NONE: placeholder, no sensor attached.
    Serial.println("[sensor] SENSOR_TYPE_NONE - returning dummy values. "
                    "TODO: set the final sensor in config.h.");
    return true;
#endif
}

SensorReading sensor_read() {
    SensorReading r{0.0f, 0.0f, false};

#if SENSOR_TYPE == SENSOR_TYPE_SHT4X
    sensors_event_t humidity, temp;
    if (s_sht4x.getEvent(&humidity, &temp)) {
        r.temperatureC = temp.temperature;
        r.humidityPercent = humidity.relative_humidity;
        r.valid = true;
    }

#elif SENSOR_TYPE == SENSOR_TYPE_SHT31
    float t = s_sht31.readTemperature();
    float h = s_sht31.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        r.temperatureC = t;
        r.humidityPercent = h;
        r.valid = true;
    }

#elif SENSOR_TYPE == SENSOR_TYPE_AHT20
    sensors_event_t humidity, temp;
    if (s_aht20.getEvent(&humidity, &temp)) {
        r.temperatureC = temp.temperature;
        r.humidityPercent = humidity.relative_humidity;
        r.valid = true;
    }

#else
    // Placeholder dummy values while no sensor has been finally chosen.
    r.temperatureC = 24.3f;
    r.humidityPercent = 55.0f;
    r.valid = true;
#endif

    return r;
}

bool sensor_reading_changed_significantly(const SensorReading &lastDisplayed,
                                           const SensorReading &current) {
    if (!current.valid) return false;
    if (!lastDisplayed.valid) return true;

    float tempDelta = fabsf(current.temperatureC - lastDisplayed.temperatureC);
    float humDelta = fabsf(current.humidityPercent - lastDisplayed.humidityPercent);

    return tempDelta >= TEMP_HYSTERESIS_C || humDelta >= HUMIDITY_HYSTERESIS;
}

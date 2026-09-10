#ifndef EMBEDDED_QUALIFIER_LAB_APP_H
#define EMBEDDED_QUALIFIER_LAB_APP_H

#include <stdint.h>

typedef enum
{
    SAMPLE_REJECTED = 0,
    SAMPLE_ACCEPTED = 1,
    SAMPLE_ALARM = 2
} SampleResult;

typedef struct
{
    int32_t temperature_x10;
    uint32_t humidity_x10;
} SensorSample;

typedef struct
{
    int32_t min_valid_temp_x10;
    int32_t max_valid_temp_x10;
    int32_t alarm_temp_x10;
    uint32_t max_valid_humidity_x10;
    uint32_t alarm_humidity_x10;
} AlarmConfig;

extern const AlarmConfig g_alarm_config;

void app_init(void);
SampleResult app_process_sample(const SensorSample *sample);
uint32_t app_sample_count(void);
uint32_t app_alarm_count(void);
uint32_t app_invalid_count(void);

#endif


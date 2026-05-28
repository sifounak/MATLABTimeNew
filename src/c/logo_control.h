#pragma once

#include <pebble.h>
#include <stdint.h>

typedef enum {
  LogoRotationTriggerOff = 0,
  LogoRotationTriggerDoubleTap = 1,
  LogoRotationTriggerShake = 2,
  LogoRotationTriggerMinute = 3,
  LogoRotationTriggerHour = 4,
} LogoRotationTrigger;

void logo_control_init(void);
void logo_control_deinit(void);
void logo_control_set_trigger(int32_t trigger);
void logo_control_handle_minute_tick(struct tm *tick_time);

void matlab_time_animate_logo(void);

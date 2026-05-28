#include "logo_control.h"

#if defined(__has_include)
#if __has_include("message_keys.auto.h")
#include "message_keys.auto.h"
#define HAS_MESSAGE_KEYS_AUTO 1
#endif
#endif

#ifndef HAS_MESSAGE_KEYS_AUTO
#define MESSAGE_KEY_LogoRotationTrigger 10010
#endif

#define DOUBLE_TAP_WINDOW_MS 500

static LogoRotationTrigger s_trigger = LogoRotationTriggerOff;
static bool s_skip_first_minute_tick = true;
static bool s_accel_tap_subscribed = false;
static bool s_touch_subscribed = false;
static bool s_waiting_for_second_tap = false;
static AppTimer *s_double_tap_timer = NULL;

static void clear_double_tap_state(void) {
  if (s_double_tap_timer) {
    app_timer_cancel(s_double_tap_timer);
    s_double_tap_timer = NULL;
  }
  s_waiting_for_second_tap = false;
}

static void double_tap_timeout_callback(void *context) {
  s_double_tap_timer = NULL;
  s_waiting_for_second_tap = false;
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_trigger == LogoRotationTriggerShake) {
    matlab_time_animate_logo();
  }
}

#if defined(PBL_TOUCH)
static void touch_handler(const TouchEvent *event, void *context) {
  if (s_trigger != LogoRotationTriggerDoubleTap || event->type != TouchEvent_Liftoff) {
    return;
  }

  if (s_waiting_for_second_tap) {
    clear_double_tap_state();
    matlab_time_animate_logo();
    return;
  }

  s_waiting_for_second_tap = true;
  s_double_tap_timer = app_timer_register(DOUBLE_TAP_WINDOW_MS, double_tap_timeout_callback, NULL);
}
#endif

static void set_accel_tap_subscription(bool enabled) {
  if (enabled && !s_accel_tap_subscribed) {
    accel_tap_service_subscribe(accel_tap_handler);
    s_accel_tap_subscribed = true;
  } else if (!enabled && s_accel_tap_subscribed) {
    accel_tap_service_unsubscribe();
    s_accel_tap_subscribed = false;
  }
}

static void set_touch_subscription(bool enabled) {
#if defined(PBL_TOUCH)
  if (enabled && !s_touch_subscribed) {
    if (touch_service_is_enabled()) {
      touch_service_subscribe(touch_handler, NULL);
      s_touch_subscribed = true;
    } else {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Touch service unavailable; double tap logo trigger disabled");
    }
  } else if (!enabled && s_touch_subscribed) {
    touch_service_unsubscribe();
    s_touch_subscribed = false;
  }
#else
  if (enabled) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Touch support not compiled; double tap logo trigger disabled");
  }
#endif

  if (!enabled) {
    clear_double_tap_state();
  }
}

void logo_control_set_trigger(int32_t trigger) {
  if (trigger < LogoRotationTriggerOff || trigger > LogoRotationTriggerHour) {
    trigger = LogoRotationTriggerOff;
  }

  persist_write_int(MESSAGE_KEY_LogoRotationTrigger, trigger);
  s_trigger = (LogoRotationTrigger)trigger;
  s_skip_first_minute_tick = true;
  set_accel_tap_subscription(s_trigger == LogoRotationTriggerShake);
  set_touch_subscription(s_trigger == LogoRotationTriggerDoubleTap);
}

void logo_control_handle_minute_tick(struct tm *tick_time) {
  if (s_skip_first_minute_tick) {
    s_skip_first_minute_tick = false;
    return;
  }

  if (s_trigger == LogoRotationTriggerMinute) {
    matlab_time_animate_logo();
  } else if (s_trigger == LogoRotationTriggerHour && tick_time->tm_min == 0) {
    matlab_time_animate_logo();
  }
}

static void logo_control_inbox_received(DictionaryIterator *iter, void *context) {
  Tuple *trigger_tuple = dict_find(iter, MESSAGE_KEY_LogoRotationTrigger);
  if (trigger_tuple) {
    logo_control_set_trigger(trigger_tuple->value->int32);
  }
}

void logo_control_init(void) {
  int32_t trigger = LogoRotationTriggerOff;
  if (persist_exists(MESSAGE_KEY_LogoRotationTrigger)) {
    trigger = persist_read_int(MESSAGE_KEY_LogoRotationTrigger);
  }
  logo_control_set_trigger(trigger);
  app_message_register_inbox_received(logo_control_inbox_received);
}

void logo_control_deinit(void) {
  set_accel_tap_subscription(false);
  set_touch_subscription(false);
  clear_double_tap_state();
}

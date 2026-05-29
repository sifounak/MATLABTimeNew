#include <pebble.h>

#define SETTINGS_KEY 1
#define WEATHER_REFRESH_MINUTES 60
#define DOUBLE_TAP_WINDOW_MS 500
#define DEGREE_SYMBOL "\xC2\xB0"

typedef enum {
  ComplicationEmpty = 0,
  ComplicationTemperature = 1,
  ComplicationBattery = 2,
  ComplicationUV = 3,
} ComplicationType;

typedef enum {
  TemperatureCelsius = 0,
  TemperatureFahrenheit = 1,
  TemperatureKelvin = 2,
} TemperatureUnit;

typedef enum {
  DateFormatWords = 0,
  DateFormatMDY = 1,
  DateFormatDMY = 2,
} DateFormat;

typedef enum {
  LogoRotationTriggerOff = 0,
  LogoRotationTriggerDoubleTap = 1,
  LogoRotationTriggerShake = 2,
  LogoRotationTriggerMinute = 3,
  LogoRotationTriggerHour = 4,
} LogoRotationTrigger;

typedef struct {
  GColor background_color;
  GColor text_color;
  int32_t temperature_unit;
  int32_t date_format;
  bool use_24_hour;
  int32_t complication_left;
  int32_t complication_middle;
  int32_t complication_right;
  bool vibe_on_disconnect;
  bool vibe_on_connect;
  int32_t logo_rotation_trigger;
} Settings;

static Settings s_settings;

static Window *s_window;
static Layer *s_window_layer;
static BitmapLayer *s_logo_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_left_layer;
static TextLayer *s_middle_layer;
static TextLayer *s_right_layer;
static Layer *s_bluetooth_layer;

static GBitmapSequence *s_logo_sequence;
static GBitmap *s_logo_bitmap;
static bool s_logo_animating;
static AppTimer *s_refresh_timer;
static AppTimer *s_double_tap_timer;
static bool s_waiting_for_second_tap;
static bool s_skip_first_minute_tick;
static bool s_accel_tap_subscribed;
static bool s_touch_subscribed;
static bool s_connection_initialized;
static bool s_connected;

static GFont s_time_font;
static GFont s_date_font;
static GFont s_small_font;

static int s_battery_percent = 100;
static bool s_battery_charging;
static bool s_has_weather;
static int32_t s_weather_temp_c;
static int32_t s_weather_uv;

static char s_time_buffer[12];
static char s_date_buffer[24];
static char s_left_buffer[20];
static char s_middle_buffer[20];
static char s_right_buffer[20];

static void prv_layout_layers(void);

static bool prv_is_round(void) {
  GRect bounds = layer_get_bounds(s_window_layer);
  return bounds.size.w == bounds.size.h;
}

static void prv_default_settings(void) {
  s_settings.background_color = GColorBlack;
  s_settings.text_color = GColorWhite;
  s_settings.temperature_unit = TemperatureFahrenheit;
  s_settings.date_format = DateFormatWords;
  s_settings.use_24_hour = false;
  s_settings.complication_left = ComplicationUV;
  s_settings.complication_middle = ComplicationBattery;
  s_settings.complication_right = ComplicationTemperature;
  s_settings.vibe_on_disconnect = true;
  s_settings.vibe_on_connect = true;
  s_settings.logo_rotation_trigger = LogoRotationTriggerOff;
}

static int32_t prv_clamp_i32(int32_t value, int32_t min, int32_t max, int32_t fallback) {
  return (value < min || value > max) ? fallback : value;
}

static void prv_normalize_settings(void) {
  s_settings.temperature_unit = prv_clamp_i32(s_settings.temperature_unit, 0, 2, TemperatureFahrenheit);
  s_settings.date_format = prv_clamp_i32(s_settings.date_format, 0, 2, DateFormatWords);
  s_settings.complication_left = prv_clamp_i32(s_settings.complication_left, 0, 3, ComplicationUV);
  s_settings.complication_middle = prv_clamp_i32(s_settings.complication_middle, 0, 3, ComplicationBattery);
  s_settings.complication_right = prv_clamp_i32(s_settings.complication_right, 0, 3, ComplicationTemperature);
  s_settings.logo_rotation_trigger = prv_clamp_i32(s_settings.logo_rotation_trigger, 0, 4, LogoRotationTriggerOff);
}

static void prv_load_settings(void) {
  prv_default_settings();
  if (persist_exists(SETTINGS_KEY) && persist_get_size(SETTINGS_KEY) == (int)sizeof(s_settings)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
    prv_normalize_settings();
  }
}

static void prv_save_settings(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void prv_request_weather(void) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK || !iter) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Unable to begin weather request: %d", result);
    return;
  }
  dict_write_uint8(iter, MESSAGE_KEY_RequestWeather, 1);
  app_message_outbox_send();
}

static void prv_clear_logo_to_background(void) {
  uint8_t *data = gbitmap_get_data(s_logo_bitmap);
  uint16_t stride = gbitmap_get_bytes_per_row(s_logo_bitmap);
  GRect bounds = gbitmap_get_bounds(s_logo_bitmap);
  memset(data, s_settings.background_color.argb, stride * bounds.size.h);
}

static void prv_load_first_logo_frame(void) {
  gbitmap_sequence_restart(s_logo_sequence);
  prv_clear_logo_to_background();
  gbitmap_sequence_update_bitmap_next_frame(s_logo_sequence, s_logo_bitmap, NULL);
  bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_logo_layer));
}

static void prv_logo_timer_callback(void *context) {
  uint32_t next_delay;

  prv_clear_logo_to_background();
  if (gbitmap_sequence_update_bitmap_next_frame(s_logo_sequence, s_logo_bitmap, &next_delay)) {
    bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);
    layer_mark_dirty(bitmap_layer_get_layer(s_logo_layer));
    app_timer_register(next_delay, prv_logo_timer_callback, NULL);
  } else {
    prv_load_first_logo_frame();
    s_logo_animating = false;
  }
}

static void prv_animate_logo(void) {
  if (s_logo_animating || !s_logo_sequence || !s_logo_bitmap || !s_logo_layer) {
    return;
  }
  s_logo_animating = true;
  app_timer_register(1, prv_logo_timer_callback, NULL);
}

static void prv_clear_double_tap_state(void) {
  if (s_double_tap_timer) {
    app_timer_cancel(s_double_tap_timer);
    s_double_tap_timer = NULL;
  }
  s_waiting_for_second_tap = false;
}

static void prv_double_tap_timeout(void *context) {
  s_double_tap_timer = NULL;
  s_waiting_for_second_tap = false;
}

static void prv_accel_tap_handler(AccelAxisType axis, int32_t direction) {
  if (s_settings.logo_rotation_trigger == LogoRotationTriggerShake) {
    prv_animate_logo();
  }
}

#if defined(PBL_TOUCH)
static void prv_touch_handler(const TouchEvent *event, void *context) {
  if (s_settings.logo_rotation_trigger != LogoRotationTriggerDoubleTap ||
      event->type != TouchEvent_Liftoff) {
    return;
  }

  if (s_waiting_for_second_tap) {
    prv_clear_double_tap_state();
    prv_animate_logo();
    return;
  }

  s_waiting_for_second_tap = true;
  s_double_tap_timer = app_timer_register(DOUBLE_TAP_WINDOW_MS, prv_double_tap_timeout, NULL);
}
#endif

static void prv_set_accel_tap_subscription(bool enabled) {
  if (enabled && !s_accel_tap_subscribed) {
    accel_tap_service_subscribe(prv_accel_tap_handler);
    s_accel_tap_subscribed = true;
  } else if (!enabled && s_accel_tap_subscribed) {
    accel_tap_service_unsubscribe();
    s_accel_tap_subscribed = false;
  }
}

static void prv_set_touch_subscription(bool enabled) {
#if defined(PBL_TOUCH)
  if (enabled && !s_touch_subscribed) {
    if (touch_service_is_enabled()) {
      touch_service_subscribe(prv_touch_handler, NULL);
      s_touch_subscribed = true;
    } else {
      APP_LOG(APP_LOG_LEVEL_WARNING, "Touch unavailable; double tap logo trigger disabled");
    }
  } else if (!enabled && s_touch_subscribed) {
    touch_service_unsubscribe();
    s_touch_subscribed = false;
  }
#else
  if (enabled) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Touch not compiled; double tap logo trigger disabled");
  }
#endif

  if (!enabled) {
    prv_clear_double_tap_state();
  }
}

static void prv_apply_logo_trigger(void) {
  s_skip_first_minute_tick = true;
  prv_set_accel_tap_subscription(s_settings.logo_rotation_trigger == LogoRotationTriggerShake);
  prv_set_touch_subscription(s_settings.logo_rotation_trigger == LogoRotationTriggerDoubleTap);
}

static void prv_format_temperature(char *buffer, size_t buffer_size) {
  if (!s_has_weather) {
    buffer[0] = '\0';
    return;
  }

  if (s_settings.temperature_unit == TemperatureCelsius) {
    snprintf(buffer, buffer_size, "%ld" DEGREE_SYMBOL "C", (long)s_weather_temp_c);
  } else if (s_settings.temperature_unit == TemperatureKelvin) {
    snprintf(buffer, buffer_size, "%ld K", (long)(s_weather_temp_c + 273));
  } else {
    snprintf(buffer, buffer_size, "%ld" DEGREE_SYMBOL "F", (long)((s_weather_temp_c * 9 / 5) + 32));
  }
}

static void prv_format_complication(int32_t type, char *buffer, size_t buffer_size) {
  buffer[0] = '\0';

  if (type == ComplicationTemperature) {
    prv_format_temperature(buffer, buffer_size);
  } else if (type == ComplicationBattery) {
    if (s_battery_charging) {
      snprintf(buffer, buffer_size, "Chg %d%%", s_battery_percent);
    } else {
      snprintf(buffer, buffer_size, "%d%%", s_battery_percent);
    }
  } else if (type == ComplicationUV && s_has_weather) {
    snprintf(buffer, buffer_size, "UV %ld", (long)s_weather_uv);
  }
}

static int16_t prv_text_width(const char *text, GFont font) {
  if (!text || !text[0] || !font) {
    return 0;
  }

  GSize size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, 200, 24), GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return size.w;
}

static void prv_update_complications(void) {
  prv_format_complication(s_settings.complication_left, s_left_buffer, sizeof(s_left_buffer));
  prv_format_complication(s_settings.complication_middle, s_middle_buffer, sizeof(s_middle_buffer));
  prv_format_complication(s_settings.complication_right, s_right_buffer, sizeof(s_right_buffer));

  prv_layout_layers();
  text_layer_set_text(s_left_layer, s_left_buffer);
  text_layer_set_text(s_middle_layer, s_middle_buffer);
  text_layer_set_text(s_right_layer, s_right_buffer);
}

static void prv_update_time_and_date(struct tm *tick_time) {
  if (s_settings.use_24_hour) {
    strftime(s_time_buffer, sizeof(s_time_buffer), "%H:%M", tick_time);
  } else {
    strftime(s_time_buffer, sizeof(s_time_buffer), "%l:%M %p", tick_time);
  }
  text_layer_set_text(s_time_layer, s_time_buffer);

  if (s_settings.date_format == DateFormatMDY) {
    strftime(s_date_buffer, sizeof(s_date_buffer), "%m/%d/%Y", tick_time);
  } else if (s_settings.date_format == DateFormatDMY) {
    strftime(s_date_buffer, sizeof(s_date_buffer), "%d/%m/%Y", tick_time);
  } else {
    strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %e", tick_time);
  }
  text_layer_set_text(s_date_layer, s_date_buffer);
}

static void prv_update_now(void) {
  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  prv_update_time_and_date(tick_time);
}

static void prv_bluetooth_update_proc(Layer *layer, GContext *ctx) {
  if (s_connected) {
    return;
  }

  GRect bounds = layer_get_bounds(layer);
  int16_t mid_x = bounds.size.w / 2;
  int16_t mid_y = bounds.size.h / 2;
  int16_t length = 8;
  int16_t thickness = 3;

  graphics_context_set_stroke_color(ctx, GColorRed);
  graphics_context_set_stroke_width(ctx, thickness);
  graphics_draw_line(ctx, GPoint(mid_x, mid_y - 2 * length), GPoint(mid_x, mid_y + 2 * length));
  graphics_draw_line(ctx, GPoint(mid_x - length, mid_y - length), GPoint(mid_x + length, mid_y + length));
  graphics_draw_line(ctx, GPoint(mid_x - length, mid_y + length), GPoint(mid_x + length, mid_y - length));
  graphics_draw_line(ctx, GPoint(mid_x, mid_y - 2 * length), GPoint(mid_x + length, mid_y - length));
  graphics_draw_line(ctx, GPoint(mid_x, mid_y + 2 * length), GPoint(mid_x + length, mid_y + length));
}

static void prv_apply_colors(void) {
  window_set_background_color(s_window, s_settings.background_color);
  text_layer_set_text_color(s_time_layer, s_settings.text_color);
  text_layer_set_text_color(s_date_layer, s_settings.text_color);
  text_layer_set_text_color(s_left_layer, s_settings.text_color);
  text_layer_set_text_color(s_middle_layer, s_settings.text_color);
  text_layer_set_text_color(s_right_layer, s_settings.text_color);
  bitmap_layer_set_background_color(s_logo_layer, s_settings.background_color);
  prv_load_first_logo_frame();
  layer_mark_dirty(s_bluetooth_layer);
}

static void prv_layout_layers(void) {
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  bool is_round = prv_is_round();
  int16_t width = bounds.size.w;
  int16_t height = bounds.size.h;
  int16_t time_layout_height = is_round ? 48 : 40;
  int16_t date_font_height = is_round ? 26 : 22;
  int16_t small_font_height = 16;
  int16_t time_text_layer_offset = is_round ? -5 : -4;
  int16_t date_text_layer_offset = is_round ? 2 : 1;
  int16_t complication_text_layer_offset = is_round ? 1 : 0;
  int16_t time_y = height / 2 - time_layout_height / 4 + (!is_round ? 12 : 0);
  int16_t date_y = time_y + (time_layout_height * 86 / 100) + 8;
  int16_t complication_y = (!is_round ? height - small_font_height - 5 : height - small_font_height - 10) - 3;

  layer_set_frame(text_layer_get_layer(s_time_layer), GRect(0, time_y + time_text_layer_offset, width, time_layout_height + 8));
  layer_set_frame(text_layer_get_layer(s_date_layer), GRect(0, date_y + date_text_layer_offset, width, date_font_height + 6));

  if (!is_round) {
    text_layer_set_text_alignment(s_left_layer, GTextAlignmentLeft);
    text_layer_set_text_alignment(s_middle_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(s_right_layer, GTextAlignmentRight);

    layer_set_frame(text_layer_get_layer(s_left_layer), GRect(15, complication_y + complication_text_layer_offset, width / 3, small_font_height + 4));
    layer_set_frame(text_layer_get_layer(s_middle_layer), GRect(width / 3, complication_y + complication_text_layer_offset, width / 3, small_font_height + 4));
    layer_set_frame(text_layer_get_layer(s_right_layer), GRect(width - 15 - width / 3, complication_y + complication_text_layer_offset, width / 3, small_font_height + 4));
  } else {
    int16_t gap = 14;
    int16_t safe_inset = 20;
    int16_t middle_width = prv_text_width(s_middle_buffer, s_small_font);
    int16_t left_width = prv_text_width(s_left_buffer, s_small_font);
    int16_t right_width = prv_text_width(s_right_buffer, s_small_font);

    if (middle_width < 1) {
      middle_width = 1;
    }
    if (left_width < 1) {
      left_width = 1;
    }
    if (right_width < 1) {
      right_width = 1;
    }

    int16_t middle_x = (width - middle_width) / 2;
    int16_t flank_y = complication_y - small_font_height / 2 + complication_text_layer_offset;
    int16_t right_x = middle_x + middle_width + gap;
    int16_t left_available = middle_x - gap - safe_inset;
    int16_t right_available = width - safe_inset - right_x;

    if (left_available < 1) {
      left_available = 1;
    }
    if (right_available < 1) {
      right_available = 1;
    }
    if (left_width > left_available) {
      left_width = left_available;
    }
    if (right_width > right_available) {
      right_width = right_available;
    }

    int16_t left_x = middle_x - gap - left_width;

    text_layer_set_text_alignment(s_left_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_middle_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(s_right_layer, GTextAlignmentLeft);

    layer_set_frame(text_layer_get_layer(s_left_layer), GRect(left_x, flank_y, left_width, small_font_height + 4));
    layer_set_frame(text_layer_get_layer(s_middle_layer), GRect(middle_x, complication_y + complication_text_layer_offset, middle_width, small_font_height + 4));
    layer_set_frame(text_layer_get_layer(s_right_layer), GRect(right_x, flank_y, right_width, small_font_height + 4));
  }

  if (s_logo_sequence) {
    GSize logo_size = gbitmap_sequence_get_bitmap_size(s_logo_sequence);
    int16_t logo_x = (width - logo_size.w) / 2;
    int16_t logo_y = ((time_y - logo_size.h) / 2) + 5 + (!is_round ? 5 : 0);
    layer_set_frame(bitmap_layer_get_layer(s_logo_layer), GRect(logo_x, logo_y, logo_size.w, logo_size.h));
  }

  layer_set_frame(s_bluetooth_layer, GRect(width * 82 / 100 - 20, time_y - 30, 40, 40));
}

static void prv_refresh_logo_callback(void *context) {
  if (s_logo_layer) {
    layer_mark_dirty(bitmap_layer_get_layer(s_logo_layer));
    s_refresh_timer = app_timer_register(1000, prv_refresh_logo_callback, NULL);
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  prv_update_time_and_date(tick_time);

  if (!s_skip_first_minute_tick) {
    if (s_settings.logo_rotation_trigger == LogoRotationTriggerMinute) {
      prv_animate_logo();
    } else if (s_settings.logo_rotation_trigger == LogoRotationTriggerHour && tick_time->tm_min == 0) {
      prv_animate_logo();
    }
  }
  s_skip_first_minute_tick = false;

  if (tick_time->tm_min % WEATHER_REFRESH_MINUTES == 0) {
    prv_request_weather();
  }
}

static void prv_battery_handler(BatteryChargeState state) {
  s_battery_percent = state.charge_percent;
  s_battery_charging = state.is_charging;
  prv_update_complications();
}

static void prv_connection_handler(bool connected) {
  bool was_connected = s_connected;
  s_connected = connected;

  layer_mark_dirty(s_bluetooth_layer);
  if (s_connection_initialized) {
    if (!s_connected && was_connected && s_settings.vibe_on_disconnect) {
      vibes_long_pulse();
    } else if (s_connected && !was_connected && s_settings.vibe_on_connect) {
      vibes_double_pulse();
    }
  }
  s_connection_initialized = true;
}

static void prv_unobstructed_change(AnimationProgress progress, void *context) {
  prv_layout_layers();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  bool changed_settings = false;
  bool changed_weather = false;

  Tuple *weather_temp_t = dict_find(iter, MESSAGE_KEY_WeatherTemperatureC);
  if (weather_temp_t) {
    s_weather_temp_c = weather_temp_t->value->int32;
    s_has_weather = true;
    changed_weather = true;
  }

  Tuple *weather_uv_t = dict_find(iter, MESSAGE_KEY_WeatherUV);
  if (weather_uv_t) {
    s_weather_uv = weather_uv_t->value->int32;
    s_has_weather = true;
    changed_weather = true;
  }

  Tuple *bg_t = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if (bg_t) {
    s_settings.background_color = GColorFromHEX(bg_t->value->int32);
    changed_settings = true;
  }

  Tuple *text_t = dict_find(iter, MESSAGE_KEY_TextColor);
  if (text_t) {
    s_settings.text_color = GColorFromHEX(text_t->value->int32);
    changed_settings = true;
  }

  Tuple *temp_unit_t = dict_find(iter, MESSAGE_KEY_TemperatureUnit);
  if (temp_unit_t) {
    s_settings.temperature_unit = prv_clamp_i32(temp_unit_t->value->int32, 0, 2, TemperatureFahrenheit);
    changed_settings = true;
  }

  Tuple *date_t = dict_find(iter, MESSAGE_KEY_DateFormat);
  if (date_t) {
    s_settings.date_format = prv_clamp_i32(date_t->value->int32, 0, 2, DateFormatWords);
    changed_settings = true;
  }

  Tuple *hour_t = dict_find(iter, MESSAGE_KEY_HourFormat);
  if (hour_t) {
    s_settings.use_24_hour = hour_t->value->int32 == 1;
    changed_settings = true;
  }

  Tuple *left_t = dict_find(iter, MESSAGE_KEY_ComplicationLeft);
  if (left_t) {
    s_settings.complication_left = prv_clamp_i32(left_t->value->int32, 0, 3, ComplicationUV);
    changed_settings = true;
  }

  Tuple *middle_t = dict_find(iter, MESSAGE_KEY_ComplicationMiddle);
  if (middle_t) {
    s_settings.complication_middle = prv_clamp_i32(middle_t->value->int32, 0, 3, ComplicationBattery);
    changed_settings = true;
  }

  Tuple *right_t = dict_find(iter, MESSAGE_KEY_ComplicationRight);
  if (right_t) {
    s_settings.complication_right = prv_clamp_i32(right_t->value->int32, 0, 3, ComplicationTemperature);
    changed_settings = true;
  }

  Tuple *disconnect_t = dict_find(iter, MESSAGE_KEY_VibeOnDisconnect);
  if (disconnect_t) {
    s_settings.vibe_on_disconnect = disconnect_t->value->int32 == 1;
    changed_settings = true;
  }

  Tuple *connect_t = dict_find(iter, MESSAGE_KEY_VibeOnConnect);
  if (connect_t) {
    s_settings.vibe_on_connect = connect_t->value->int32 == 1;
    changed_settings = true;
  }

  Tuple *logo_t = dict_find(iter, MESSAGE_KEY_LogoRotationTrigger);
  if (logo_t) {
    s_settings.logo_rotation_trigger = prv_clamp_i32(logo_t->value->int32, 0, 4, LogoRotationTriggerOff);
    prv_apply_logo_trigger();
    changed_settings = true;
  }

  if (changed_settings) {
    prv_save_settings();
    prv_apply_colors();
    prv_update_now();
  }

  if (changed_settings || changed_weather) {
    prv_update_complications();
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Outbox failed: %d", reason);
}

static void prv_main_window_load(Window *window) {
  s_window_layer = window_get_root_layer(window);
  bool is_round = prv_is_round();

  s_time_font = fonts_load_custom_font(resource_get_handle(is_round ? RESOURCE_ID_FONT_GOOGLE_SANS_48 : RESOURCE_ID_FONT_GOOGLE_SANS_40));
  s_date_font = fonts_load_custom_font(resource_get_handle(is_round ? RESOURCE_ID_FONT_GOOGLE_SANS_26 : RESOURCE_ID_FONT_GOOGLE_SANS_22));
  s_small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_GOOGLE_SANS_16));

  s_logo_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_LOGO_ANIMATION);
  s_logo_bitmap = gbitmap_create_blank(gbitmap_sequence_get_bitmap_size(s_logo_sequence), GBitmapFormat8Bit);
  s_logo_layer = bitmap_layer_create(GRect(0, 0, 90, 90));
  bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpSet);
  bitmap_layer_set_background_color(s_logo_layer, s_settings.background_color);

  s_time_layer = text_layer_create(GRectZero);
  s_date_layer = text_layer_create(GRectZero);
  s_left_layer = text_layer_create(GRectZero);
  s_middle_layer = text_layer_create(GRectZero);
  s_right_layer = text_layer_create(GRectZero);

  TextLayer *layers[] = {s_time_layer, s_date_layer, s_left_layer, s_middle_layer, s_right_layer};
  for (size_t i = 0; i < ARRAY_LENGTH(layers); i++) {
    text_layer_set_background_color(layers[i], GColorClear);
    text_layer_set_text_color(layers[i], s_settings.text_color);
    text_layer_set_overflow_mode(layers[i], GTextOverflowModeTrailingEllipsis);
  }

  text_layer_set_font(s_time_layer, s_time_font);
  text_layer_set_font(s_date_layer, s_date_font);
  text_layer_set_font(s_left_layer, s_small_font);
  text_layer_set_font(s_middle_layer, s_small_font);
  text_layer_set_font(s_right_layer, s_small_font);

  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_left_layer, GTextAlignmentLeft);
  text_layer_set_text_alignment(s_middle_layer, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_right_layer, GTextAlignmentRight);

  s_bluetooth_layer = layer_create(GRectZero);
  layer_set_update_proc(s_bluetooth_layer, prv_bluetooth_update_proc);

  layer_add_child(s_window_layer, bitmap_layer_get_layer(s_logo_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_left_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_middle_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_right_layer));
  layer_add_child(s_window_layer, s_bluetooth_layer);

  prv_layout_layers();
  prv_load_first_logo_frame();
  prv_update_now();
  prv_update_complications();

  UnobstructedAreaHandlers unobstructed_handlers = {
    .change = prv_unobstructed_change,
  };
  unobstructed_area_service_subscribe(unobstructed_handlers, NULL);
}

static void prv_main_window_unload(Window *window) {
  unobstructed_area_service_unsubscribe();

  if (s_refresh_timer) {
    app_timer_cancel(s_refresh_timer);
    s_refresh_timer = NULL;
  }
  prv_clear_double_tap_state();

  layer_destroy(s_bluetooth_layer);
  text_layer_destroy(s_right_layer);
  text_layer_destroy(s_middle_layer);
  text_layer_destroy(s_left_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_time_layer);
  bitmap_layer_destroy(s_logo_layer);
  gbitmap_destroy(s_logo_bitmap);
  gbitmap_sequence_destroy(s_logo_sequence);

  fonts_unload_custom_font(s_small_font);
  fonts_unload_custom_font(s_date_font);
  fonts_unload_custom_font(s_time_font);
}

static void prv_init(void) {
  prv_load_settings();

  s_window = window_create();
  window_set_background_color(s_window, s_settings.background_color);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_main_window_load,
    .unload = prv_main_window_unload,
  });
  window_stack_push(s_window, true);

  s_connected = connection_service_peek_pebble_app_connection();
  prv_connection_handler(s_connected);
  prv_apply_logo_trigger();

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
  battery_state_service_subscribe(prv_battery_handler);
  prv_battery_handler(battery_state_service_peek());
  connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = prv_connection_handler,
  });

  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(512, 128);

  s_refresh_timer = app_timer_register(1000, prv_refresh_logo_callback, NULL);
  prv_request_weather();
}

static void prv_deinit(void) {
  prv_set_accel_tap_subscription(false);
  prv_set_touch_subscription(false);
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

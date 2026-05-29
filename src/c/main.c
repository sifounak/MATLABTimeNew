#include <pebble.h>
#include <stdlib.h>

#define SETTINGS_KEY 1
#define WEATHER_CACHE_KEY 2
#define WEATHER_REFRESH_MINUTES 30
#define WEATHER_MIN_UPDATE_SECONDS (30 * 60)
#define DOUBLE_TAP_WINDOW_MS 500
#define DEGREE_SYMBOL "\xC2\xB0"
#define ROTATED_COMPLICATION_BITMAP_WIDTH 56
#define ROTATED_COMPLICATION_BITMAP_HEIGHT 18
#define ROTATED_COMPLICATION_EDGE_INSET 16
#define ROTATED_COMPLICATION_GLYPH_SPACING 1

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
  LeadingZeroNone = 0,
  LeadingZeroDate = 1,
  LeadingZeroTime = 2,
  LeadingZeroDateTime = 3,
} LeadingZeroMode;

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
  int32_t leading_zero_mode;
  int32_t complication_left;
  int32_t complication_middle;
  int32_t complication_right;
  bool rotate_side_text;
  bool vibe_on_disconnect;
  bool vibe_on_connect;
  int32_t logo_rotation_trigger;
} Settings;

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
} LegacySettings;

typedef struct {
  int16_t width;
  int16_t height;
  const uint16_t *rows;
} RotatedGlyph;

typedef struct {
  int32_t temperature_c;
  int32_t uv;
  uint32_t updated_at;
} WeatherCache;

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
static Layer *s_rotated_complication_layer;

static GBitmapSequence *s_logo_sequence;
static GBitmap *s_logo_bitmap;
static GBitmap *s_rotated_left_bitmap;
static GBitmap *s_rotated_right_bitmap;
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
static uint32_t s_weather_updated_at;

static char s_time_buffer[12];
static char s_date_buffer[24];
static char s_left_buffer[20];
static char s_middle_buffer[20];
static char s_right_buffer[20];
static char s_rotated_left_buffer[20];
static char s_rotated_right_buffer[20];

static void prv_layout_layers(void);

static bool prv_is_round(void) {
  GRect bounds = layer_get_bounds(s_window_layer);
  return bounds.size.w == bounds.size.h;
}

static bool prv_should_rotate_side_complications(void) {
  return s_settings.rotate_side_text && prv_is_round();
}

static bool prv_should_show_date_leading_zero(void) {
  return s_settings.leading_zero_mode == LeadingZeroDate ||
         s_settings.leading_zero_mode == LeadingZeroDateTime;
}

static bool prv_should_show_time_leading_zero(void) {
  return s_settings.leading_zero_mode == LeadingZeroTime ||
         s_settings.leading_zero_mode == LeadingZeroDateTime;
}

static void prv_default_settings(void) {
  s_settings.background_color = GColorBlack;
  s_settings.text_color = GColorWhite;
  s_settings.temperature_unit = TemperatureFahrenheit;
  s_settings.date_format = DateFormatWords;
  s_settings.use_24_hour = false;
  s_settings.leading_zero_mode = LeadingZeroNone;
  s_settings.complication_left = ComplicationUV;
  s_settings.complication_middle = ComplicationBattery;
  s_settings.complication_right = ComplicationTemperature;
  s_settings.rotate_side_text = true;
  s_settings.vibe_on_disconnect = true;
  s_settings.vibe_on_connect = true;
  s_settings.logo_rotation_trigger = LogoRotationTriggerOff;
}

static int32_t prv_clamp_i32(int32_t value, int32_t min, int32_t max, int32_t fallback) {
  return (value < min || value > max) ? fallback : value;
}

static bool prv_tuple_get_i32(Tuple *tuple, int32_t *value) {
  if (!tuple || !value) {
    return false;
  }

  if (tuple->type == TUPLE_INT) {
    *value = tuple->value->int32;
    return true;
  }

  if (tuple->type == TUPLE_UINT) {
    *value = (int32_t)tuple->value->uint32;
    return true;
  }

  if (tuple->type == TUPLE_CSTRING) {
    char *end = NULL;
    long parsed = strtol(tuple->value->cstring, &end, 0);
    if (end && end != tuple->value->cstring && *end == '\0') {
      *value = (int32_t)parsed;
      return true;
    }
  }

  return false;
}

static void prv_normalize_settings(void) {
  s_settings.temperature_unit = prv_clamp_i32(s_settings.temperature_unit, 0, 2, TemperatureFahrenheit);
  s_settings.date_format = prv_clamp_i32(s_settings.date_format, 0, 2, DateFormatWords);
  s_settings.use_24_hour = s_settings.use_24_hour ? true : false;
  s_settings.leading_zero_mode = prv_clamp_i32(s_settings.leading_zero_mode, 0, 3, LeadingZeroNone);
  s_settings.complication_left = prv_clamp_i32(s_settings.complication_left, 0, 3, ComplicationUV);
  s_settings.complication_middle = prv_clamp_i32(s_settings.complication_middle, 0, 3, ComplicationBattery);
  s_settings.complication_right = prv_clamp_i32(s_settings.complication_right, 0, 3, ComplicationTemperature);
  s_settings.rotate_side_text = s_settings.rotate_side_text ? true : false;
  s_settings.vibe_on_disconnect = s_settings.vibe_on_disconnect ? true : false;
  s_settings.vibe_on_connect = s_settings.vibe_on_connect ? true : false;
  s_settings.logo_rotation_trigger = prv_clamp_i32(s_settings.logo_rotation_trigger, 0, 4, LogoRotationTriggerOff);
}

static void prv_apply_legacy_settings(const LegacySettings *legacy) {
  s_settings.background_color = legacy->background_color;
  s_settings.text_color = legacy->text_color;
  s_settings.temperature_unit = legacy->temperature_unit;
  s_settings.date_format = legacy->date_format;
  s_settings.use_24_hour = legacy->use_24_hour;
  s_settings.complication_left = legacy->complication_left;
  s_settings.complication_middle = legacy->complication_middle;
  s_settings.complication_right = legacy->complication_right;
  s_settings.vibe_on_disconnect = legacy->vibe_on_disconnect;
  s_settings.vibe_on_connect = legacy->vibe_on_connect;
  s_settings.logo_rotation_trigger = legacy->logo_rotation_trigger;
}

static void prv_load_settings(void) {
  prv_default_settings();
  if (persist_exists(SETTINGS_KEY)) {
    int settings_size = persist_get_size(SETTINGS_KEY);
    if (settings_size == (int)sizeof(s_settings)) {
      persist_read_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
    } else if (settings_size == (int)sizeof(LegacySettings)) {
      LegacySettings legacy;
      persist_read_data(SETTINGS_KEY, &legacy, sizeof(legacy));
      prv_apply_legacy_settings(&legacy);
    }
    prv_normalize_settings();
  }
}

static void prv_save_settings(void) {
  persist_write_data(SETTINGS_KEY, &s_settings, sizeof(s_settings));
}

static void prv_load_weather_cache(void) {
  WeatherCache cache;
  if (persist_exists(WEATHER_CACHE_KEY) &&
      persist_get_size(WEATHER_CACHE_KEY) == (int)sizeof(cache)) {
    persist_read_data(WEATHER_CACHE_KEY, &cache, sizeof(cache));
    if (cache.updated_at > 0) {
      s_weather_temp_c = cache.temperature_c;
      s_weather_uv = cache.uv;
      s_weather_updated_at = cache.updated_at;
      s_has_weather = true;
    }
  }
}

static void prv_save_weather_cache(void) {
  if (!s_has_weather || s_weather_updated_at == 0) {
    return;
  }

  WeatherCache cache = {
    .temperature_c = s_weather_temp_c,
    .uv = s_weather_uv,
    .updated_at = s_weather_updated_at,
  };
  persist_write_data(WEATHER_CACHE_KEY, &cache, sizeof(cache));
}

static bool prv_weather_should_update(void) {
  if (!s_has_weather || s_weather_updated_at == 0) {
    return true;
  }

  time_t now = time(NULL);
  if (now <= 0 || (uint32_t)now < s_weather_updated_at) {
    return false;
  }

  return (uint32_t)now - s_weather_updated_at >= WEATHER_MIN_UPDATE_SECONDS;
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

static void prv_request_weather_if_stale(void) {
  if (prv_weather_should_update()) {
    prv_request_weather();
  }
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
    snprintf(buffer, buffer_size, "%d%%", s_battery_percent);
  } else if (type == ComplicationUV && s_has_weather) {
    snprintf(buffer, buffer_size, "UV%ld", (long)s_weather_uv);
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

static const RotatedGlyph *prv_rotated_glyph(const char **cursor) {
  static const uint16_t rows_0[] = {0x38, 0xfc, 0xc6, 0x186, 0x183, 0x183, 0x183, 0x183, 0x182, 0x186, 0xee, 0x7c};
  static const uint16_t rows_1[] = {0x3, 0xf, 0xb, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3};
  static const uint16_t rows_2[] = {0x1c, 0x7e, 0x63, 0x3, 0x3, 0x6, 0xe, 0x1c, 0x38, 0x30, 0x7f, 0x7f};
  static const uint16_t rows_3[] = {0x1c, 0x3e, 0x63, 0x3, 0x3, 0x1e, 0x1e, 0x3, 0x3, 0x63, 0x77, 0x3e};
  static const uint16_t rows_4[] = {0xe, 0xe, 0x1e, 0x36, 0x26, 0x66, 0xc6, 0xff, 0xff, 0x6, 0x6};
  static const uint16_t rows_5[] = {0x3f, 0x20, 0x60, 0x60, 0x7e, 0x73, 0x3, 0x1, 0x63, 0x77, 0x3e};
  static const uint16_t rows_6[] = {0x4, 0xc, 0xc, 0x18, 0x30, 0x3e, 0x77, 0x63, 0x41, 0x63, 0x77, 0x3e};
  static const uint16_t rows_7[] = {0x7f, 0x3, 0x6, 0x6, 0xc, 0xc, 0x18, 0x18, 0x30, 0x30, 0x60};
  static const uint16_t rows_8[] = {0x1c, 0x3e, 0x63, 0x63, 0x63, 0x3e, 0x3e, 0x63, 0x61, 0x61, 0x77, 0x3e};
  static const uint16_t rows_9[] = {0x1c, 0x3e, 0x63, 0x41, 0x41, 0x63, 0x3f, 0x1e, 0x6, 0xc, 0x18, 0x18};
  static const uint16_t rows_c[] = {0x38, 0xfe, 0x1c3, 0x300, 0x300, 0x300, 0x200, 0x300, 0x300, 0x183, 0x1e7, 0x7e};
  static const uint16_t rows_f[] = {0x7f, 0x60, 0x60, 0x60, 0x60, 0x7f, 0x60, 0x60, 0x60, 0x60, 0x60};
  static const uint16_t rows_g[] = {0x70, 0x1fc, 0x386, 0x600, 0x600, 0x600, 0x41f, 0x601, 0x603, 0x303, 0x3ce, 0xfc};
  static const uint16_t rows_h[] = {0x183, 0x183, 0x183, 0x183, 0x1ff, 0x1ff, 0x183, 0x183, 0x183, 0x183, 0x183};
  static const uint16_t rows_k[] = {0x187, 0x18c, 0x19c, 0x1b8, 0x1b0, 0x1f8, 0x1d8, 0x18c, 0x18e, 0x186, 0x183};
  static const uint16_t rows_u[] = {0x183, 0x183, 0x183, 0x183, 0x183, 0x183, 0x183, 0x183, 0x183, 0xee, 0x7c};
  static const uint16_t rows_v[] = {0x303, 0x183, 0x186, 0xc6, 0xc4, 0xcc, 0x6c, 0x68, 0x78, 0x38, 0x30};
  static const uint16_t rows_percent[] = {0x708, 0xf8c, 0x898, 0x890, 0xfb0, 0x760, 0x4e, 0xdf, 0x191, 0x111, 0x31f, 0x60e};
  static const uint16_t rows_degree[] = {0x6, 0xa, 0x19, 0xe};
  static const RotatedGlyph glyph_0 = {9, 12, rows_0};
  static const RotatedGlyph glyph_1 = {4, 11, rows_1};
  static const RotatedGlyph glyph_2 = {7, 12, rows_2};
  static const RotatedGlyph glyph_3 = {7, 12, rows_3};
  static const RotatedGlyph glyph_4 = {8, 11, rows_4};
  static const RotatedGlyph glyph_5 = {7, 11, rows_5};
  static const RotatedGlyph glyph_6 = {7, 12, rows_6};
  static const RotatedGlyph glyph_7 = {7, 11, rows_7};
  static const RotatedGlyph glyph_8 = {7, 12, rows_8};
  static const RotatedGlyph glyph_9 = {7, 12, rows_9};
  static const RotatedGlyph glyph_c = {10, 12, rows_c};
  static const RotatedGlyph glyph_f = {7, 11, rows_f};
  static const RotatedGlyph glyph_g = {11, 12, rows_g};
  static const RotatedGlyph glyph_h = {9, 11, rows_h};
  static const RotatedGlyph glyph_k = {9, 11, rows_k};
  static const RotatedGlyph glyph_u = {9, 11, rows_u};
  static const RotatedGlyph glyph_v = {10, 11, rows_v};
  static const RotatedGlyph glyph_percent = {12, 12, rows_percent};
  static const RotatedGlyph glyph_degree = {5, 4, rows_degree};

  unsigned char c = (unsigned char)**cursor;

  if (c == '\0') {
    return NULL;
  }
  if (c == 0xc2 && (unsigned char)(*cursor)[1] == 0xb0) {
    *cursor += 2;
    return &glyph_degree;
  }

  (*cursor)++;
  if (c >= 'a' && c <= 'z') {
    c = (unsigned char)(c - 'a' + 'A');
  }

  switch (c) {
    case '0': return &glyph_0;
    case '1': return &glyph_1;
    case '2': return &glyph_2;
    case '3': return &glyph_3;
    case '4': return &glyph_4;
    case '5': return &glyph_5;
    case '6': return &glyph_6;
    case '7': return &glyph_7;
    case '8': return &glyph_8;
    case '9': return &glyph_9;
    case 'C': return &glyph_c;
    case 'F': return &glyph_f;
    case 'G': return &glyph_g;
    case 'H': return &glyph_h;
    case 'K': return &glyph_k;
    case 'U': return &glyph_u;
    case 'V': return &glyph_v;
    case '%': return &glyph_percent;
    case ' ': return NULL;
    default:
      return &glyph_0;
  }
}

static int16_t prv_rotated_text_bitmap_width(const char *text) {
  int16_t width = 0;
  bool has_glyph = false;
  const char *cursor = text;

  while (cursor && cursor[0]) {
    if (has_glyph) {
      width += ROTATED_COMPLICATION_GLYPH_SPACING;
    }
    const RotatedGlyph *glyph = prv_rotated_glyph(&cursor);
    width += glyph ? glyph->width : 4;
    has_glyph = true;
  }

  return width;
}

static void prv_set_rotated_text_pixel(GBitmap *bitmap, int16_t x, int16_t y, uint8_t color) {
  GRect bounds = gbitmap_get_bounds(bitmap);
  if (x < 0 || y < 0 || x >= bounds.size.w || y >= bounds.size.h) {
    return;
  }

  uint8_t *data = gbitmap_get_data(bitmap);
  uint16_t stride = gbitmap_get_bytes_per_row(bitmap);
  data[y * stride + x] = color;
}

static void prv_draw_rotated_text_glyph(GBitmap *bitmap, const RotatedGlyph *glyph,
                                        int16_t x, int16_t y, uint8_t color) {
  if (!glyph) {
    return;
  }

  for (int16_t row = 0; row < glyph->height; row++) {
    for (int16_t col = 0; col < glyph->width; col++) {
      if (!(glyph->rows[row] & (1 << (glyph->width - col - 1)))) {
        continue;
      }
      prv_set_rotated_text_pixel(bitmap, x + col, y + row, color);
    }
  }
}

static void prv_destroy_rotated_complication_bitmaps(void) {
  if (s_rotated_left_bitmap) {
    gbitmap_destroy(s_rotated_left_bitmap);
    s_rotated_left_bitmap = NULL;
  }
  if (s_rotated_right_bitmap) {
    gbitmap_destroy(s_rotated_right_bitmap);
    s_rotated_right_bitmap = NULL;
  }
  s_rotated_left_buffer[0] = '\0';
  s_rotated_right_buffer[0] = '\0';
}

static GBitmap *prv_create_rotated_complication_bitmap(const char *text) {
  if (!text || !text[0]) {
    return NULL;
  }

  GSize bitmap_size = GSize(ROTATED_COMPLICATION_BITMAP_WIDTH, ROTATED_COMPLICATION_BITMAP_HEIGHT);
  GRect bitmap_bounds = GRect(0, 0, bitmap_size.w, bitmap_size.h);
  GBitmap *bitmap = gbitmap_create_blank(bitmap_size, GBitmapFormat8Bit);
  if (!bitmap) {
    return NULL;
  }

  uint8_t *data = gbitmap_get_data(bitmap);
  uint16_t stride = gbitmap_get_bytes_per_row(bitmap);
  memset(data, 0, stride * bitmap_bounds.size.h);

  int16_t text_width = prv_rotated_text_bitmap_width(text);
  int16_t x = (bitmap_bounds.size.w - text_width) / 2;
  int16_t y = (bitmap_bounds.size.h - 16) / 2 + 2;
  uint8_t text_color = s_settings.text_color.argb;
  const char *cursor = text;

  while (cursor && cursor[0]) {
    const RotatedGlyph *glyph = prv_rotated_glyph(&cursor);
    prv_draw_rotated_text_glyph(bitmap, glyph, x, y, text_color);
    x += (glyph ? glyph->width : 4) + ROTATED_COMPLICATION_GLYPH_SPACING;
  }

  return bitmap;
}

static bool prv_update_rotated_complication_bitmap(GBitmap **bitmap, char *cached_text,
                                                   size_t cached_text_size, const char *new_text) {
  if (!prv_should_rotate_side_complications()) {
    bool changed = *bitmap || cached_text[0];
    if (*bitmap) {
      gbitmap_destroy(*bitmap);
      *bitmap = NULL;
    }
    cached_text[0] = '\0';
    return changed;
  }
  if (strncmp(cached_text, new_text, cached_text_size) == 0) {
    return false;
  }

  if (*bitmap) {
    gbitmap_destroy(*bitmap);
    *bitmap = NULL;
  }

  strncpy(cached_text, new_text, cached_text_size);
  cached_text[cached_text_size - 1] = '\0';
  *bitmap = prv_create_rotated_complication_bitmap(cached_text);
  return true;
}

static bool prv_update_rotated_left_bitmap(void) {
  return prv_update_rotated_complication_bitmap(&s_rotated_left_bitmap, s_rotated_left_buffer,
                                                sizeof(s_rotated_left_buffer), s_left_buffer);
}

static bool prv_update_rotated_right_bitmap(void) {
  return prv_update_rotated_complication_bitmap(&s_rotated_right_bitmap, s_rotated_right_buffer,
                                                sizeof(s_rotated_right_buffer), s_right_buffer);
}

static void prv_mark_rotated_complications_dirty_if_needed(bool changed) {
  if (changed && s_rotated_complication_layer) {
    layer_mark_dirty(s_rotated_complication_layer);
  }
}

static void prv_update_complications(void) {
  prv_format_complication(s_settings.complication_left, s_left_buffer, sizeof(s_left_buffer));
  prv_format_complication(s_settings.complication_middle, s_middle_buffer, sizeof(s_middle_buffer));
  prv_format_complication(s_settings.complication_right, s_right_buffer, sizeof(s_right_buffer));

  bool changed_rotated = prv_update_rotated_left_bitmap();
  changed_rotated = prv_update_rotated_right_bitmap() || changed_rotated;
  prv_mark_rotated_complications_dirty_if_needed(changed_rotated);
  prv_layout_layers();
  text_layer_set_text(s_left_layer, s_left_buffer);
  text_layer_set_text(s_middle_layer, s_middle_buffer);
  text_layer_set_text(s_right_layer, s_right_buffer);
}

static void prv_update_complications_for_type(ComplicationType type) {
  bool changed_rotated = false;

  if (s_settings.complication_left == type) {
    prv_format_complication(s_settings.complication_left, s_left_buffer, sizeof(s_left_buffer));
    text_layer_set_text(s_left_layer, s_left_buffer);
    changed_rotated = prv_update_rotated_left_bitmap();
  }

  if (s_settings.complication_middle == type) {
    prv_format_complication(s_settings.complication_middle, s_middle_buffer, sizeof(s_middle_buffer));
    text_layer_set_text(s_middle_layer, s_middle_buffer);
    prv_layout_layers();
  }

  if (s_settings.complication_right == type) {
    prv_format_complication(s_settings.complication_right, s_right_buffer, sizeof(s_right_buffer));
    text_layer_set_text(s_right_layer, s_right_buffer);
    changed_rotated = prv_update_rotated_right_bitmap() || changed_rotated;
  }

  prv_mark_rotated_complications_dirty_if_needed(changed_rotated);
}

static void prv_update_time_and_date(struct tm *tick_time) {
  if (s_settings.use_24_hour) {
    snprintf(s_time_buffer, sizeof(s_time_buffer),
             prv_should_show_time_leading_zero() ? "%02d:%02d" : "%d:%02d",
             tick_time->tm_hour, tick_time->tm_min);
  } else {
    strftime(s_time_buffer, sizeof(s_time_buffer),
             prv_should_show_time_leading_zero() ? "%I:%M %p" : "%l:%M %p",
             tick_time);
  }
  text_layer_set_text(s_time_layer, s_time_buffer);

  bool show_date_leading_zero = prv_should_show_date_leading_zero();
  int year = tick_time->tm_year + 1900;
  int month = tick_time->tm_mon + 1;
  int day = tick_time->tm_mday;

  if (s_settings.date_format == DateFormatMDY) {
    snprintf(s_date_buffer, sizeof(s_date_buffer),
             show_date_leading_zero ? "%02d/%02d/%04d" : "%d/%d/%04d",
             month, day, year);
  } else if (s_settings.date_format == DateFormatDMY) {
    snprintf(s_date_buffer, sizeof(s_date_buffer),
             show_date_leading_zero ? "%02d/%02d/%04d" : "%d/%d/%04d",
             day, month, year);
  } else {
    if (show_date_leading_zero) {
      strftime(s_date_buffer, sizeof(s_date_buffer), "%a %b %d", tick_time);
    } else {
      char date_prefix[12];
      strftime(date_prefix, sizeof(date_prefix), "%a %b", tick_time);
      snprintf(s_date_buffer, sizeof(s_date_buffer), "%s %d", date_prefix, day);
    }
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

static GPoint prv_edge_point_for_degrees(GRect bounds, int32_t degrees) {
  int16_t center_x = bounds.size.w / 2;
  int16_t center_y = bounds.size.h / 2;
  int16_t radius = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2 -
                   ROTATED_COMPLICATION_EDGE_INSET;

  if (degrees == 295) {
    return GPoint(center_x + radius * 423 / 1000, center_y + radius * 906 / 1000);
  }
  if (degrees == 245) {
    return GPoint(center_x - radius * 423 / 1000, center_y + radius * 906 / 1000);
  }

  return GPoint(center_x, center_y);
}

static void prv_draw_rotated_complication(GContext *ctx, GBitmap *bitmap, GPoint destination_center,
                                          int32_t angle) {
  if (!bitmap) {
    return;
  }

  GRect bitmap_bounds = gbitmap_get_bounds(bitmap);
  GPoint source_center = GPoint(bitmap_bounds.size.w / 2, bitmap_bounds.size.h / 2);
  graphics_draw_rotated_bitmap(ctx, bitmap, source_center, angle, destination_center);
}

static void prv_rotated_complication_update_proc(Layer *layer, GContext *ctx) {
  if (!prv_should_rotate_side_complications()) {
    return;
  }

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  GRect bounds = layer_get_bounds(layer);
  prv_draw_rotated_complication(ctx, s_rotated_left_bitmap, prv_edge_point_for_degrees(bounds, 245),
                                TRIG_MAX_ANGLE * 25 / 360);
  prv_draw_rotated_complication(ctx, s_rotated_right_bitmap, prv_edge_point_for_degrees(bounds, 295),
                                TRIG_MAX_ANGLE - TRIG_MAX_ANGLE * 25 / 360);
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
  s_rotated_left_buffer[0] = '\0';
  s_rotated_right_buffer[0] = '\0';
  layer_mark_dirty(s_bluetooth_layer);
}

static void prv_layout_layers(void) {
  GRect bounds = layer_get_unobstructed_bounds(s_window_layer);
  bool is_round = prv_is_round();
  bool rotate_side_text = is_round && s_settings.rotate_side_text;
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
    layer_set_hidden(text_layer_get_layer(s_left_layer), false);
    layer_set_hidden(text_layer_get_layer(s_right_layer), false);
    layer_set_hidden(s_rotated_complication_layer, true);

    text_layer_set_text_alignment(s_left_layer, GTextAlignmentLeft);
    text_layer_set_text_alignment(s_middle_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(s_right_layer, GTextAlignmentRight);

    layer_set_frame(text_layer_get_layer(s_left_layer), GRect(15, complication_y + complication_text_layer_offset, width / 3, small_font_height + 4));
    layer_set_frame(text_layer_get_layer(s_middle_layer), GRect(width / 3, complication_y + complication_text_layer_offset, width / 3, small_font_height + 4));
    layer_set_frame(text_layer_get_layer(s_right_layer), GRect(width - 15 - width / 3, complication_y + complication_text_layer_offset, width / 3, small_font_height + 4));
  } else {
    layer_set_hidden(text_layer_get_layer(s_left_layer), rotate_side_text);
    layer_set_hidden(text_layer_get_layer(s_right_layer), rotate_side_text);
    layer_set_hidden(s_rotated_complication_layer, !rotate_side_text);

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
  layer_set_frame(s_rotated_complication_layer, GRect(0, 0, width, height));
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
    prv_request_weather_if_stale();
  }
}

static void prv_battery_handler(BatteryChargeState state) {
  bool battery_changed = s_battery_percent != state.charge_percent ||
                         s_battery_charging != state.is_charging;
  s_battery_percent = state.charge_percent;
  s_battery_charging = state.is_charging;

  if (battery_changed) {
    prv_update_complications_for_type(ComplicationBattery);
  }
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
  bool changed_temperature = false;
  bool changed_uv = false;
  bool received_weather = false;
  bool had_weather = s_has_weather;
  int32_t value;

  Tuple *weather_temp_t = dict_find(iter, MESSAGE_KEY_WeatherTemperatureC);
  if (prv_tuple_get_i32(weather_temp_t, &value)) {
    changed_temperature = !had_weather || s_weather_temp_c != value;
    s_weather_temp_c = value;
    s_has_weather = true;
    received_weather = true;
  }

  Tuple *weather_uv_t = dict_find(iter, MESSAGE_KEY_WeatherUV);
  if (prv_tuple_get_i32(weather_uv_t, &value)) {
    changed_uv = !had_weather || s_weather_uv != value;
    s_has_weather = true;
    s_weather_uv = value;
    received_weather = true;
  }

  if (received_weather) {
    s_weather_updated_at = (uint32_t)time(NULL);
    prv_save_weather_cache();
  }

  Tuple *bg_t = dict_find(iter, MESSAGE_KEY_BackgroundColor);
  if (prv_tuple_get_i32(bg_t, &value)) {
    s_settings.background_color = GColorFromHEX(value);
    changed_settings = true;
  }

  Tuple *text_t = dict_find(iter, MESSAGE_KEY_TextColor);
  if (prv_tuple_get_i32(text_t, &value)) {
    s_settings.text_color = GColorFromHEX(value);
    changed_settings = true;
  }

  Tuple *temp_unit_t = dict_find(iter, MESSAGE_KEY_TemperatureUnit);
  if (prv_tuple_get_i32(temp_unit_t, &value)) {
    s_settings.temperature_unit = prv_clamp_i32(value, 0, 2, TemperatureFahrenheit);
    changed_settings = true;
  }

  Tuple *date_t = dict_find(iter, MESSAGE_KEY_DateFormat);
  if (prv_tuple_get_i32(date_t, &value)) {
    s_settings.date_format = prv_clamp_i32(value, 0, 2, DateFormatWords);
    changed_settings = true;
  }

  Tuple *hour_t = dict_find(iter, MESSAGE_KEY_HourFormat);
  if (prv_tuple_get_i32(hour_t, &value)) {
    s_settings.use_24_hour = value == 1;
    changed_settings = true;
  }

  Tuple *leading_zeros_t = dict_find(iter, MESSAGE_KEY_LeadingZeros);
  if (prv_tuple_get_i32(leading_zeros_t, &value)) {
    s_settings.leading_zero_mode = prv_clamp_i32(value, 0, 3, LeadingZeroNone);
    changed_settings = true;
  }

  Tuple *left_t = dict_find(iter, MESSAGE_KEY_ComplicationLeft);
  if (prv_tuple_get_i32(left_t, &value)) {
    s_settings.complication_left = prv_clamp_i32(value, 0, 3, ComplicationUV);
    changed_settings = true;
  }

  Tuple *middle_t = dict_find(iter, MESSAGE_KEY_ComplicationMiddle);
  if (prv_tuple_get_i32(middle_t, &value)) {
    s_settings.complication_middle = prv_clamp_i32(value, 0, 3, ComplicationBattery);
    changed_settings = true;
  }

  Tuple *right_t = dict_find(iter, MESSAGE_KEY_ComplicationRight);
  if (prv_tuple_get_i32(right_t, &value)) {
    s_settings.complication_right = prv_clamp_i32(value, 0, 3, ComplicationTemperature);
    changed_settings = true;
  }

  Tuple *rotate_side_t = dict_find(iter, MESSAGE_KEY_RotateSideText);
  if (prv_tuple_get_i32(rotate_side_t, &value)) {
    s_settings.rotate_side_text = value == 1;
    changed_settings = true;
  }

  Tuple *disconnect_t = dict_find(iter, MESSAGE_KEY_VibeOnDisconnect);
  if (prv_tuple_get_i32(disconnect_t, &value)) {
    s_settings.vibe_on_disconnect = value == 1;
    changed_settings = true;
  }

  Tuple *connect_t = dict_find(iter, MESSAGE_KEY_VibeOnConnect);
  if (prv_tuple_get_i32(connect_t, &value)) {
    s_settings.vibe_on_connect = value == 1;
    changed_settings = true;
  }

  Tuple *logo_t = dict_find(iter, MESSAGE_KEY_LogoRotationTrigger);
  if (prv_tuple_get_i32(logo_t, &value)) {
    s_settings.logo_rotation_trigger = prv_clamp_i32(value, 0, 4, LogoRotationTriggerOff);
    prv_apply_logo_trigger();
    changed_settings = true;
  }

  if (changed_settings) {
    prv_save_settings();
    prv_apply_colors();
    prv_update_now();
  }

  if (changed_settings) {
    prv_update_complications();
  } else {
    if (changed_temperature) {
      prv_update_complications_for_type(ComplicationTemperature);
    }
    if (changed_uv) {
      prv_update_complications_for_type(ComplicationUV);
    }
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
  s_rotated_complication_layer = layer_create(GRectZero);
  layer_set_update_proc(s_rotated_complication_layer, prv_rotated_complication_update_proc);

  layer_add_child(s_window_layer, bitmap_layer_get_layer(s_logo_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_time_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_date_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_left_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_middle_layer));
  layer_add_child(s_window_layer, text_layer_get_layer(s_right_layer));
  layer_add_child(s_window_layer, s_bluetooth_layer);
  layer_add_child(s_window_layer, s_rotated_complication_layer);

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

  layer_destroy(s_rotated_complication_layer);
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
  prv_load_weather_cache();

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
  s_skip_first_minute_tick = true;

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
  prv_request_weather_if_stale();
}

static void prv_deinit(void) {
  prv_set_accel_tap_subscription(false);
  prv_set_touch_subscription(false);
  connection_service_unsubscribe();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  app_message_deregister_callbacks();
  window_destroy(s_window);
  prv_destroy_rotated_complication_bitmaps();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

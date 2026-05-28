#include <pebble.h>

static Window *s_window;
static BitmapLayer *s_logo_bitmap_layer;
static GBitmapSequence *s_logo_sequence = NULL;
static GBitmap *s_logo_bitmap = NULL;
static bool s_first_minute = true;
static GColor s_bg_color;

static void clear_bitmap_to_bg() {
  uint8_t *data = gbitmap_get_data(s_logo_bitmap);
  uint16_t stride = gbitmap_get_bytes_per_row(s_logo_bitmap);
  GRect bounds = gbitmap_get_bounds(s_logo_bitmap);
  memset(data, s_bg_color.argb, stride * bounds.size.h);
}

static void load_first_frame() {
  gbitmap_sequence_restart(s_logo_sequence);
  clear_bitmap_to_bg();
  gbitmap_sequence_update_bitmap_next_frame(s_logo_sequence, s_logo_bitmap, NULL);
  bitmap_layer_set_bitmap(s_logo_bitmap_layer, s_logo_bitmap);
  layer_mark_dirty(bitmap_layer_get_layer(s_logo_bitmap_layer));
}

static void logo_timer_callback(void *context) {
  uint32_t next_delay;

  clear_bitmap_to_bg();
  if (gbitmap_sequence_update_bitmap_next_frame(s_logo_sequence, s_logo_bitmap, &next_delay)) {
    bitmap_layer_set_bitmap(s_logo_bitmap_layer, s_logo_bitmap);
    app_timer_register(next_delay, logo_timer_callback, NULL);
    layer_mark_dirty(bitmap_layer_get_layer(s_logo_bitmap_layer));
  } else {
    // Animation complete, reset to first frame
    load_first_frame();
  }
}

static void animate_logo() {
  app_timer_register(1, logo_timer_callback, NULL);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_first_minute) {
    s_first_minute = false;
    return;
  }
  animate_logo();
}

static void refresh_logo_callback(void *context) {
  layer_mark_dirty(bitmap_layer_get_layer(s_logo_bitmap_layer));
  app_timer_register(1000, refresh_logo_callback, NULL);
}

int main(void) {
  s_window = window_create();
  window_stack_push(s_window, true);

  // Background color (default black, matching JS default)
  s_bg_color = GColorBlack;

  // Create logo layer
  s_logo_sequence = gbitmap_sequence_create_with_resource(RESOURCE_ID_LOGO_ANIMATION);
  GSize logo_size = gbitmap_sequence_get_bitmap_size(s_logo_sequence);
  s_logo_bitmap = gbitmap_create_blank(logo_size, GBitmapFormat8Bit);

  // Position logo to match JS layout:
  // timeY = screenH/2 - fontHeight*0.25, logoY = (timeY - logoH) / 2 + 5
  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));
  bool is_emery = bounds.size.w >= 200;
  int time_y = bounds.size.h / 2 - 12;  // 48 * 0.25 = 12
  int logo_x = (bounds.size.w - logo_size.w) / 2;
  int logo_y = (time_y - logo_size.h) / 2 + 5 + (is_emery ? 5 : 0);
  s_logo_bitmap_layer = bitmap_layer_create(GRect(logo_x, logo_y, logo_size.w, logo_size.h));
  bitmap_layer_set_compositing_mode(s_logo_bitmap_layer, GCompOpSet);
  bitmap_layer_set_background_color(s_logo_bitmap_layer, s_bg_color);
  load_first_frame();

  layer_add_child(window_get_root_layer(s_window), bitmap_layer_get_layer(s_logo_bitmap_layer));

  // Subscribe to minute ticks for animation trigger
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  // Periodically redraw logo on top of Poco framebuffer
  app_timer_register(1000, refresh_logo_callback, NULL);

  // Start Moddable JS engine
  (void)moddable_createMachine(NULL);

  // Cleanup
  tick_timer_service_unsubscribe();
  bitmap_layer_destroy(s_logo_bitmap_layer);
  gbitmap_sequence_destroy(s_logo_sequence);
  gbitmap_destroy(s_logo_bitmap);
  window_destroy(s_window);
}

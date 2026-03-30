#include <pebble.h>

// --- Constants ---
#define PERSIST_KEY_RATIO   1
#define PERSIST_KEY_COFFEE  2

#define DEFAULT_RATIO      17
#define DEFAULT_COFFEE     20
#define MIN_RATIO          10
#define MAX_RATIO          25
#define MIN_COFFEE         10
#define MAX_COFFEE         100
#define MIN_WATER          100
#define MAX_WATER          2500
#define ACCEL_THRESHOLD    14  // ticks before accelerating (14 * 100ms = ~1.4s)

typedef enum {
  FOCUS_COFFEE,
  FOCUS_WATER
} Focus;

// --- State ---
static int s_ratio;
static int s_coffee_cg;  // coffee in centigrams (e.g. 2000 = 20.0g) for decimal precision
static int s_water_ml;
static Focus s_focus = FOCUS_COFFEE;
static bool s_editing_ratio = false;
static int s_repeat_count = 0;
static int s_last_button = -1;

// --- UI ---
static Window *s_main_window;
static TextLayer *s_ratio_layer;
static TextLayer *s_coffee_label_layer;
static TextLayer *s_coffee_value_layer;
static TextLayer *s_water_label_layer;
static TextLayer *s_water_value_layer;
static Layer *s_coffee_bg_layer;
static Layer *s_water_bg_layer;

// Buffers for formatted text
static char s_ratio_buf[16];
static char s_coffee_buf[16];
static char s_water_buf[16];

// --- Layout constants ---
#define SCREEN_W 144
#define SCREEN_H 168
#define RATIO_H  26
#define LABEL_H  22
#define VALUE_H  40
#define SIDE_PAD 4

// Y positions (matched to Figma design)
#define RATIO_Y         4
#define COFFEE_BG_Y     28
#define COFFEE_BG_H     74
#define COFFEE_LABEL_Y  30
#define COFFEE_VALUE_Y  54
#define WATER_LABEL_Y   106
#define WATER_BG_Y      (WATER_LABEL_Y - 2)
#define WATER_VALUE_Y   130
#define WATER_BG_H      74

// --- Forward declarations ---
static void update_display(void);
static void recalculate(void);

// --- Background drawing for inverted selection ---
static void coffee_bg_update_proc(Layer *layer, GContext *ctx) {
  if (s_editing_ratio) return;
  if (s_focus == FOCUS_COFFEE) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  }
}

static void water_bg_update_proc(Layer *layer, GContext *ctx) {
  if (s_editing_ratio) return;
  if (s_focus == FOCUS_WATER) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  }
}

// --- Recalculation ---
static void recalculate(void) {
  if (s_focus == FOCUS_COFFEE) {
    // Water = coffee * ratio
    s_water_ml = (s_coffee_cg * s_ratio) / 100;
    // Clamp water
    if (s_water_ml > MAX_WATER) s_water_ml = MAX_WATER;
    if (s_water_ml < MIN_WATER) s_water_ml = MIN_WATER;
  } else {
    // Coffee = water / ratio (store as centigrams for 1 decimal)
    s_coffee_cg = (s_water_ml * 100) / s_ratio;
    // Clamp coffee
    if (s_coffee_cg > MAX_COFFEE * 100) s_coffee_cg = MAX_COFFEE * 100;
    if (s_coffee_cg < MIN_COFFEE * 100) s_coffee_cg = MIN_COFFEE * 100;
  }
}

// --- Display update ---
static void update_display(void) {
  // Ratio text (inverted colors indicate edit mode)
  snprintf(s_ratio_buf, sizeof(s_ratio_buf), "1:%d", s_ratio);
  text_layer_set_text(s_ratio_layer, s_ratio_buf);

  // Coffee value (units shown in label, LECO font is numbers-only)
  int whole = s_coffee_cg / 100;
  int frac = (s_coffee_cg % 100) / 10;
  snprintf(s_coffee_buf, sizeof(s_coffee_buf), "%d.%d", whole, frac);
  text_layer_set_text(s_coffee_value_layer, s_coffee_buf);

  // Water value
  snprintf(s_water_buf, sizeof(s_water_buf), "%d", s_water_ml);
  text_layer_set_text(s_water_value_layer, s_water_buf);

  // Colors based on focus (inverted for selected)
  if (s_editing_ratio) {
    // In ratio edit mode, dim both sections
    text_layer_set_text_color(s_coffee_label_layer, GColorBlack);
    text_layer_set_background_color(s_coffee_label_layer, GColorClear);
    text_layer_set_text_color(s_coffee_value_layer, GColorBlack);
    text_layer_set_background_color(s_coffee_value_layer, GColorClear);
    text_layer_set_text_color(s_water_label_layer, GColorBlack);
    text_layer_set_background_color(s_water_label_layer, GColorClear);
    text_layer_set_text_color(s_water_value_layer, GColorBlack);
    text_layer_set_background_color(s_water_value_layer, GColorClear);
    // Highlight ratio
    text_layer_set_text_color(s_ratio_layer, GColorWhite);
    text_layer_set_background_color(s_ratio_layer, GColorBlack);
  } else {
    // Normal ratio
    text_layer_set_text_color(s_ratio_layer, GColorBlack);
    text_layer_set_background_color(s_ratio_layer, GColorClear);

    if (s_focus == FOCUS_COFFEE) {
      // Coffee inverted
      text_layer_set_text_color(s_coffee_label_layer, GColorWhite);
      text_layer_set_background_color(s_coffee_label_layer, GColorClear);
      text_layer_set_text_color(s_coffee_value_layer, GColorWhite);
      text_layer_set_background_color(s_coffee_value_layer, GColorClear);
      // Water normal
      text_layer_set_text_color(s_water_label_layer, GColorBlack);
      text_layer_set_background_color(s_water_label_layer, GColorClear);
      text_layer_set_text_color(s_water_value_layer, GColorBlack);
      text_layer_set_background_color(s_water_value_layer, GColorClear);
    } else {
      // Coffee normal
      text_layer_set_text_color(s_coffee_label_layer, GColorBlack);
      text_layer_set_background_color(s_coffee_label_layer, GColorClear);
      text_layer_set_text_color(s_coffee_value_layer, GColorBlack);
      text_layer_set_background_color(s_coffee_value_layer, GColorClear);
      // Water inverted
      text_layer_set_text_color(s_water_label_layer, GColorWhite);
      text_layer_set_background_color(s_water_label_layer, GColorClear);
      text_layer_set_text_color(s_water_value_layer, GColorWhite);
      text_layer_set_background_color(s_water_value_layer, GColorClear);
    }
  }

  // Mark background layers dirty to trigger redraws
  layer_mark_dirty(s_coffee_bg_layer);
  layer_mark_dirty(s_water_bg_layer);
}

// --- Acceleration helper ---
static void track_repeat(int button_id) {
  if (s_last_button == button_id) {
    s_repeat_count++;
  } else {
    s_repeat_count = 1;
    s_last_button = button_id;
  }
}

static bool is_accelerated(void) {
  return s_repeat_count > ACCEL_THRESHOLD;
}

// --- Button handlers ---
static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_editing_ratio) {
    if (s_ratio < MAX_RATIO) s_ratio++;
    recalculate();
    update_display();
    return;
  }

  track_repeat(BUTTON_ID_UP);
  if (s_focus == FOCUS_COFFEE) {
    int step = is_accelerated() ? 100 : 10; // 1g or 0.1g
    if (s_coffee_cg < MAX_COFFEE * 100) s_coffee_cg += step;
    if (s_coffee_cg > MAX_COFFEE * 100) s_coffee_cg = MAX_COFFEE * 100;
  } else {
    int step = is_accelerated() ? 10 : 1;   // 10ml or 1ml
    if (s_water_ml < MAX_WATER) s_water_ml += step;
    if (s_water_ml > MAX_WATER) s_water_ml = MAX_WATER;
  }
  recalculate();
  update_display();
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_editing_ratio) {
    if (s_ratio > MIN_RATIO) s_ratio--;
    recalculate();
    update_display();
    return;
  }

  track_repeat(BUTTON_ID_DOWN);
  if (s_focus == FOCUS_COFFEE) {
    int step = is_accelerated() ? 100 : 10; // 1g or 0.1g
    if (s_coffee_cg > MIN_COFFEE * 100) s_coffee_cg -= step;
    if (s_coffee_cg < MIN_COFFEE * 100) s_coffee_cg = MIN_COFFEE * 100;
  } else {
    int step = is_accelerated() ? 10 : 1;   // 10ml or 1ml
    if (s_water_ml > MIN_WATER) s_water_ml -= step;
    if (s_water_ml < MIN_WATER) s_water_ml = MIN_WATER;
  }
  recalculate();
  update_display();
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_editing_ratio) {
    // Confirm ratio and save
    persist_write_int(PERSIST_KEY_RATIO, s_ratio);
    s_editing_ratio = false;
    recalculate();
    update_display();
    return;
  }

  // Toggle focus (don't recalculate — values stay as they are)
  s_focus = (s_focus == FOCUS_COFFEE) ? FOCUS_WATER : FOCUS_COFFEE;
  update_display();
}

static void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (!s_editing_ratio) {
    s_editing_ratio = true;
    update_display();
  }
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_editing_ratio) {
    // Cancel ratio edit — reload saved value
    s_ratio = persist_exists(PERSIST_KEY_RATIO) ? persist_read_int(PERSIST_KEY_RATIO) : DEFAULT_RATIO;
    s_editing_ratio = false;
    recalculate();
    update_display();
    return;
  }

  // Save state before exiting
  persist_write_int(PERSIST_KEY_COFFEE, s_coffee_cg);
  window_stack_pop(true);
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, up_click_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, down_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

// --- Window lifecycle ---
static TextLayer *create_text_layer(GRect frame, GFont font, GTextAlignment align) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_font(layer, font);
  text_layer_set_text_alignment(layer, align);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorBlack);
  return layer;
}

static void main_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);

  GFont font_ratio = fonts_get_system_font(FONT_KEY_LECO_20_BOLD_NUMBERS);
  GFont font_large = fonts_get_system_font(FONT_KEY_LECO_32_BOLD_NUMBERS);
  GFont font_label = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Ratio label (top center)
  s_ratio_layer = create_text_layer(
    GRect(SIDE_PAD, RATIO_Y, SCREEN_W - SIDE_PAD * 2, RATIO_H),
    font_ratio, GTextAlignmentCenter
  );
  layer_add_child(root, text_layer_get_layer(s_ratio_layer));

  // Coffee background layer (for inverted selection)
  s_coffee_bg_layer = layer_create(GRect(0, COFFEE_BG_Y, SCREEN_W, COFFEE_BG_H));
  layer_set_update_proc(s_coffee_bg_layer, coffee_bg_update_proc);
  layer_add_child(root, s_coffee_bg_layer);

  // Coffee label
  s_coffee_label_layer = create_text_layer(
    GRect(SIDE_PAD, COFFEE_LABEL_Y, SCREEN_W - SIDE_PAD * 2, LABEL_H),
    font_label, GTextAlignmentCenter
  );
  text_layer_set_text(s_coffee_label_layer, "COFFEE (g)");
  layer_add_child(root, text_layer_get_layer(s_coffee_label_layer));

  // Coffee value
  s_coffee_value_layer = create_text_layer(
    GRect(SIDE_PAD, COFFEE_VALUE_Y, SCREEN_W - SIDE_PAD * 2, VALUE_H),
    font_large, GTextAlignmentCenter
  );
  layer_add_child(root, text_layer_get_layer(s_coffee_value_layer));

  // Water background layer
  s_water_bg_layer = layer_create(GRect(0, WATER_BG_Y, SCREEN_W, WATER_BG_H));
  layer_set_update_proc(s_water_bg_layer, water_bg_update_proc);
  layer_add_child(root, s_water_bg_layer);

  // Water label
  s_water_label_layer = create_text_layer(
    GRect(SIDE_PAD, WATER_LABEL_Y, SCREEN_W - SIDE_PAD * 2, LABEL_H),
    font_label, GTextAlignmentCenter
  );
  text_layer_set_text(s_water_label_layer, "WATER (ml)");
  layer_add_child(root, text_layer_get_layer(s_water_label_layer));

  // Water value
  s_water_value_layer = create_text_layer(
    GRect(SIDE_PAD, WATER_VALUE_Y, SCREEN_W - SIDE_PAD * 2, VALUE_H),
    font_large, GTextAlignmentCenter
  );
  layer_add_child(root, text_layer_get_layer(s_water_value_layer));

  // Initial display
  recalculate();
  update_display();
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_ratio_layer);
  text_layer_destroy(s_coffee_label_layer);
  text_layer_destroy(s_coffee_value_layer);
  text_layer_destroy(s_water_label_layer);
  text_layer_destroy(s_water_value_layer);
  layer_destroy(s_coffee_bg_layer);
  layer_destroy(s_water_bg_layer);
}

// --- App lifecycle ---
static void init(void) {
  // Load persisted values
  s_ratio = persist_exists(PERSIST_KEY_RATIO) ? persist_read_int(PERSIST_KEY_RATIO) : DEFAULT_RATIO;
  s_coffee_cg = persist_exists(PERSIST_KEY_COFFEE) ? persist_read_int(PERSIST_KEY_COFFEE) : DEFAULT_COFFEE * 100;

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  // Save state on exit
  persist_write_int(PERSIST_KEY_COFFEE, s_coffee_cg);
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

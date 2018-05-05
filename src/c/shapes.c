#include <pebble.h>
#include "SmallMaths.h"

#define SETTINGS_KEY 1

static Window *s_window;
static Layer *bitmap_layer;

static GColor background_color;
static GColor line_color;
static bool swap_hour_min;
static bool outline_shape;
static int line_spacing;
static int shape_size;
static GPath *filling_path = NULL;

typedef struct ClaySettings {
  GColor BackgroundColor;
  GColor LineColor;
  bool SwapHourMin;
  bool OutlineShape;
  int LineSpacing;
  int ShapeSize;
} ClaySettings;

static ClaySettings settings;

static void bitmap_layer_update_proc(Layer *layer, GContext* ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int minute = (*t).tm_min;
  int hour = (*t).tm_hour;

  if (swap_hour_min) {
    int temp = hour;
    hour = minute / 5;
    minute = temp * 5;
  }

  int five_minute = minute / 5;

  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  int diameter = sm_sqrt(sm_powint(bounds.size.w,2) + sm_powint(bounds.size.h,2));

  // background color
  graphics_context_set_fill_color(ctx, background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int32_t hour_angle = TRIG_MAX_ANGLE * (hour % 12) / 12.0;
  int32_t perpendicular = hour_angle + TRIG_MAX_RATIO / 4;

  graphics_context_set_stroke_width(ctx, 1);
  graphics_context_set_stroke_color(ctx, line_color);
  graphics_context_set_antialiased(ctx, true);

  for (int shift = -diameter / 2; shift < diameter / 2; shift += line_spacing) {
    int shiftX =   sin_lookup(perpendicular) * shift / TRIG_MAX_RATIO;
    int shiftY = - cos_lookup(perpendicular) * shift / TRIG_MAX_RATIO;

    GPoint diameterPoint1 = GPoint(
      center.x + sin_lookup(hour_angle) * (diameter / 2) / TRIG_MAX_RATIO + shiftX,
      center.y - cos_lookup(hour_angle) * (diameter / 2) / TRIG_MAX_RATIO + shiftY
    );
    GPoint diameterPoint2 = GPoint(
      center.x - sin_lookup(hour_angle) * (diameter / 2) / TRIG_MAX_RATIO + shiftX,
      center.y + cos_lookup(hour_angle) * (diameter / 2) / TRIG_MAX_RATIO + shiftY
    );

    graphics_draw_line(ctx, diameterPoint1, diameterPoint2);
  }

  if (five_minute == 0) {
    // no drawing
  } else if (five_minute == 1) {
    graphics_fill_circle(ctx, center, shape_size);
    if (outline_shape) {
      graphics_draw_circle(ctx, center, shape_size);
    }
  } else if (five_minute == 2) {
    int line_width = 15;
    graphics_context_set_stroke_width(ctx, line_width);
    graphics_context_set_stroke_color(ctx, background_color);
    graphics_draw_circle(ctx, center, shape_size - line_width/2.0);
    if (outline_shape) {
      graphics_context_set_stroke_width(ctx, 1);
      graphics_context_set_stroke_color(ctx, line_color);
      graphics_draw_circle(ctx, center, shape_size - line_width);
      graphics_draw_circle(ctx, center, shape_size);
    }
  } else {
    int angle_per_corner = TRIG_MAX_ANGLE / five_minute;
    GPoint points[12];
    for (int i = 0; i < five_minute; i++) {
      points[i] = GPoint(
        center.x + sin_lookup(angle_per_corner * i) * (shape_size) / TRIG_MAX_RATIO,
        center.y - cos_lookup(angle_per_corner * i) * (shape_size) / TRIG_MAX_RATIO
      );
    }
    GPathInfo pathInfo = {
      .num_points = five_minute,
      .points = points
    };
    if (filling_path != NULL) {
      gpath_destroy(filling_path);
    }
    filling_path = gpath_create(&pathInfo);
    gpath_draw_filled(ctx, filling_path);
    if (outline_shape) {
      gpath_draw_outline(ctx, filling_path);
    }
  }
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

    bitmap_layer = layer_create(bounds);
  layer_set_update_proc(bitmap_layer, bitmap_layer_update_proc);
  layer_add_child(window_layer, bitmap_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(bitmap_layer);
  if (filling_path != NULL) {
    gpath_destroy(filling_path);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  int minute = (*tick_time).tm_min;
  int hour = (*tick_time).tm_hour;
  if ((minute + hour * 60) % 5 == 0) {
    layer_mark_dirty(bitmap_layer);
  }
}

static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  // Read color preferences
  Tuple *bg_color_t = dict_find(iter, MESSAGE_KEY_background_color);
  if(bg_color_t) {
    background_color = GColorFromHEX(bg_color_t->value->int32);
  }
  Tuple *line_color_t = dict_find(iter, MESSAGE_KEY_line_color);
  if(line_color_t) {
    line_color = GColorFromHEX(line_color_t->value->int32);
  }
  Tuple *swap_hour_min_t = dict_find(iter, MESSAGE_KEY_swap_hour_min);
  if (swap_hour_min_t) {
    swap_hour_min = swap_hour_min_t->value->uint8 != 0;
  }
  Tuple *outline_shape_t = dict_find(iter, MESSAGE_KEY_outline_shape);
  if (outline_shape_t) {
    outline_shape = outline_shape_t->value->uint8 != 0;
  }
  Tuple *line_spacing_t = dict_find(iter, MESSAGE_KEY_line_spacing);
  if (line_spacing_t) {
    line_spacing = line_spacing_t->value->int32;
  }
  Tuple *shape_size_t = dict_find(iter, MESSAGE_KEY_shape_size);
  if (shape_size_t) {
    shape_size = shape_size_t->value->int32;
  }

  layer_mark_dirty(bitmap_layer);

  settings.BackgroundColor = background_color;
  settings.LineColor = line_color;
  settings.SwapHourMin = swap_hour_min;
  settings.OutlineShape = outline_shape;
  settings.LineSpacing = line_spacing;
  settings.ShapeSize = shape_size;

  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));
}

static void prv_init(void) {
    // default settings
  settings.BackgroundColor = GColorWhite;
  settings.LineColor = GColorFromRGBA(205, 34, 49, 255);
  settings.SwapHourMin = false;
  settings.OutlineShape = false;
  settings.LineSpacing = 5;
  settings.ShapeSize = 30;

  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
  // apply saved data
  background_color = settings.BackgroundColor;
  line_color = settings.LineColor;
  swap_hour_min = settings.SwapHourMin;
  outline_shape = settings.OutlineShape;
  line_spacing = settings.LineSpacing;
  shape_size = settings.ShapeSize;

  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_open(128, 128);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", s_window);

  app_event_loop();
  prv_deinit();
}

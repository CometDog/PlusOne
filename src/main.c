#include "pebble.h"

#ifdef PBL_COLOR
  #include "gcolor_definitions.h" // Allows the use of color
#endif

static Window *s_main_window; // Main window
static TextLayer *s_month_label, *s_day_label, 
  *s_hour_label, *s_min_label, 
  *s_state_top_label, *s_state_bottom_label; // Labels for text
static Layer *s_solid_layer, *s_time_layer, *s_battery_layer; // Background layers

// Integers to store dot numbers and positions
int dot = 0;
int x = 12;
int y = 156;

// Bluetooth
int state = 1; // Determines which state the state_label is in
bool bt = true; // Determines state that bluetooth is in. True is on.

//Number of shakes
bool shake = false; // Determines if the watch has been shaken (shook?)

// Buffers
static char s_month_buffer[] = "MM";
static char s_day_buffer[] = "DD";
static char s_hour_buffer[] = "hh";
static char s_min_buffer[] = "mm";

// Update background when called
static void update_bg(Layer *layer, GContext *ctx) {
  
  GRect bounds = layer_get_bounds(layer); // Set bounds to full window
  
  // Anti-aliasing
  #ifdef PBL_COLOR
    graphics_context_set_antialiased(ctx, true);
  #endif
  
  //Background color
  #ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, GColorVividCerulean); // Set the fill color.
  #else 
    graphics_context_set_fill_color(ctx, GColorBlack); // Set the fill color.
  #endif
  graphics_fill_rect(ctx, bounds, 0, GCornerNone); // Fill the screen
  
  // Reset dot position and count
  dot = 1;
  x = 12;
  y = 156;
  
  //Time dots
  graphics_context_set_fill_color(ctx, GColorWhite);
  while (dot < 60) { // Create only 59 dots
    if (dot == 56) {
      x = 24;
    }
    if (dot == 59) {
      y = y - 12;
      x = 36;
    }
    graphics_fill_circle(ctx, GPoint(x,y), 1);
    x += 12; // Move along x-axis at 12 pixels each iteration...
    if (x > (5 * 12)) { // ...until there are five in a row...
      x = 12; // ... then reset the x position...
      y = y - 12; // ... and move to the next row up
    }
    dot += 1; // Next dot
  }
  
  // Reset dot position and count for battery position
  dot = 1;
  x = 77;
  y = 156;
  
  //Battery dots
  #ifdef PBL_COLOR 
    graphics_context_set_fill_color(ctx, GColorYellow);
  #else
    graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  while (dot <= 10) { // Create only 10 dots
    graphics_fill_circle(ctx, GPoint(x,y), 1);
    y = y - 12; // Move to next row up
    dot += 1; // Next dot
  }
}

// Handles updating time
static void update_time(Layer *layer, GContext *ctx) {
  
  // Reset dot position and count
  dot = 0;
  x = 12;
  y = 156;
  
  GRect bounds = layer_get_bounds(layer); // Set bounds to full window
  GPoint center = grect_center_point(&bounds); // Find center
  
  // Anti-aliasing
  #ifdef PBL_COLOR
    graphics_context_set_antialiased(ctx, true);
  #endif
    
  // Get time and structure
  time_t temp = time(NULL); 
  struct tm *t = localtime(&temp);
  int sec = t->tm_sec;
  
  // Set the time a second in the future to solve a bug
  temp = time(NULL) + 1;
  t = localtime(&temp);
  
  //Background circle
  graphics_context_set_fill_color(ctx, GColorWhite);
  while (dot < sec) { // Loops while the dot count is lower than the second amount
    if (dot == 55) {
      x = 24;
    }
    if (dot == 58) {
      y = y - 12;
      x = 36;
    }
    graphics_fill_circle(ctx, GPoint(x,y), 4);
    x += 12; // Move along x-axis at 12 pixels each iteration...
    if (x > (5 * 12)) { // ...until there are five in a row...
      x = 12; // ... then reset the x position...
      y = y - 12; // ... and move to the next row up
    }
    dot += 1; // Next dot
  }  
  
  //Digital clock
  if(clock_is_24h_style() == true) {
    strftime(s_hour_buffer, sizeof("00"), "%H", t); // Write time in 24 hour format into buffer
  } else {
    strftime(s_hour_buffer, sizeof("00"), "%I", t); // Write time in 12 hour format into buffer
  }
  strftime(s_min_buffer, sizeof("00"), "%M", t); // Write time in 24 hour format into buffer
  
  //Write time buffers to labels
  text_layer_set_text(s_hour_label, s_hour_buffer);
  text_layer_set_text(s_min_label, s_min_buffer); 

  //Date
  //Write and display dates
  strftime(s_month_buffer, sizeof(s_month_buffer), "%m", t);
  strftime(s_day_buffer, sizeof(s_day_buffer), "%d", t);
  text_layer_set_text(s_month_label, s_month_buffer);
  text_layer_set_text(s_day_label, s_day_buffer);
}

// Handle battery updates
static void update_battery(Layer *layer, GContext *ctx) {
  
  GRect bounds = layer_get_bounds(layer); // Set bounds to full window
  
  // Get the battery percent in a range of 0-10
  int bat = battery_state_service_peek().charge_percent / 10;
  
  // Reset dot count and position for battery
  dot = 0;
  x = 77;
  y = 156;
  
  //Battery dots
  #ifdef PBL_COLOR 
    graphics_context_set_fill_color(ctx, GColorYellow);
  #else
    graphics_context_set_fill_color(ctx, GColorWhite);
  #endif
  while (dot < bat) { // Loop while the dot count is lower than the battery percent
    graphics_fill_circle(ctx, GPoint(x,y), 4);
    y = y - 12; // Move up a row
    dot += 1; // Next dot
  }
  // Handles 10%
  if (bat == 0) {
    graphics_fill_circle(ctx, GPoint(77, 156), 4);
  }
}

// Handle bluetooth updates
void bt_handler(bool connected) {
  if (connected) {
    vibes_short_pulse();
    bt = true;
  }
  else {
    vibes_double_pulse();
    bt = false;
  }
}

// Update time shown on screen
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(window_get_root_layer(s_main_window));
}

// Loads the layers onto the main window
static void main_window_load(Window *window) {
  
  // Creates window_layer as root and sets its bounds
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Create background the layers
  s_solid_layer = layer_create(bounds);
  s_time_layer = layer_create(bounds);
  s_battery_layer = layer_create(bounds);
  
  // Update background layers
  layer_set_update_proc(s_solid_layer, update_bg);
  layer_set_update_proc(s_time_layer, update_time);
  layer_set_update_proc(s_battery_layer, update_battery);
  
  // Create the label
  s_month_label = text_layer_create(GRect(80,100,72,40));
  s_day_label = text_layer_create(GRect(80,130,72,40));
  s_hour_label = text_layer_create(GRect(80,100,72,40));
  s_min_label = text_layer_create(GRect(80,130,72,40));
  s_state_top_label = text_layer_create(GRect(80,100,72,40));
  s_state_bottom_label = text_layer_create(GRect(80,130,72,40));
  
  //Set font
  text_layer_set_font(s_month_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_font(s_hour_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_font(s_min_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_font(s_state_top_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  text_layer_set_font(s_state_bottom_label, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  
  // Set background and text colors
  #ifdef PBL_COLOR 
    text_layer_set_background_color(s_month_label, GColorVividCerulean);
    text_layer_set_background_color(s_day_label, GColorVividCerulean);
    text_layer_set_background_color(s_hour_label, GColorVividCerulean);
    text_layer_set_background_color(s_min_label, GColorVividCerulean);
    text_layer_set_background_color(s_state_top_label, GColorVividCerulean);
    text_layer_set_background_color(s_state_bottom_label, GColorVividCerulean);
  #else
    text_layer_set_background_color(s_month_label, GColorBlack);
    text_layer_set_background_color(s_day_label, GColorBlack);
    text_layer_set_background_color(s_hour_label, GColorBlack);
    text_layer_set_background_color(s_min_label, GColorBlack);
    text_layer_set_background_color(s_state_top_label, GColorBlack);
    text_layer_set_background_color(s_state_bottom_label, GColorBlack);
  #endif
  text_layer_set_text_color(s_month_label, GColorWhite);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_text_color(s_hour_label, GColorWhite);
  text_layer_set_text_color(s_min_label, GColorWhite);
  text_layer_set_text_color(s_state_top_label, GColorWhite);
  text_layer_set_text_color(s_state_bottom_label, GColorWhite);
  
  // Avoid blank screen in case updating time fails
  text_layer_set_text(s_month_label, "DA");
  text_layer_set_text(s_day_label, "TE");
  text_layer_set_text(s_hour_label, "TI");
  text_layer_set_text(s_min_label, "ME");
  text_layer_set_text(s_state_top_label, "DA");
  text_layer_set_text(s_state_bottom_label, "TE");
  
  // Align text
  text_layer_set_text_alignment(s_month_label, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_day_label, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_hour_label, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_min_label, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_state_top_label, GTextAlignmentCenter);
  text_layer_set_text_alignment(s_state_bottom_label, GTextAlignmentCenter);
  
  // Apply layers to screen
  layer_add_child(window_layer, s_solid_layer); 
  layer_add_child(window_layer, text_layer_get_layer(s_hour_label));
  layer_add_child(window_layer, text_layer_get_layer(s_min_label));
  layer_add_child(window_layer, text_layer_get_layer(s_month_label));
  layer_add_child(window_layer, text_layer_get_layer(s_day_label));
  layer_add_child(window_layer, text_layer_get_layer(s_state_top_label));
  layer_add_child(window_layer, text_layer_get_layer(s_state_bottom_label));
  layer_add_child(window_layer, s_time_layer);
  layer_add_child(window_layer, s_battery_layer);
}

// Hide/Unhide labels when called
static void timer_callback(void *data) {
  
  // Checks if watch was already shaken (shook!?)
  if (shake == true) {
    shake = false;
  }
  else if (shake == false) {
  // First. Bluetooth check.
    if (state == 0) {
      text_layer_set_text(s_state_top_label, "DA");
      text_layer_set_text(s_state_bottom_label, "TE");
      layer_set_hidden((Layer *)s_month_label, false);
      layer_set_hidden((Layer *)s_day_label, false);
    
      state = 1;
    
      app_timer_register(1 * 1000, timer_callback, NULL);
    }
  
    // Display the date
    else if (state == 1) {
      layer_set_hidden((Layer *)s_state_top_label, true);
      layer_set_hidden((Layer *)s_state_bottom_label, true);
    
      state = 2;
    
      app_timer_register(2 * 1000, timer_callback, NULL);
    }
  
    // Display "TIME"
    else if (state == 2) {
      text_layer_set_text(s_state_top_label, "TI");
      text_layer_set_text(s_state_bottom_label, "ME");
      layer_set_hidden((Layer *)s_state_top_label, false);
      layer_set_hidden((Layer *)s_state_bottom_label, false);
      layer_set_hidden((Layer *)s_month_label, true);
      layer_set_hidden((Layer *)s_day_label, true);
    
      state = 3;
    
      app_timer_register(1 * 1000, timer_callback, NULL);
    }
  
    // Display the time
    else if (state == 3) {
      layer_set_hidden((Layer *)s_state_top_label, true);
      layer_set_hidden((Layer *)s_state_bottom_label, true);
    
      state = 1;
    }
  }
}

//Control the shake gesture
static void tap_handler(AccelAxisType axis, int32_t direction) {
  
  // Checks if watch was already shaken (SHOOK?!)
  if (shake == false) {
    shake = true;
    
    app_timer_register(10 * 1000, timer_callback, NULL);
  }
  else if (shake == true) {
    shake = false;
    // Show "DATE"
    if (state == 1) {
      if (bt == true) {
        text_layer_set_text(s_state_top_label, "DA");
        text_layer_set_text(s_state_bottom_label, "TE");
        layer_set_hidden((Layer *)s_month_label, false);
        layer_set_hidden((Layer *)s_day_label, false);
        layer_set_hidden((Layer *)s_state_top_label, false);
        layer_set_hidden((Layer *)s_state_bottom_label, false);
      
        app_timer_register(1 * 1000, timer_callback, NULL);
      
      }
      else if (bt == false) {
        text_layer_set_text(s_state_top_label, "NO");
        text_layer_set_text(s_state_bottom_label, "BT");
        layer_set_hidden((Layer *)s_state_top_label, false);
        layer_set_hidden((Layer *)s_state_bottom_label, false);
      
        state = 0;
      
        app_timer_register(1 * 1000, timer_callback, NULL);
      }
    }
  }
}

// Unloads the layers on the main window
static void main_window_unload(Window *window) {
  
  // Destroy the background layers
  layer_destroy(s_solid_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_battery_layer);
  
  // Destroy the labels
  text_layer_destroy(s_month_label);
  text_layer_destroy(s_day_label);
  text_layer_destroy(s_hour_label);
  text_layer_destroy(s_min_label);
  text_layer_destroy(s_state_top_label);
  text_layer_destroy(s_state_bottom_label);
}
  
// Initializes the main window
static void init() {
  s_main_window = window_create(); // Create the main window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true); // Show the main window. Animations = true.
  
  //Check bluetooth status when the app starts
  if (bluetooth_connection_service_peek()) {
    bt = true;
  } 
  else {
    bt = false;
  }
  
  // Hide labels immediately
  layer_set_hidden((Layer *)s_month_label, true);
  layer_set_hidden((Layer *)s_day_label, true);
  layer_set_hidden((Layer *)s_state_top_label, true);
  layer_set_hidden((Layer *)s_state_bottom_label, true);
  
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler); // Update time every minute
  accel_tap_service_subscribe(tap_handler); // Registers shake gestures
  bluetooth_connection_service_subscribe(bt_handler); // Registers bluetooth status
}

// Deinitializes the main window
static void deinit() {
  window_destroy(s_main_window); // Destroy the main window
  accel_tap_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
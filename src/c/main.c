#include <pebble.h>
#include <stdlib.h>
#include <string.h>
#include "message_keys.auto.h"

#if defined(PBL_PLATFORM_GABBRO)
  #define FONT_TITLE   FONT_KEY_GOTHIC_24_BOLD
  #define FONT_BODY    FONT_KEY_GOTHIC_24
  #define FONT_NAME    FONT_KEY_GOTHIC_24_BOLD
  #define FONT_ROUTE_NAME FONT_KEY_GOTHIC_24_BOLD
  #define FONT_STOP_NAME FONT_KEY_GOTHIC_18_BOLD
  #define FONT_STOP_DIST FONT_KEY_GOTHIC_14_BOLD
  #define TITLE_HEIGHT 26
  #define PREV_HEIGHT  22
  #define NAME_HEIGHT  24
  #define NEXT_HEIGHT  22
  #define TEXT_VISUAL_OFFSET 4
#elif defined(PBL_PLATFORM_EMERY)
  #define FONT_TITLE   FONT_KEY_GOTHIC_18_BOLD
  #define FONT_BODY    FONT_KEY_GOTHIC_24
  #define FONT_NAME    FONT_KEY_GOTHIC_24_BOLD
  #define FONT_ROUTE_NAME FONT_KEY_GOTHIC_24_BOLD
  #define FONT_STOP_NAME FONT_KEY_GOTHIC_18_BOLD
  #define FONT_STOP_DIST FONT_KEY_GOTHIC_14_BOLD
  #define TITLE_HEIGHT 20
  #define PREV_HEIGHT  22
  #define NAME_HEIGHT  24
  #define NEXT_HEIGHT  22
  #define TEXT_VISUAL_OFFSET 4
#else
  #define FONT_TITLE   FONT_KEY_GOTHIC_14_BOLD
  #define FONT_BODY    FONT_KEY_GOTHIC_18
  #define FONT_NAME    FONT_KEY_GOTHIC_18_BOLD
  #define FONT_ROUTE_NAME FONT_KEY_GOTHIC_18_BOLD
  #define FONT_STOP_NAME FONT_KEY_GOTHIC_14_BOLD
  #define FONT_STOP_DIST FONT_KEY_GOTHIC_14_BOLD
  #define TITLE_HEIGHT 18
  #define PREV_HEIGHT  20
  #define NAME_HEIGHT  22
  #define NEXT_HEIGHT  20
  #define TEXT_VISUAL_OFFSET 3
#endif

#define AGENCY_BAR_TEXT_COLOR GColorWhite
#define AGENCY_BAR_BG_COLOR   GColorBlack
#define LEFT_MARGIN 4
#define HEART_RIGHT_MARGIN 6

#if defined(PBL_BW)
  #define HAVE_ERROR_LAYER 0
#else
  #define HAVE_ERROR_LAYER 1
#endif

#if defined(PBL_PLATFORM_GABBRO)
  #define ROUTE_RES    RESOURCE_ID_ROUTE_50
#else
  #define ROUTE_RES    RESOURCE_ID_ROUTE_25
#endif

#if defined(PBL_PLATFORM_EMERY)
  #define BELL_NOTIF_RES    RESOURCE_ID_BELL_80
#else
  #define BELL_NOTIF_RES    RESOURCE_ID_BELL_50
#endif

#define AGENCY_BAR_H 29
#define ROUTE_ROWS_VISIBLE 4
#define ROUTE_LAYER_COUNT  (ROUTE_ROWS_VISIBLE + 2)
#define MAX_PALETTE 32
#define LINE_BUF_SIZE 256
#define ROUTE_INITIAL_CAPACITY 16

#define ROUTE_SCROLL_TICK_MS      35
#define ROUTE_SCROLL_STEP         1
#define ROUTE_SCROLL_PAUSE_TICKS  30
#define ROUTE_SCROLL_ANIM_DURATION_MS 200
#define MARQUEE_IDLE_TIMEOUT_MS 60000
#define MARQUEE_RESUME_DELAY_MS 600
#define HOLD_INITIAL_REPEAT_MS   400
#define HOLD_REPEAT_MS           150
#define ALPHABET_TRIGGER_MS     5000
#define ALPHABET_ADVANCE_MS      250
#define SAVE_VIEW_TIMEOUT_MS    10000
#define ROUTE_FILL_DURATION_MS   220
#define RESTORE_MAX_RETRIES         5
#define RESTORE_RETRY_INTERVAL_MS   2000

#define STOPS_TARGET_RADIUS      8
#define STOPS_TARGET_DOT         4
#define STOPS_TARGET_COL_W       (AGENCY_BAR_H)
#define STOPS_SCROLL_TICK_MS     35
#define STOPS_SCROLL_STEP        1
#define STOPS_SCROLL_PAUSE_TICKS 30
#define STOPS_SCROLL_ANIM_DURATION_MS 200
#define STOP_ICON_SIZE           14

#define SI_HEADER_H              46
#define SI_ROW_H                 38
#define SI_LEFT_PAD              4

#define SI_REFRESH_ACTIVE_S      15
#define SI_REFRESH_IDLE_S        60
#define SI_REFRESH_WATCHDOG_MS   20000
#define SI_ACTIVITY_FLOOR_S      5
#define SI_REFRESH_FAIL_MAX      4

/* Set to 1 to show the bell + pin buttons in the stop-info header.
   Set to 0 to hide them and let the header text use the full width. */
#define SI_SHOW_ICONS 0

/* Set to 1 to keep the backlight on permanently and freeze all marquee /
   scroll animations. Useful for taking clean screenshots — no moving text
   and no ellipsis on clipped names (the overflow mode stays Fill, so the
   visible portion is just the beginning of the string). */
#define SCREENSHOT_MODE 0

#define PIN_KIND_STOP    1
#define PIN_KIND_DEPART  2

#define PERSIST_KEY_HAS_AGENCY    1
#define PERSIST_KEY_AGENCY_NAME   2
#define PERSIST_KEY_AGENCY_DOMAIN 3
#define PERSIST_KEY_AGENCY_APIKEY 4
#define PERSIST_KEY_AGENCY_COLOR  5
#define PERSIST_KEY_FAVORITES     6
#define PERSIST_KEY_STOP_SETTINGS 8

#define CMD_INIT            0
#define CMD_UP              1
#define CMD_DOWN            2
#define CMD_JUMP_TO_LETTER  3
#define CMD_JUMP_TO_DOMAIN  4

#define MAX_STOPS              128
#define STOP_NAME_LEN          64
#define COORD_SCALE            10000000
#define MAX_FAVORITES          32

#define SI_FOCUS_LIST   0
#define SI_FOCUS_BELL   1
#define SI_FOCUS_PIN    2
#define SI_MAX_BUSES             32

#define MAX_STOP_SETTINGS        32
#define STOP_SETTING_BELL        0x01
#define STOP_SETTING_PIN         0x02

#define NOTIF_OPTION_COUNT            6
#define NOTIF_ROW_H                   32
#define NOTIF_SCROLL_ANIM_DURATION_MS 180
#define NOTIF_TEXT_INSET_Y            5

typedef enum {
  SCROLL_PAUSE_START,
  SCROLL_FORWARD,
  SCROLL_PAUSE_END,
  SCROLL_BACK
} ScrollState;

typedef struct AutoScrollTextLayer {
  Layer *layer;
  char text[64];
  GFont font;
  GColor text_color;
  GColor background_color;
  int16_t text_width;
  int16_t text_height;
  int16_t scroll_offset;
  int16_t max_scroll;
  bool left_align_when_fits;
  ScrollState state;
  AppTimer *scroll_timer;
} AutoScrollTextLayer;

typedef struct {
  char name[40];
  uint32_t color;
  int32_t  route_id;
  int16_t original_index;
  int16_t max_scroll;
  int16_t scroll_offset;
  int16_t text_h;
  uint8_t is_favorite;
  int8_t  scroll_direction;
  int16_t pause_ticks;
  int32_t fill;
  int32_t fill_from;
  int32_t fill_to;
} RouteEntry;

typedef struct {
  char name[STOP_NAME_LEN];
  int32_t lat;
  int32_t lon;
  int32_t stop_id;
} StopEntry;

typedef struct {
  Window      *window;
  Layer       *top_bar_layer;
  TextLayer   *clock_layer;
  Layer       *status_icon_layer;
  Layer       *no_location_layer;
  Layer       *battery_layer;
  Layer       *route_bar_layer;
  AutoScrollTextLayer *route_name_auto;
  Layer       *list_layer;
  Layer       *text_layer;
  AppTimer    *fetch_location_timer;
  AppTimer    *location_timeout_timer;
} StopsView;

typedef struct {
  int32_t seconds;
  int8_t  capacity_pct;
  char    name[40];
} BusEntry;

typedef struct {
  int32_t stop_id;
  uint8_t flags;
  uint8_t bell_minutes;
} StopSetting;

typedef struct {
  Window    *window;
  Layer     *top_bar_layer;
  TextLayer *clock_layer;
  Layer     *status_icon_layer;
  Layer     *battery_layer;
  Layer     *header_layer;
  Layer     *header_text_layer;
  Layer     *list_layer;
  Layer     *text_layer;
  AppTimer  *fetch_timer;
} StopInfoView;

typedef struct {
  Window    *window;
  Layer     *top_bar_layer;
  TextLayer *clock_layer;
  Layer     *status_icon_layer;
  Layer     *battery_layer;
  Layer     *left_layer;
  Layer     *list_layer;
  Layer     *hl_layer;
} NotifView;

typedef struct {
  bool loading, appmsg_ok, is_initial_load;
  bool marquee_idle, has_saved_agency, showing_saved_view;
  bool no_location_slash_visible;
  bool pending_save_view, pending_selection_view;
  bool hold_active, in_alphabet_mode;
  bool routes_loaded, settings_selected;
  bool have_location, stops_loaded;
  bool location_pending;
  bool status_loading, spinner_pending_stop;
  bool parsing_palette;
  bool routes_load_failed;

  uint8_t last_command, last_letter, restore_retry_count;
  uint8_t held_button;
  uint8_t route_row_h, route_rows_visible;
  uint8_t route_text_x, route_text_w;
  uint8_t favorite_count, stop_count, stop_capacity;
  uint16_t stops_line_len;
  int32_t  stops_chunks_received;
  int32_t  stops_chunks_total;
  uint8_t route_count, route_capacity;
  uint8_t route_selected, route_first_visible;
  uint8_t line_len, chunks_received, chunks_total, palette_count;
  uint8_t battery_charge, spinner_frames;

  uint8_t stop_selected;
  uint8_t stop_first_visible;
  uint8_t stop_row_h;
  uint8_t stop_rows_visible;
  int16_t stop_display_anchor;
  int16_t stop_pending_first_visible;
  int16_t stop_text_x;
  int16_t stop_text_w;

  int16_t stop_scroll_offsets[MAX_STOPS];
  int16_t stop_max_scrolls[MAX_STOPS];
  int8_t  stop_scroll_dirs[MAX_STOPS];
  uint8_t stop_pause_ticks[MAX_STOPS];

  int16_t stop_sub_scroll_offsets[MAX_STOPS];
  int16_t stop_sub_max_scrolls[MAX_STOPS];
  int8_t  stop_sub_scroll_dirs[MAX_STOPS];
  uint8_t stop_sub_pause_ticks[MAX_STOPS];
  char    stop_sub_texts[MAX_STOPS][24];
  int16_t stops_list_scroll_y;
  int16_t stops_list_scroll_target;

  StopInfoView *si;
  bool si_loading, si_loaded;
  uint8_t si_stop_display_idx;
  uint8_t si_stop_array_idx;
  uint8_t si_bus_count;
  uint8_t si_bus_capacity;
  int16_t si_scroll_y;
  uint8_t si_focus;
  uint8_t si_selected;
  int16_t si_text_w;

  int16_t si_hdr_text_w;
  int16_t si_hdr_name_scroll;
  int16_t si_hdr_name_max;
  int8_t  si_hdr_name_dir;
  uint8_t si_hdr_name_pause;
  int16_t si_hdr_dist_scroll;
  int16_t si_hdr_dist_max;
  int8_t  si_hdr_dist_dir;
  uint8_t si_hdr_dist_pause;

  int16_t si_scroll_offsets[SI_MAX_BUSES];
  int16_t si_max_scrolls[SI_MAX_BUSES];
  int8_t  si_scroll_dirs[SI_MAX_BUSES];
  uint8_t si_pause_ticks[SI_MAX_BUSES];

  int16_t si_sub_scroll_offsets[SI_MAX_BUSES];
  int16_t si_sub_max_scrolls[SI_MAX_BUSES];
  int8_t  si_sub_scroll_dirs[SI_MAX_BUSES];
  uint8_t si_sub_pause_ticks[SI_MAX_BUSES];

  AppTimer *si_scroll_timer;
  AppTimer *si_refresh_timer;
  AppTimer *si_refresh_watchdog;
  bool      si_fetch_in_flight;
  uint8_t   si_refresh_failures;
  time_t    si_last_fetch_time;

  int16_t si_row_h;
  uint8_t si_rows_visible;
  int32_t si_route_id;
  int32_t si_stop_id;
  int32_t si_stop_distance_ft;
  char    si_stop_name[STOP_NAME_LEN];
  char    si_line_buf[128];
  uint8_t si_line_len;
  uint8_t si_chunks_received;
  uint8_t si_chunks_total;
  BusEntry *si_buses;

  NotifView *nv;
  uint8_t    notif_selected;
  int16_t    notif_scroll_y;
  int16_t    notif_scroll_target;
  uint8_t    notif_pending_selected;
  Animation *notif_scroll_anim;

  int8_t alphabet_letter;
  int16_t title_x_orig, up_arrow_x_orig, prev_x_orig, name_x_orig;
  int16_t next_x_orig, down_arrow_x_orig, domain_x_orig, saved_name_x_orig;
  int16_t agency_bar_x_orig, settings_icon_x_orig, route_list_x_orig;
  int16_t route_full_w, route_square_w;
  int16_t route_list_scroll_y, route_list_scroll_target;
  int16_t route_pending_first_visible;
  int16_t spinner_angle;
  int32_t my_lat, my_lon;

  AppTimer *retry_timer, *restore_retry_timer, *marquee_idle_timer;
  AppTimer *no_location_flash_timer;
  AppTimer *save_view_timeout_timer, *hold_timer, *alphabet_trigger;
  AppTimer *route_scroll_timer, *spinner_timer, *stops_scroll_timer;
  AppTimer *routes_push_timer;
  Animation *route_fill_anim, *route_list_scroll_anim;
  Animation *stops_list_scroll_anim;

  Window *main_window;
  Window *routes_window;
  Layer *top_bar_layer, *status_icon_layer, *battery_layer;
  Layer *routes_top_bar_layer, *routes_status_icon_layer, *routes_battery_layer;
  TextLayer *routes_clock_layer;
  Layer *up_arrow_graphics, *down_arrow_graphics;
  Layer *agency_bar_layer, *settings_icon_layer, *route_list_layer;
  TextLayer *clock_layer, *title_layer;
  GBitmap *route_bitmap, *settings_gear_bitmap;
  GBitmap *bell_25_bitmap, *pin_25_bitmap, *bus_25_bitmap;
  GBitmap *bell_80_bitmap;

  Layer **route_text_layers, **route_inv_layers;

  int32_t *favorites;
  StopSetting stop_settings[MAX_STOP_SETTINGS];
  uint8_t stop_settings_count;
  StopEntry *stops;
  int32_t *stop_distances;
  RouteEntry *routes;
  char *line_buf;
  uint32_t *palette;
  StopsView *sv;
  AutoScrollTextLayer *prev, *name, *next, *domain, *saved_name_auto;

  GColor primary_color;

  char last_domain[64];
  char saved_name[40];
  char saved_domain[64];
} AppState;

static AppState *g = NULL;

static const uint8_t NOTIF_OPTIONS[NOTIF_OPTION_COUNT] = { 5, 10, 15, 20, 25, 30 };

#define s_loading                 (g->loading)
#define s_appmsg_ok               (g->appmsg_ok)
#define s_is_initial_load         (g->is_initial_load)
#define s_marquee_idle            (g->marquee_idle)
#define s_has_saved_agency        (g->has_saved_agency)
#define s_showing_saved_view      (g->showing_saved_view)
#define s_pending_save_view       (g->pending_save_view)
#define s_pending_selection_view  (g->pending_selection_view)
#define s_hold_active             (g->hold_active)
#define s_in_alphabet_mode        (g->in_alphabet_mode)
#define s_routes_loaded           (g->routes_loaded)
#define s_routes_load_failed      (g->routes_load_failed)
#define s_settings_selected       (g->settings_selected)
#define s_have_location           (g->have_location)
#define s_stops_loaded            (g->stops_loaded)
#define s_location_pending        (g->location_pending)
#define s_status_loading          (g->status_loading)
#define s_spinner_pending_stop    (g->spinner_pending_stop)
#define s_parsing_palette         (g->parsing_palette)
#define s_last_command            (g->last_command)
#define s_last_letter             (g->last_letter)
#define s_restore_retry_count     (g->restore_retry_count)
#define s_held_button             (g->held_button)
#define s_route_row_h             (g->route_row_h)
#define s_route_rows_visible      (g->route_rows_visible)
#define s_route_text_x            (g->route_text_x)
#define s_route_text_w            (g->route_text_w)
#define s_favorite_count          (g->favorite_count)
#define s_stop_count              (g->stop_count)
#define s_stop_capacity           (g->stop_capacity)
#define s_stops_line_len          (g->stops_line_len)
#define s_stops_chunks_received   (g->stops_chunks_received)
#define s_stops_chunks_total      (g->stops_chunks_total)
#define s_route_count             (g->route_count)
#define s_route_capacity          (g->route_capacity)
#define s_route_selected          (g->route_selected)
#define s_route_first_visible     (g->route_first_visible)
#define s_line_len                (g->line_len)
#define s_chunks_received         (g->chunks_received)
#define s_chunks_total            (g->chunks_total)
#define s_palette_count           (g->palette_count)
#define s_battery_charge          (g->battery_charge)
#define s_spinner_frames          (g->spinner_frames)
#define s_alphabet_letter         (g->alphabet_letter)
#define s_title_x_orig            (g->title_x_orig)
#define s_up_arrow_x_orig         (g->up_arrow_x_orig)
#define s_prev_x_orig             (g->prev_x_orig)
#define s_name_x_orig             (g->name_x_orig)
#define s_next_x_orig             (g->next_x_orig)
#define s_down_arrow_x_orig       (g->down_arrow_x_orig)
#define s_domain_x_orig           (g->domain_x_orig)
#define s_saved_name_x_orig       (g->saved_name_x_orig)
#define s_agency_bar_x_orig       (g->agency_bar_x_orig)
#define s_settings_icon_x_orig    (g->settings_icon_x_orig)
#define s_route_list_x_orig       (g->route_list_x_orig)
#define s_route_full_w            (g->route_full_w)
#define s_route_square_w          (g->route_square_w)
#define s_route_list_scroll_y     (g->route_list_scroll_y)
#define s_route_list_scroll_target (g->route_list_scroll_target)
#define s_route_pending_first_visible (g->route_pending_first_visible)
#define s_spinner_angle           (g->spinner_angle)
#define s_my_lat                  (g->my_lat)
#define s_my_lon                  (g->my_lon)
#define s_stop_selected           (g->stop_selected)
#define s_stop_first_visible      (g->stop_first_visible)
#define s_stop_row_h              (g->stop_row_h)
#define s_stop_rows_visible       (g->stop_rows_visible)
#define s_stop_display_anchor     (g->stop_display_anchor)
#define s_stop_pending_first_visible (g->stop_pending_first_visible)
#define s_stop_text_x             (g->stop_text_x)
#define s_stop_text_w             (g->stop_text_w)
#define s_stop_scroll_offsets     (g->stop_scroll_offsets)
#define s_stop_max_scrolls        (g->stop_max_scrolls)
#define s_stop_scroll_dirs        (g->stop_scroll_dirs)
#define s_stop_pause_ticks        (g->stop_pause_ticks)
#define s_stop_sub_scroll_offsets (g->stop_sub_scroll_offsets)
#define s_stop_sub_max_scrolls    (g->stop_sub_max_scrolls)
#define s_stop_sub_scroll_dirs    (g->stop_sub_scroll_dirs)
#define s_stop_sub_pause_ticks    (g->stop_sub_pause_ticks)
#define s_stop_sub_texts          (g->stop_sub_texts)
#define s_stops_list_scroll_y     (g->stops_list_scroll_y)
#define s_stops_list_scroll_target (g->stops_list_scroll_target)
#define s_retry_timer             (g->retry_timer)
#define s_restore_retry_timer     (g->restore_retry_timer)
#define s_marquee_idle_timer      (g->marquee_idle_timer)
#define s_save_view_timeout_timer (g->save_view_timeout_timer)
#define s_hold_timer              (g->hold_timer)
#define s_alphabet_trigger        (g->alphabet_trigger)
#define s_route_scroll_timer      (g->route_scroll_timer)
#define s_spinner_timer           (g->spinner_timer)
#define s_stops_scroll_timer      (g->stops_scroll_timer)
#define s_routes_push_timer       (g->routes_push_timer)
#define s_route_fill_anim         (g->route_fill_anim)
#define s_route_list_scroll_anim  (g->route_list_scroll_anim)
#define s_stops_list_scroll_anim  (g->stops_list_scroll_anim)
#define s_main_window             (g->main_window)
#define s_routes_window           (g->routes_window)
#define s_top_bar_layer           (g->top_bar_layer)
#define s_status_icon_layer       (g->status_icon_layer)
#define s_battery_layer           (g->battery_layer)
#define s_routes_top_bar_layer    (g->routes_top_bar_layer)
#define s_routes_status_icon_layer (g->routes_status_icon_layer)
#define s_routes_battery_layer    (g->routes_battery_layer)
#define s_routes_clock_layer      (g->routes_clock_layer)
#define s_up_arrow_graphics       (g->up_arrow_graphics)
#define s_down_arrow_graphics     (g->down_arrow_graphics)
#define s_agency_bar_layer        (g->agency_bar_layer)
#define s_settings_icon_layer     (g->settings_icon_layer)
#define s_route_list_layer        (g->route_list_layer)
#define s_clock_layer             (g->clock_layer)
#define s_title_layer             (g->title_layer)
#define s_route_bitmap            (g->route_bitmap)
#define s_bell_25_bitmap          (g->bell_25_bitmap)
#define s_pin_25_bitmap           (g->pin_25_bitmap)
#define s_bus_25_bitmap           (g->bus_25_bitmap)
#define s_bell_80_bitmap          (g->bell_80_bitmap)
#define s_settings_gear_bitmap    (g->settings_gear_bitmap)
#define s_route_text_layers       (g->route_text_layers)
#define s_route_inv_layers        (g->route_inv_layers)
#define s_favorites               (g->favorites)
#define s_stop_settings           (g->stop_settings)
#define s_stop_settings_count     (g->stop_settings_count)
#define s_stops                   (g->stops)
#define s_stop_distances          (g->stop_distances)
#define s_routes                  (g->routes)
#define s_line_buf                (g->line_buf)
#define s_palette                 (g->palette)
#define s_sv                      (g->sv)
#define s_prev                    (g->prev)
#define s_name                    (g->name)
#define s_next                    (g->next)
#define s_domain                  (g->domain)
#define s_saved_name_auto         (g->saved_name_auto)
#define s_primary_color           (g->primary_color)
#define s_last_domain             (g->last_domain)
#define s_saved_name              (g->saved_name)
#define s_saved_domain            (g->saved_domain)
#define s_no_location_slash_visible (g->no_location_slash_visible)
#define s_no_location_flash_timer   (g->no_location_flash_timer)

#define s_si                      (g->si)
#define s_si_loading              (g->si_loading)
#define s_si_loaded               (g->si_loaded)
#define s_si_stop_display_idx     (g->si_stop_display_idx)
#define s_si_stop_array_idx       (g->si_stop_array_idx)
#define s_si_bus_count            (g->si_bus_count)
#define s_si_bus_capacity         (g->si_bus_capacity)
#define s_si_scroll_y             (g->si_scroll_y)
#define s_si_focus                (g->si_focus)
#define s_si_row_h                (g->si_row_h)
#define s_si_rows_visible         (g->si_rows_visible)
#define s_si_route_id             (g->si_route_id)
#define s_si_stop_id              (g->si_stop_id)
#define s_si_stop_distance_ft     (g->si_stop_distance_ft)
#define s_si_stop_name            (g->si_stop_name)
#define s_si_line_buf             (g->si_line_buf)
#define s_si_line_len             (g->si_line_len)
#define s_si_chunks_received      (g->si_chunks_received)
#define s_si_chunks_total         (g->si_chunks_total)
#define s_si_buses                (g->si_buses)
#define s_si_selected             (g->si_selected)
#define s_si_text_w               (g->si_text_w)
#define s_si_scroll_offsets       (g->si_scroll_offsets)
#define s_si_max_scrolls          (g->si_max_scrolls)
#define s_si_scroll_dirs          (g->si_scroll_dirs)
#define s_si_pause_ticks          (g->si_pause_ticks)
#define s_si_sub_scroll_offsets   (g->si_sub_scroll_offsets)
#define s_si_sub_max_scrolls      (g->si_sub_max_scrolls)
#define s_si_sub_scroll_dirs      (g->si_sub_scroll_dirs)
#define s_si_sub_pause_ticks      (g->si_sub_pause_ticks)
#define s_si_scroll_timer         (g->si_scroll_timer)

#define s_si_refresh_timer        (g->si_refresh_timer)
#define s_si_refresh_watchdog     (g->si_refresh_watchdog)
#define s_si_fetch_in_flight      (g->si_fetch_in_flight)
#define s_si_refresh_failures     (g->si_refresh_failures)
#define s_si_last_fetch_time      (g->si_last_fetch_time)

#define s_si_hdr_text_w           (g->si_hdr_text_w)
#define s_si_hdr_name_scroll      (g->si_hdr_name_scroll)
#define s_si_hdr_name_max         (g->si_hdr_name_max)
#define s_si_hdr_name_dir         (g->si_hdr_name_dir)
#define s_si_hdr_name_pause       (g->si_hdr_name_pause)
#define s_si_hdr_dist_scroll      (g->si_hdr_dist_scroll)
#define s_si_hdr_dist_max         (g->si_hdr_dist_max)
#define s_si_hdr_dist_dir         (g->si_hdr_dist_dir)
#define s_si_hdr_dist_pause       (g->si_hdr_dist_pause)

#define s_nv                      (g->nv)
#define s_notif_selected          (g->notif_selected)
#define s_notif_scroll_y          (g->notif_scroll_y)
#define s_notif_scroll_target     (g->notif_scroll_target)
#define s_notif_pending_selected  (g->notif_pending_selected)
#define s_notif_scroll_anim       (g->notif_scroll_anim)

#if HAVE_ERROR_LAYER
static TextLayer *s_error_layer;
#endif

static void send_command(uint8_t command);
static void send_jump_command(uint8_t letter);
static void send_jump_to_domain_command(const char *domain);
static void send_fetch_config_request(void);
static void send_fetch_routes_request(void);
static void send_restore_request(void);
static void save_agency_config(const char *apikey, uint32_t color_val);
static void hold_repeat_cb(void *context);
static void route_hold_repeat_cb(void *context);
static void stops_hold_repeat_cb(void *context);
static void alphabet_trigger_cb(void *context);
static void save_view_timeout_cb(void *context);
static void begin_pending_save_view(void);
static void show_selection(void);
static void hide_selection(void);
static void push_routes_window(void);
static void update_route_list(void);
static void start_route_fill_anim(void);
static void handle_routes_chunk(int32_t idx, int32_t total, const char *data);
static void restore_retry_cb(void *context);
static void marquee_idle_cb(void *context);
static void reset_marquee_activity(void);
static void send_fetch_stops_request(int32_t route_id);
static void send_fetch_location_request(void *context);
static void handle_stops_chunk(int32_t idx, int32_t total, const char *data);
static void load_favorites(void);
static void save_favorites(void);
static void apply_favorites_and_sort(void);
static void toggle_favorite(int route_idx);
static void draw_heart(GContext *ctx, int cx, int cy, int size, GColor color, bool filled);
static void open_stops_window(int route_idx);
static void recompute_stop_layout(void);
static void stops_move_selection(int delta);
static void finish_hold(void);
static void route_move_selection(int delta);
static void dirty_all_route_text_layers(void);
static void update_route_inv_layers(void);
static void start_route_scroll_timer(void);
static void commit_pending_route_scroll(void);
static void start_route_scroll_anim(int new_fv);
static void set_route_layers_y_offset(int32_t y_offset);
static void sort_routes(void);
static void stops_scroll_tick(void *context);
static void start_stops_scroll_timer(void);
static void start_stops_scroll_anim(int delta_fv, int new_fv);
static void compute_stop_distances(void);
static void try_finish_stops_load(void);
static void update_clock(void);
static void open_stop_info_window(int display_idx);
static void si_select_handler(ClickRecognizerRef r, void *ctx);
static void si_click_config_provider(void *context);
static void si_handle_arrival_chunk(int32_t idx, int32_t total, const char *data);
static void si_scroll_tick(void *context);
static void start_si_scroll_timer(void);
static void si_move_selection(int delta);
static void recompute_si_layout(void);
static void si_text_update_proc(Layer *layer, GContext *ctx);
static void recompute_si_header_layout(void);
static void si_header_text_update_proc(Layer *layer, GContext *ctx);
static bool advance_scroll_one(int16_t *offset, int16_t *max,
                               int8_t *dir, uint8_t *pause);
static void load_stop_settings(void);
static bool stop_has_bell(int32_t stop_id);
static bool stop_has_pin(int32_t stop_id);
static void stop_toggle_flag(int32_t stop_id, uint8_t bit);

static void si_send_arrival_fetch(void);
static void si_schedule_refresh(void);
static void si_refresh_cb(void *context);
static void si_refresh_watchdog_cb(void *context);

static void open_notif_window(void);
static void commit_pending_notif_scroll(void);
static void start_notif_scroll_anim(int sel_delta);
static void notif_scroll_anim_update(Animation *anim, AnimationProgress progress);
static void notif_scroll_anim_teardown(Animation *anim);
static void notif_hl_layer_update_proc(Layer *layer, GContext *ctx);

static void vibrate_scroll(void) {
  static const uint32_t seg[] = { 30 };
  VibePattern pat = { .durations = seg, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static void vibrate_alphabet_letter(void) {
  static const uint32_t seg[] = { 60 };
  VibePattern pat = { .durations = seg, .num_segments = 1 };
  vibes_enqueue_custom_pattern(pat);
}

static GColor contrast_gcolor(uint32_t rgb) {
#if defined(PBL_BW)
  GColor q = GColorFromHEX(rgb);
  return (q.argb == GColorBlack.argb) ? GColorWhite : GColorBlack;
#else
  int r = (rgb >> 16) & 0xFF;
  int g = (rgb >> 8) & 0xFF;
  int b = rgb & 0xFF;
  int y = (r * 299 + g * 587 + b * 114) / 1000;
  return (y >= 128) ? GColorBlack : GColorWhite;
#endif
}

static GColor primary_text_color(void) {
  int cr = s_primary_color.r * 85;
  int cg = s_primary_color.g * 85;
  int cb = s_primary_color.b * 85;
  int y = (cr * 299 + cg * 587 + cb * 114) / 1000;
  return (y >= 128) ? GColorBlack : GColorWhite;
}

static GColor topbar_text_color(void) {
#if defined(PBL_BW)
  return GColorWhite;
#else
  int cr = s_primary_color.r * 85;
  int cg = s_primary_color.g * 85;
  int cb = s_primary_color.b * 85;
  int y = (cr * 299 + cg * 587 + cb * 114) / 1000;
  return (y >= 128) ? GColorBlack : GColorWhite;
#endif
}

static int64_t isqrt64(int64_t n) {
  if (n <= 0) return 0;
  int64_t x = n;
  int64_t y = (x + 1) >> 1;
  while (y < x) { x = y; y = (x + n / x) >> 1; }
  return x;
}

static void mark_status_icons_dirty(void) {
  if (s_status_icon_layer) layer_mark_dirty(s_status_icon_layer);
  if (s_routes_status_icon_layer) layer_mark_dirty(s_routes_status_icon_layer);
  if (s_sv && s_sv->status_icon_layer) layer_mark_dirty(s_sv->status_icon_layer);
  if (s_si && s_si->status_icon_layer) layer_mark_dirty(s_si->status_icon_layer);
  if (s_nv && s_nv->status_icon_layer) layer_mark_dirty(s_nv->status_icon_layer);
}

static void battery_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int w = bounds.size.w;
  int h = bounds.size.h;
  int body_w = w - 3;

  GColor fg = topbar_text_color();

  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 1);
  GRect body = GRect(0, 0, body_w - 1, h - 1);
  graphics_draw_rect(ctx, body);

  GRect nub = GRect(body_w - 1, h / 2 - 2, 3, 4);
  graphics_context_set_fill_color(ctx, fg);
  graphics_fill_rect(ctx, nub, 0, GCornerNone);

  int inner_w = body_w - 4;
  int inner_h = h - 4;
  int fill_w = (inner_w * s_battery_charge) / 100;
  if (fill_w > 0) {
    GRect fill = GRect(2, 2, fill_w, inner_h);
    graphics_context_set_fill_color(ctx, fg);
    graphics_fill_rect(ctx, fill, 0, GCornerNone);
  }
}

static void battery_state_handler(BatteryChargeState state) {
  s_battery_charge = state.charge_percent;
  if (s_battery_layer) layer_mark_dirty(s_battery_layer);
  if (s_routes_battery_layer) layer_mark_dirty(s_routes_battery_layer);
  if (s_sv && s_sv->battery_layer) layer_mark_dirty(s_sv->battery_layer);
  if (s_si && s_si->battery_layer) layer_mark_dirty(s_si->battery_layer);
  if (s_nv && s_nv->battery_layer) layer_mark_dirty(s_nv->battery_layer);
}

static void status_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2;
  graphics_context_set_stroke_color(ctx, topbar_text_color());
  graphics_context_set_stroke_width(ctx, 2);

  if (s_status_loading) {
    int radius = 6;
    GRect arc_rect = GRect(cx - radius, cy - radius, radius * 2, radius * 2);
    int32_t start = DEG_TO_TRIGANGLE(s_spinner_angle);
    int32_t end   = start + DEG_TO_TRIGANGLE(270);
    graphics_draw_arc(ctx, arc_rect, GOvalScaleModeFitCircle, start, end);
  } else {
    GPoint p1 = GPoint(cx - 6, cy);
    GPoint p2 = GPoint(cx - 2, cy + 4);
    GPoint p3 = GPoint(cx + 6, cy - 5);
    graphics_draw_line(ctx, p1, p2);
    graphics_draw_line(ctx, p2, p3);
  }
}

#define NO_LOCATION_FLASH_MS 500

static void no_location_flash_cb(void *context) {
  s_no_location_flash_timer = NULL;
  if (!s_sv || !s_sv->no_location_layer) return;
  if (layer_get_hidden(s_sv->no_location_layer)) return;

  if (s_marquee_idle) {
    s_no_location_slash_visible = true;
    layer_mark_dirty(s_sv->no_location_layer);
    return;
  }

  s_no_location_slash_visible = !s_no_location_slash_visible;
  layer_mark_dirty(s_sv->no_location_layer);
  s_no_location_flash_timer = app_timer_register(NO_LOCATION_FLASH_MS,
                                                 no_location_flash_cb, NULL);
}

static void no_location_flash_sync(void) {
  if (!s_sv || !s_sv->no_location_layer) return;
  bool visible = !layer_get_hidden(s_sv->no_location_layer);

  if (!visible) {
    if (s_no_location_flash_timer) {
      app_timer_cancel(s_no_location_flash_timer);
      s_no_location_flash_timer = NULL;
    }
    return;
  }

  if (s_marquee_idle) {
    if (s_no_location_flash_timer) {
      app_timer_cancel(s_no_location_flash_timer);
      s_no_location_flash_timer = NULL;
    }
    if (!s_no_location_slash_visible) {
      s_no_location_slash_visible = true;
      layer_mark_dirty(s_sv->no_location_layer);
    }
    return;
  }

  if (!s_no_location_flash_timer) {
    s_no_location_flash_timer = app_timer_register(NO_LOCATION_FLASH_MS,
                                                   no_location_flash_cb, NULL);
  }
}

static void no_location_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2;

  graphics_context_set_stroke_color(ctx, topbar_text_color());
  graphics_context_set_stroke_width(ctx, 2);

  const int r       = 4;
  const int head_cy = cy - 3;
  const int tip_y   = cy + 6;

  graphics_draw_circle(ctx, GPoint(cx, head_cy), r);

  GPoint tl  = GPoint(cx - 3, head_cy + 3);
  GPoint tr  = GPoint(cx + 3, head_cy + 3);
  GPoint tip = GPoint(cx, tip_y);
  graphics_draw_line(ctx, tl, tip);
  graphics_draw_line(ctx, tr, tip);

  if (s_no_location_slash_visible) {
    graphics_draw_line(ctx, GPoint(cx - 5, cy - 6), GPoint(cx + 5, cy + 4));
  }

  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, GPoint(cx, head_cy), 2);
}

static void spinner_timer_callback(void *context) {
  s_spinner_angle = (s_spinner_angle + 30) % 360;
  s_spinner_frames++;

  if (s_spinner_pending_stop && s_spinner_frames >= 5) {
    s_status_loading = false;
    s_spinner_pending_stop = false;
    s_spinner_frames = 0;
    s_spinner_angle = 0;
    s_spinner_timer = NULL;
    mark_status_icons_dirty();
    return;
  }
  mark_status_icons_dirty();
  s_spinner_timer = app_timer_register(100, spinner_timer_callback, NULL);
}

static void status_icon_set_loading(bool loading) {
  if (loading) {
    s_spinner_frames = 0;
    s_spinner_pending_stop = false;
    s_status_loading = true;
    if (!s_spinner_timer) {
      s_spinner_timer = app_timer_register(100, spinner_timer_callback, NULL);
    }
  } else {
    if (s_spinner_timer) {
      s_spinner_pending_stop = true;
    } else {
      s_status_loading = false;
    }
  }
  mark_status_icons_dirty();
}

static void auto_scroll_timer_callback(void *context) {
  AutoScrollTextLayer *auto_layer = context;
#if SCREENSHOT_MODE
  auto_layer->scroll_timer = NULL;
  return;
#else
  int next_interval = 35;

  switch (auto_layer->state) {
    case SCROLL_PAUSE_START:
      if (s_marquee_idle && auto_layer->scroll_offset == 0) {
        auto_layer->scroll_timer = NULL;
        return;
      }
      auto_layer->state = SCROLL_FORWARD;
      next_interval = 35;
      break;
    case SCROLL_FORWARD:
      auto_layer->scroll_offset++;
      if (auto_layer->scroll_offset >= auto_layer->max_scroll) {
        auto_layer->scroll_offset = auto_layer->max_scroll;
        auto_layer->state = SCROLL_PAUSE_END;
        next_interval = s_marquee_idle ? 35 : 1200;
      }
      break;
    case SCROLL_PAUSE_END:
      auto_layer->state = SCROLL_BACK;
      next_interval = 35;
      break;
    case SCROLL_BACK:
      auto_layer->scroll_offset--;
      if (auto_layer->scroll_offset <= 0) {
        auto_layer->scroll_offset = 0;
        auto_layer->state = SCROLL_PAUSE_START;
        if (s_marquee_idle) {
          layer_mark_dirty(auto_layer->layer);
          auto_layer->scroll_timer = NULL;
          return;
        }
        next_interval = 1200;
      }
      break;
  }
  layer_mark_dirty(auto_layer->layer);
  auto_layer->scroll_timer = app_timer_register(next_interval,
                                                auto_scroll_timer_callback, auto_layer);
#endif
}

static void auto_scroll_text_layer_update_proc(Layer *layer, GContext *ctx) {
  AutoScrollTextLayer *auto_layer = *(AutoScrollTextLayer **)layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, auto_layer->background_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  graphics_context_set_text_color(ctx, auto_layer->text_color);
  int y = bounds.origin.y + (bounds.size.h - auto_layer->text_height) / 2
          - TEXT_VISUAL_OFFSET;
  GRect text_rect = GRect(bounds.origin.x - auto_layer->scroll_offset,
                          y, auto_layer->text_width, auto_layer->text_height);
  graphics_draw_text(ctx, auto_layer->text, auto_layer->font, text_rect,
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void auto_scroll_text_layer_set_text(AutoScrollTextLayer *auto_layer, const char *text) {
  if (!auto_layer) return;
  strncpy(auto_layer->text, text, sizeof(auto_layer->text) - 1);
  auto_layer->text[sizeof(auto_layer->text) - 1] = '\0';

  if (auto_layer->scroll_timer) {
    app_timer_cancel(auto_layer->scroll_timer);
    auto_layer->scroll_timer = NULL;
  }
  GSize text_size = graphics_text_layout_get_content_size(auto_layer->text,
                                                          auto_layer->font,
                                                          GRect(0, 0, 10000, 10000),
                                                          GTextOverflowModeWordWrap,
                                                          GTextAlignmentLeft);
  auto_layer->text_width  = (int16_t)text_size.w;
  auto_layer->text_height = (int16_t)text_size.h;

  int layer_width = layer_get_bounds(auto_layer->layer).size.w;
  auto_layer->max_scroll = auto_layer->text_width - layer_width;

  if (auto_layer->max_scroll > 0) {
    auto_layer->scroll_offset = 0;
    auto_layer->state = SCROLL_PAUSE_START;
#if SCREENSHOT_MODE
    auto_layer->scroll_timer = NULL;
#else
    auto_layer->scroll_timer = app_timer_register(MARQUEE_RESUME_DELAY_MS,
                                                  auto_scroll_timer_callback, auto_layer);
#endif
  } else {
    if (auto_layer->left_align_when_fits) {
      auto_layer->scroll_offset = 0;
    } else {
      auto_layer->scroll_offset = -(layer_width - auto_layer->text_width) / 2;
    }
    auto_layer->state = SCROLL_PAUSE_START;
    auto_layer->scroll_timer = NULL;
  }
  layer_mark_dirty(auto_layer->layer);
}

static AutoScrollTextLayer* auto_scroll_text_layer_create(GRect frame) {
  AutoScrollTextLayer *auto_layer = malloc(sizeof(AutoScrollTextLayer));
  if (!auto_layer) return NULL;
  memset(auto_layer, 0, sizeof(AutoScrollTextLayer));
  auto_layer->layer = layer_create_with_data(frame, sizeof(AutoScrollTextLayer *));
  if (!auto_layer->layer) { free(auto_layer); return NULL; }
  *(AutoScrollTextLayer **)layer_get_data(auto_layer->layer) = auto_layer;
  layer_set_update_proc(auto_layer->layer, auto_scroll_text_layer_update_proc);
  auto_layer->font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  auto_layer->text_color = GColorBlack;
  auto_layer->background_color = GColorClear;
  auto_layer->left_align_when_fits = false;
  return auto_layer;
}

static void auto_scroll_text_layer_destroy(AutoScrollTextLayer *auto_layer) {
  if (!auto_layer) return;
  if (auto_layer->scroll_timer) app_timer_cancel(auto_layer->scroll_timer);
  layer_destroy(auto_layer->layer);
  free(auto_layer);
}

static void marquee_idle_cb(void *context) {
  s_marquee_idle_timer = NULL;
  s_marquee_idle = true;

  if (s_no_location_flash_timer) {
    app_timer_cancel(s_no_location_flash_timer);
    s_no_location_flash_timer = NULL;
  }
  s_no_location_slash_visible = true;
  if (s_sv && s_sv->no_location_layer &&
      !layer_get_hidden(s_sv->no_location_layer)) {
    layer_mark_dirty(s_sv->no_location_layer);
  }
}

static void reset_marquee_activity(void) {
  if (s_marquee_idle_timer) {
    app_timer_cancel(s_marquee_idle_timer);
    s_marquee_idle_timer = NULL;
  }
  s_marquee_idle = false;

#if !SCREENSHOT_MODE
  AutoScrollTextLayer *layers[] = { s_prev, s_name, s_next, s_domain, s_saved_name_auto };
  for (size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
    AutoScrollTextLayer *al = layers[i];
    if (!al) continue;
    if (al->max_scroll <= 0) continue;
    if (al->scroll_timer) continue;
    al->state = SCROLL_PAUSE_START;
    al->scroll_timer = app_timer_register(MARQUEE_RESUME_DELAY_MS,
                                          auto_scroll_timer_callback, al);
  }

  if (s_sv && s_sv->route_name_auto) {
    AutoScrollTextLayer *al = s_sv->route_name_auto;
    if (al->max_scroll > 0 && !al->scroll_timer) {
      al->state = SCROLL_PAUSE_START;
      al->scroll_timer = app_timer_register(MARQUEE_RESUME_DELAY_MS,
                                            auto_scroll_timer_callback, al);
    }
  }
#endif

  start_route_scroll_timer();
  start_stops_scroll_timer();
  start_si_scroll_timer();

  if (s_si && s_si->window && !s_si_fetch_in_flight &&
      s_si_last_fetch_time > 0 &&
      (time(NULL) - s_si_last_fetch_time) > SI_ACTIVITY_FLOOR_S) {
    si_send_arrival_fetch();
  }
  si_schedule_refresh();

  s_marquee_idle_timer = app_timer_register(MARQUEE_IDLE_TIMEOUT_MS,
                                            marquee_idle_cb, NULL);

  no_location_flash_sync();
}

static void tap_handler(AccelAxisType axis, int32_t direction) { reset_marquee_activity(); }

#if defined(PBL_TOUCH)
static void touch_event_handler(const TouchEvent *event, void *context) {
  (void)event; (void)context;
  reset_marquee_activity();
}
#endif

static void top_bar_draw(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
#if defined(PBL_BW)
  graphics_context_set_fill_color(ctx, GColorBlack);
#else
  graphics_context_set_fill_color(ctx, s_primary_color);
#endif
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void draw_up_arrow(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2, cy = bounds.size.h / 2;
  GPoint points[3] = { GPoint(cx - 6, cy + 4), GPoint(cx + 6, cy + 4), GPoint(cx, cy - 4) };
  GPathInfo path_info = { .num_points = 3, .points = points };
  GPath *path = gpath_create(&path_info);
  graphics_context_set_fill_color(ctx, s_primary_color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_down_arrow(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int cx = bounds.size.w / 2, cy = bounds.size.h / 2;
  GPoint points[3] = { GPoint(cx - 6, cy - 4), GPoint(cx + 6, cy - 4), GPoint(cx, cy + 4) };
  GPathInfo path_info = { .num_points = 3, .points = points };
  GPath *path = gpath_create(&path_info);
  graphics_context_set_fill_color(ctx, s_primary_color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void draw_heart(GContext *ctx, int cx, int cy, int size, GColor color, bool filled) {
  static const int8_t pts[14][2] = {
    {  0, -5 }, { -5, -9 }, { -8, -8 }, { -9, -4 }, { -9,  0 },
    { -7,  3 }, { -4,  5 }, {  0,  9 }, {  4,  5 }, {  7,  3 },
    {  9,  0 }, {  9, -4 }, {  8, -8 }, {  5, -9 }
  };
  GPoint path_pts[14];
  for (int i = 0; i < 14; i++) {
    path_pts[i].x = cx + (pts[i][0] * size) / 18;
    path_pts[i].y = cy + (pts[i][1] * size) / 18;
  }
  GPathInfo info = { .num_points = 14, .points = path_pts };
  GPath *path = gpath_create(&info);
  if (filled) {
    graphics_context_set_fill_color(ctx, color);
    gpath_draw_filled(ctx, path);
  }
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

static void sort_routes(void) {
  for (int i = 1; i < s_route_count; i++) {
    RouteEntry tmp = s_routes[i];
    int j = i - 1;
    while (j >= 0) {
      bool tmp_before;
      if (s_routes[j].is_favorite != tmp.is_favorite) {
        tmp_before = (tmp.is_favorite && !s_routes[j].is_favorite);
      } else {
        tmp_before = (tmp.original_index < s_routes[j].original_index);
      }
      if (!tmp_before) break;
      s_routes[j + 1] = s_routes[j];
      j--;
    }
    s_routes[j + 1] = tmp;
  }
}

static void load_favorites(void) {
  s_favorite_count = 0;
  if (!s_favorites) {
    s_favorites = malloc(sizeof(int32_t) * MAX_FAVORITES);
    if (!s_favorites) return;
  }
  int size = persist_get_size(PERSIST_KEY_FAVORITES);
  if (size <= 0) return;
  int max_bytes = MAX_FAVORITES * (int)sizeof(int32_t);
  if (size > max_bytes) size = max_bytes;
  persist_read_data(PERSIST_KEY_FAVORITES, s_favorites, size);
  s_favorite_count = size / (int)sizeof(int32_t);
}

static void save_favorites(void) {
  if (!s_favorites) return;
  if (s_favorite_count == 0) persist_delete(PERSIST_KEY_FAVORITES);
  else persist_write_data(PERSIST_KEY_FAVORITES, s_favorites,
                          s_favorite_count * (int)sizeof(int32_t));
}

static void load_stop_settings(void) {
  s_stop_settings_count = 0;
  int size = persist_get_size(PERSIST_KEY_STOP_SETTINGS);
  if (size <= 0) return;
  int max_bytes = (int)sizeof(StopSetting) * MAX_STOP_SETTINGS;
  if (size > max_bytes) size = max_bytes;
  persist_read_data(PERSIST_KEY_STOP_SETTINGS, s_stop_settings, size);
  s_stop_settings_count = size / (int)sizeof(StopSetting);
}

static void save_stop_settings(void) {
  if (s_stop_settings_count == 0) {
    persist_delete(PERSIST_KEY_STOP_SETTINGS);
    return;
  }
  persist_write_data(PERSIST_KEY_STOP_SETTINGS, s_stop_settings,
                     s_stop_settings_count * (int)sizeof(StopSetting));
}

static StopSetting* find_stop_setting(int32_t stop_id, bool create) {
  if (stop_id <= 0) return NULL;
  for (int i = 0; i < s_stop_settings_count; i++) {
    if (s_stop_settings[i].stop_id == stop_id) return &s_stop_settings[i];
  }
  if (!create) return NULL;
  if (s_stop_settings_count >= MAX_STOP_SETTINGS) return NULL;
  StopSetting *s = &s_stop_settings[s_stop_settings_count++];
  s->stop_id = stop_id;
  s->flags = 0;
  s->bell_minutes = 0;
  return s;
}

static bool stop_has_bell(int32_t stop_id) {
  StopSetting *s = find_stop_setting(stop_id, false);
  return s && (s->flags & STOP_SETTING_BELL);
}

static bool stop_has_pin(int32_t stop_id) {
  StopSetting *s = find_stop_setting(stop_id, false);
  return s && (s->flags & STOP_SETTING_PIN);
}

static void stop_toggle_flag(int32_t stop_id, uint8_t bit) {
  StopSetting *s = find_stop_setting(stop_id, true);
  if (!s) return;
  s->flags ^= bit;
  if (s->flags == 0) {
    int idx = (int)(s - s_stop_settings);
    for (int i = idx; i < s_stop_settings_count - 1; i++) {
      s_stop_settings[i] = s_stop_settings[i + 1];
    }
    s_stop_settings_count--;
  }
  save_stop_settings();

  if (s_sv && s_sv->window) {
    recompute_stop_layout();
    if (s_sv->text_layer) layer_mark_dirty(s_sv->text_layer);
    start_stops_scroll_timer();
  }
}

static void apply_favorites_and_sort(void) {
  if (!s_favorites) s_favorite_count = 0;
  for (int i = 0; i < s_route_count; i++) {
    s_routes[i].original_index = (int16_t)i;
    s_routes[i].is_favorite = 0;
    int32_t rid = s_routes[i].route_id;
    for (int f = 0; f < s_favorite_count; f++) {
      if (s_favorites[f] == rid) { s_routes[i].is_favorite = 1; break; }
    }
  }
  if (s_favorite_count > 0 && s_route_count > 1) sort_routes();
}

static void toggle_favorite(int route_idx) {
  if (route_idx < 0 || route_idx >= s_route_count) return;
  if (!s_favorites) return;
  int32_t rid = s_routes[route_idx].route_id;
  if (rid <= 0) return;

  int found = -1;
  for (int i = 0; i < s_favorite_count; i++) {
    if (s_favorites[i] == rid) { found = i; break; }
  }
  if (found >= 0) {
    for (int i = found; i < s_favorite_count - 1; i++) s_favorites[i] = s_favorites[i + 1];
    s_favorite_count--;
    s_routes[route_idx].is_favorite = 0;
  } else {
    if (s_favorite_count >= MAX_FAVORITES) return;
    s_favorites[s_favorite_count++] = rid;
    s_routes[route_idx].is_favorite = 1;
  }
  save_favorites();

  int32_t selected_id = s_routes[s_route_selected].route_id;
  commit_pending_route_scroll();
  if (s_route_fill_anim) { animation_unschedule(s_route_fill_anim); s_route_fill_anim = NULL; }
  sort_routes();
  for (int i = 0; i < s_route_count; i++) {
    if (s_routes[i].route_id == selected_id) { s_route_selected = i; break; }
  }
  int new_fv = s_route_first_visible;
  if (s_route_selected < new_fv) new_fv = s_route_selected;
  if (s_route_selected >= new_fv + s_route_rows_visible) new_fv = s_route_selected - s_route_rows_visible + 1;
  if (new_fv < 0) new_fv = 0;
  int max_fv = s_route_count - s_route_rows_visible;
  if (max_fv < 0) max_fv = 0;
  if (new_fv > max_fv) new_fv = max_fv;
  s_route_first_visible = new_fv;

  for (int i = 0; i < s_route_count; i++) {
    s_routes[i].fill = (i == s_route_selected && !s_settings_selected)
                       ? s_route_full_w : s_route_square_w;
    s_routes[i].fill_from = s_routes[i].fill;
    s_routes[i].fill_to = s_routes[i].fill;
    s_routes[i].scroll_offset = 0;
    s_routes[i].scroll_direction = 1;
    s_routes[i].pause_ticks = ROUTE_SCROLL_PAUSE_TICKS;
  }
  s_route_list_scroll_y = 0;
  set_route_layers_y_offset(0);
  update_route_inv_layers();
  dirty_all_route_text_layers();
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
  vibes_short_pulse();
}

static void agency_bar_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, AGENCY_BAR_BG_COLOR);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
}

static void settings_icon_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GColor bg = s_settings_selected ? GColorWhite : GColorLightGray;
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  int cx = bounds.size.w / 2;
  int cy = bounds.size.h / 2;
  if (s_settings_gear_bitmap) {
    GSize icon_size = gbitmap_get_bounds(s_settings_gear_bitmap).size;
    int icon_x = (bounds.size.w - icon_size.w) / 2;
    int icon_y = (bounds.size.h - icon_size.h) / 2;
    GRect icon_rect = GRect(icon_x, icon_y, icon_size.w, icon_size.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_settings_gear_bitmap, icon_rect);
  } else {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 2);
    for (int i = -1; i <= 1; i++) {
      graphics_draw_line(ctx,
        GPoint(cx - 7, cy + i * 5),
        GPoint(cx + 7, cy + i * 5));
    }
  }
}

static void route_list_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GFont name_font = fonts_get_system_font(FONT_ROUTE_NAME);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (s_route_count == 0) {
    const char *msg;
    if (s_routes_load_failed) {
      msg = "Can't reach phone";
    } else if (s_routes_loaded) {
      msg = "No routes available";
    } else {
      msg = "Loading routes...";
    }
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, msg, name_font, bounds,
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    return;
  }
  int row_h = s_route_row_h;
  int32_t scroll_y = s_route_list_scroll_y;
  GSize icon_size = GSize(0, 0);
  if (s_route_bitmap) icon_size = gbitmap_get_bounds(s_route_bitmap).size;

  for (int j = -1; j <= s_route_rows_visible; j++) {
    int idx = s_route_first_visible + j;
    if (idx < 0 || idx >= s_route_count) continue;
    int y = j * row_h + scroll_y;
    int32_t fill_w = s_routes[idx].fill;
    if (fill_w < 0) fill_w = 0;
    if (fill_w > bounds.size.w) fill_w = bounds.size.w;
    if (fill_w > 0) {
      GColor line_color = GColorFromHEX(s_routes[idx].color);
      graphics_context_set_fill_color(ctx, line_color);
      graphics_fill_rect(ctx, GRect(0, y, fill_w, row_h), 0, GCornerNone);
    }
    if (s_route_bitmap && icon_size.w > 0 && icon_size.h > 0) {
      GRect icon_rect = GRect((row_h - icon_size.w) / 2,
                              y + (row_h - icon_size.h) / 2,
                              icon_size.w, icon_size.h);
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, s_route_bitmap, icon_rect);
    }
    const int heart_size = 15;
    int heart_cx = bounds.size.w - HEART_RIGHT_MARGIN - heart_size / 2;
    int heart_cy = y + row_h / 2;
    GColor heart_color;
    if (fill_w > heart_cx - heart_size / 2) {
      heart_color = contrast_gcolor(s_routes[idx].color);
    } else {
      heart_color = GColorBlack;
    }
    draw_heart(ctx, heart_cx, heart_cy, heart_size, heart_color,
               s_routes[idx].is_favorite != 0);
  }
}

static void route_text_layer_update_proc(Layer *layer, GContext *ctx) {
  int i = *(int *)layer_get_data(layer);
  int route_idx = s_route_first_visible + i;
  if (route_idx < 0 || route_idx >= s_route_count) return;
  GRect bounds = layer_get_bounds(layer);
  GFont name_font = fonts_get_system_font(FONT_ROUTE_NAME);
  int16_t scroll = s_routes[route_idx].scroll_offset;
  int16_t text_h = s_routes[route_idx].text_h;
  if (text_h <= 0) text_h = bounds.size.h;
  int y = (bounds.size.h - text_h) / 2 - TEXT_VISUAL_OFFSET;
  graphics_context_set_text_color(ctx, GColorBlack);
  GRect text_rect = GRect(-scroll, y, 1000, text_h);
  graphics_draw_text(ctx, s_routes[route_idx].name, name_font, text_rect,
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void route_inv_layer_update_proc(Layer *layer, GContext *ctx) {
  int i = *(int *)layer_get_data(layer);
  int route_idx = s_route_first_visible + i;
  if (route_idx < 0 || route_idx >= s_route_count) return;
  GRect bounds = layer_get_bounds(layer);
  GFont name_font = fonts_get_system_font(FONT_ROUTE_NAME);
  int16_t scroll = s_routes[route_idx].scroll_offset;
  GColor line_color = GColorFromHEX(s_routes[route_idx].color);
  graphics_context_set_fill_color(ctx, line_color);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), 0, GCornerNone);
  GColor inv_color = contrast_gcolor(s_routes[route_idx].color);
  int16_t text_h = s_routes[route_idx].text_h;
  if (text_h <= 0) text_h = bounds.size.h;
  int y = (bounds.size.h - text_h) / 2 - TEXT_VISUAL_OFFSET;
  graphics_context_set_text_color(ctx, inv_color);
  GRect text_rect = GRect(-scroll, y, 1000, text_h);
  graphics_draw_text(ctx, s_routes[route_idx].name, name_font, text_rect,
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void dirty_all_route_text_layers(void) {
  if (!s_route_text_layers || !s_route_inv_layers) return;
  for (int i = 0; i < ROUTE_LAYER_COUNT; i++) {
    if (s_route_text_layers[i]) layer_mark_dirty(s_route_text_layers[i]);
    if (s_route_inv_layers[i]) layer_mark_dirty(s_route_inv_layers[i]);
  }
}

static void update_route_inv_layers(void) {
  if (!s_route_inv_layers) return;
  for (int i = 0; i < ROUTE_LAYER_COUNT; i++) {
    Layer *l = s_route_inv_layers[i];
    if (!l) continue;
    int route_idx = s_route_first_visible + (i - 1);
    if (route_idx < 0 || route_idx >= s_route_count) { layer_set_hidden(l, true); continue; }
    int32_t fill_w = s_routes[route_idx].fill;
    if (fill_w > s_route_text_x) {
      int32_t inv_w = fill_w - s_route_text_x;
      if (inv_w > s_route_text_w) inv_w = s_route_text_w;
      GRect f = layer_get_frame(l);
      f.size.w = inv_w;
      layer_set_frame(l, f);
      layer_set_hidden(l, false);
    } else {
      layer_set_hidden(l, true);
    }
  }
}

static void set_route_layers_y_offset(int32_t y_offset) {
  if (!s_route_text_layers || !s_route_inv_layers) return;
  for (int i = 0; i < ROUTE_LAYER_COUNT; i++) {
    int y = (i - 1) * s_route_row_h + y_offset;
    if (s_route_text_layers[i]) {
      GRect f = layer_get_frame(s_route_text_layers[i]);
      f.origin.y = y;
      layer_set_frame(s_route_text_layers[i], f);
    }
    if (s_route_inv_layers[i]) {
      GRect f = layer_get_frame(s_route_inv_layers[i]);
      f.origin.y = y;
      layer_set_frame(s_route_inv_layers[i], f);
    }
  }
}

static void route_list_scroll_anim_update(Animation *anim, AnimationProgress progress) {
  int32_t target = s_route_list_scroll_target;
  s_route_list_scroll_y = (int16_t)(((int64_t)target * (int64_t)progress) / ANIMATION_NORMALIZED_MAX);
  set_route_layers_y_offset(s_route_list_scroll_y);
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
}

static void route_list_scroll_anim_teardown(Animation *anim) {
  s_route_first_visible = s_route_pending_first_visible;
  s_route_list_scroll_y = 0;
  s_route_list_scroll_target = 0;
  set_route_layers_y_offset(0);
  update_route_inv_layers();
  dirty_all_route_text_layers();
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
  s_route_list_scroll_anim = NULL;
  if (!s_marquee_idle) start_route_scroll_timer();
}

static void commit_pending_route_scroll(void) {
  if (!s_route_list_scroll_anim) return;
  Animation *anim = s_route_list_scroll_anim;
  s_route_list_scroll_anim = NULL;
  animation_unschedule(anim);
  s_route_first_visible = s_route_pending_first_visible;
  s_route_list_scroll_y = 0;
  s_route_list_scroll_target = 0;
  set_route_layers_y_offset(0);
  update_route_inv_layers();
  dirty_all_route_text_layers();
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
}

static void start_route_scroll_anim(int new_fv) {
  if (s_route_list_scroll_anim) {
    animation_unschedule(s_route_list_scroll_anim);
    s_route_list_scroll_anim = NULL;
  }
  s_route_pending_first_visible = (int16_t)new_fv;
  int delta = s_route_first_visible - new_fv;
  s_route_list_scroll_y = 0;
  s_route_list_scroll_target = (int16_t)(delta * s_route_row_h);
  static const AnimationImplementation impl = {
    .update = route_list_scroll_anim_update,
    .teardown = route_list_scroll_anim_teardown
  };
  s_route_list_scroll_anim = animation_create();
  animation_set_implementation(s_route_list_scroll_anim, &impl);
  animation_set_duration(s_route_list_scroll_anim, ROUTE_SCROLL_ANIM_DURATION_MS);
  animation_set_curve(s_route_list_scroll_anim, AnimationCurveEaseInOut);
  animation_schedule(s_route_list_scroll_anim);
}

static void route_scroll_tick(void *context) {
  s_route_scroll_timer = NULL;
#if SCREENSHOT_MODE
  return;
#else
  if (!s_showing_saved_view || s_route_count == 0) return;
  if (s_route_list_scroll_anim != NULL) {
    s_route_scroll_timer = app_timer_register(ROUTE_SCROLL_TICK_MS, route_scroll_tick, NULL);
    return;
  }
  bool any_dirty = false;
  int start = s_marquee_idle ? 0 : s_route_first_visible;
  int end   = s_marquee_idle ? s_route_count : (s_route_first_visible + s_route_rows_visible);
  for (int i = start; i < end; i++) {
    if (i < 0 || i >= s_route_count) continue;
    if (s_routes[i].max_scroll <= 0) continue;
    RouteEntry *r = &s_routes[i];
    if (s_marquee_idle && r->scroll_offset == 0) { r->scroll_direction = 1; continue; }
    if (r->pause_ticks > 0) {
      if (!s_marquee_idle) { r->pause_ticks--; continue; }
      r->pause_ticks = 0;
    }
    if (r->scroll_direction >= 0) {
      r->scroll_offset += ROUTE_SCROLL_STEP;
      if (r->scroll_offset >= r->max_scroll) {
        r->scroll_offset = r->max_scroll;
        r->scroll_direction = -1;
        if (!s_marquee_idle) r->pause_ticks = ROUTE_SCROLL_PAUSE_TICKS;
      }
    } else {
      r->scroll_offset -= ROUTE_SCROLL_STEP;
      if (r->scroll_offset <= 0) {
        r->scroll_offset = 0;
        r->scroll_direction = 1;
        if (!s_marquee_idle) r->pause_ticks = ROUTE_SCROLL_PAUSE_TICKS;
      }
    }
    any_dirty = true;
  }
  if (any_dirty) dirty_all_route_text_layers();
  if (s_marquee_idle) {
    bool any_moving = false;
    for (int i = 0; i < s_route_count; i++) {
      if (s_routes[i].max_scroll <= 0) continue;
      if (s_routes[i].scroll_offset != 0) { any_moving = true; break; }
    }
    if (!any_moving) return;
  }
  s_route_scroll_timer = app_timer_register(ROUTE_SCROLL_TICK_MS, route_scroll_tick, NULL);
#endif
}

static void start_route_scroll_timer(void) {
#if SCREENSHOT_MODE
  return;
#endif
  if (s_route_scroll_timer) return;
  if (s_route_count == 0) return;
  bool any_scroll = false;
  for (int i = 0; i < s_route_count; i++) {
    if (s_routes[i].max_scroll > 0) { any_scroll = true; break; }
  }
  if (!any_scroll) return;
  s_route_scroll_timer = app_timer_register(ROUTE_SCROLL_TICK_MS, route_scroll_tick, NULL);
}

static void route_fill_anim_update(Animation *anim, AnimationProgress progress) {
  for (int i = 0; i < s_route_count; i++) {
    int64_t from = s_routes[i].fill_from;
    int64_t to   = s_routes[i].fill_to;
    s_routes[i].fill = (int32_t)(from + ((to - from) * (int64_t)progress) / ANIMATION_NORMALIZED_MAX);
  }
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
  update_route_inv_layers();
  dirty_all_route_text_layers();
}

static void route_fill_anim_teardown(Animation *anim) {
  for (int i = 0; i < s_route_count; i++) s_routes[i].fill = s_routes[i].fill_to;
  s_route_fill_anim = NULL;
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
  update_route_inv_layers();
  dirty_all_route_text_layers();
}

static void start_route_fill_anim(void) {
  if (s_route_count == 0) return;
  if (s_route_fill_anim) { animation_unschedule(s_route_fill_anim); s_route_fill_anim = NULL; }
  for (int i = 0; i < s_route_count; i++) {
    s_routes[i].fill_from = s_routes[i].fill;
    s_routes[i].fill_to = (i == s_route_selected && !s_settings_selected)
                          ? s_route_full_w : s_route_square_w;
  }
  static const AnimationImplementation impl = {
    .update = route_fill_anim_update,
    .teardown = route_fill_anim_teardown
  };
  s_route_fill_anim = animation_create();
  animation_set_implementation(s_route_fill_anim, &impl);
  animation_set_duration(s_route_fill_anim, ROUTE_FILL_DURATION_MS);
  animation_set_curve(s_route_fill_anim, AnimationCurveEaseInOut);
  animation_schedule(s_route_fill_anim);
}

static void update_route_list(void) { if (s_route_list_layer) layer_mark_dirty(s_route_list_layer); }

static uint32_t bw_safe_color(uint32_t color) {
#if defined(PBL_BW)
  int r = (color >> 16) & 0xFF, g = (color >> 8) & 0xFF, b = color & 0xFF;
  int brightness = (r * 299 + g * 587 + b * 114) / 1000;
  return (brightness < 128) ? 0x000000 : 0xAAAAAA;
#endif
  return color;
}

static bool route_grow(void) {
  if (s_route_count < s_route_capacity) return true;
  int new_cap = (s_route_capacity == 0) ? ROUTE_INITIAL_CAPACITY : s_route_capacity * 2;
  RouteEntry *grown = realloc(s_routes, new_cap * sizeof(RouteEntry));
  if (!grown) return false;
  s_routes = grown;
  s_route_capacity = new_cap;
  return true;
}

static void parse_routes_line(const char *line) {
  if (s_parsing_palette) {
    if (line[0] == 'P' && line[1] == ';') {
      const char *p = line + 2;
      s_palette_count = 0;
      while (*p && s_palette_count < MAX_PALETTE) {
        uint32_t color = 0;
        int digits = 0;
        while (digits < 6 && *p && *p != ';') {
          char ch = *p++;
          int nibble = 0;
          if (ch >= '0' && ch <= '9') nibble = ch - '0';
          else if (ch >= 'a' && ch <= 'f') nibble = ch - 'a' + 10;
          else if (ch >= 'A' && ch <= 'F') nibble = ch - 'A' + 10;
          color = (color << 4) | (uint32_t)nibble;
          digits++;
        }
        s_palette[s_palette_count++] = color;
        if (*p == ';') p++;
      }
    }
    s_parsing_palette = false;
    return;
  }
  if (!route_grow()) return;
  RouteEntry *r = &s_routes[s_route_count];
  const char *p = line;
  const char *name_start = p;
  while (*p && *p != ';') p++;
  int name_len = p - name_start;
  if (name_len >= (int)sizeof(r->name)) name_len = sizeof(r->name) - 1;
  memcpy(r->name, name_start, name_len);
  r->name[name_len] = '\0';

  uint32_t color = 0;
  if (*p == ';') p++;
  int idx = 0;
  while (*p >= '0' && *p <= '9') { idx = idx * 10 + (*p - '0'); p++; }
  if (idx >= 0 && idx < s_palette_count) color = s_palette[idx];
  r->color = bw_safe_color(color);

  int32_t route_id = 0;
  if (*p == ';') {
    p++;
    while (*p >= '0' && *p <= '9') { route_id = route_id * 10 + (*p - '0'); p++; }
  }
  r->route_id = route_id;

  GFont name_font = fonts_get_system_font(FONT_ROUTE_NAME);
  int text_area_w = s_route_text_w;
  if (text_area_w < 1) text_area_w = 1;
  GSize sz = graphics_text_layout_get_content_size(r->name, name_font,
                                                   GRect(0, 0, 1000, 100),
                                                   GTextOverflowModeFill, GTextAlignmentLeft);
  int16_t natural_w = (int16_t)sz.w;
  r->max_scroll = (natural_w > text_area_w) ? (natural_w - text_area_w) : 0;
  r->text_h = (int16_t)sz.h;
  r->original_index = (int16_t)s_route_count;
  r->is_favorite = 0;
  r->scroll_offset = 0;
  r->scroll_direction = 1;
  r->pause_ticks = ROUTE_SCROLL_PAUSE_TICKS;
  bool is_first = (s_route_count == 0);
  r->fill = is_first ? s_route_full_w : s_route_square_w;
  r->fill_from = r->fill;
  r->fill_to = r->fill;
  s_route_count++;
}

static void handle_routes_chunk(int32_t idx, int32_t total, const char *data) {
  if (idx == 0) {
    if (!s_line_buf) {
      s_line_buf = malloc(LINE_BUF_SIZE);
      if (!s_line_buf) return;
    }
    if (!s_palette) {
      s_palette = malloc(sizeof(uint32_t) * MAX_PALETTE);
      if (!s_palette) return;
    }
    s_route_count = 0;
    s_line_len = 0;
    s_parsing_palette = true;
    s_chunks_received = 0;
    s_chunks_total = (uint8_t)total;
    s_route_selected = 0;
    s_route_first_visible = 0;
    s_settings_selected = false;
    if (s_routes) { free(s_routes); s_routes = NULL; }
    s_route_capacity = 0;
  }
  if (idx != s_chunks_received) return;
  const char *p = data;
  while (*p) {
    char c = *p++;
    if (c == '\n') {
      s_line_buf[s_line_len] = '\0';
      if (s_line_len > 0) parse_routes_line(s_line_buf);
      s_line_len = 0;
    } else if (s_line_len < LINE_BUF_SIZE - 1) {
      s_line_buf[s_line_len++] = c;
    } else {
      s_line_len = 0;
    }
  }
  s_chunks_received++;
  if (s_chunks_received == s_chunks_total) {
    if (s_line_len > 0) { s_line_buf[s_line_len] = '\0'; parse_routes_line(s_line_buf); s_line_len = 0; }
    apply_favorites_and_sort();
    for (int i = 0; i < s_route_count; i++) {
      s_routes[i].fill = (i == s_route_selected && !s_settings_selected)
                         ? s_route_full_w : s_route_square_w;
      s_routes[i].fill_from = s_routes[i].fill;
      s_routes[i].fill_to = s_routes[i].fill;
      s_routes[i].scroll_offset = 0;
      s_routes[i].scroll_direction = 1;
      s_routes[i].pause_ticks = ROUTE_SCROLL_PAUSE_TICKS;
    }
    s_routes_loaded = true;
    update_route_list();
    update_route_inv_layers();
    dirty_all_route_text_layers();
    reset_marquee_activity();
    s_loading = false;
    status_icon_set_loading(false);
    if (s_pending_save_view) {
      s_pending_save_view = false;
      if (s_save_view_timeout_timer) {
        app_timer_cancel(s_save_view_timeout_timer);
        s_save_view_timeout_timer = NULL;
      }
      push_routes_window();
    }
  }
}

static void update_no_location_icon(void) {
  if (!s_sv || !s_sv->no_location_layer) return;
  bool show = !s_have_location && !s_location_pending;
  bool was_hidden = layer_get_hidden(s_sv->no_location_layer);

  if (show && was_hidden) {
    s_no_location_slash_visible = false;
  }

  layer_set_hidden(s_sv->no_location_layer, !show);
  if (show) layer_mark_dirty(s_sv->no_location_layer);
  no_location_flash_sync();
}

static void try_finish_stops_load(void) {
  update_no_location_icon();
  if (!s_stops_loaded) return;
  if (s_location_pending) return;

  if (s_have_location && s_stop_count > 0) compute_stop_distances();

  if (s_sv && s_sv->window) {
    recompute_stop_layout();
    if (s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
    if (s_sv->text_layer) layer_mark_dirty(s_sv->text_layer);
    start_stops_scroll_timer();
  }
  status_icon_set_loading(false);
}

static void location_timeout_cb(void *context) {
  if (!s_sv) return;
  s_sv->location_timeout_timer = NULL;
  s_location_pending = false;
  try_finish_stops_load();
}

static void recompute_stop_layout(void) {
  if (s_stop_text_w < 1) return;
  GFont name_font = fonts_get_system_font(FONT_STOP_NAME);
  GFont sub_font  = fonts_get_system_font(FONT_STOP_DIST);

  int narrow_w = s_stop_text_w - (STOP_ICON_SIZE + 4);
  if (narrow_w < 1) narrow_w = 1;

  for (int i = 0; i < s_stop_count; i++) {
      int has_icons = stop_has_bell(s_stops[i].stop_id) ||
                      stop_has_pin(s_stops[i].stop_id);
      int W = has_icons ? narrow_w : s_stop_text_w;
      int eff_w = W - 4;
      if (eff_w < 1) eff_w = 1;

      GSize nsz = graphics_text_layout_get_content_size(
        s_stops[i].name, name_font,
        GRect(0, 0, 10000, 100),
        GTextOverflowModeFill, GTextAlignmentLeft);
      s_stop_max_scrolls[i] = (nsz.w > eff_w) ? (nsz.w - eff_w) : 0;
      s_stop_scroll_offsets[i] = 0;
      s_stop_scroll_dirs[i] = 1;
      s_stop_pause_ticks[i] = STOPS_SCROLL_PAUSE_TICKS;

      char *sub = s_stop_sub_texts[i];
      if (s_have_location && s_stop_distances) {
        int32_t ft = s_stop_distances[i];
        char dist[16];
        if (ft < 1000) snprintf(dist, sizeof(dist), "%d ft", (int)ft);
        else {
          int32_t tenths = (ft * 10 + 2640) / 5280;
          snprintf(dist, sizeof(dist), "%d.%d mi",
                   (int)(tenths / 10), (int)(tenths % 10));
        }
        if (i == s_stop_display_anchor)
          snprintf(sub, 24, "%s (nearest)", dist);
        else
          snprintf(sub, 24, "%s", dist);
      } else {
        snprintf(sub, 24, "Stop #%d", i + 1);
      }

      GSize ssz = graphics_text_layout_get_content_size(
        sub, sub_font, GRect(0, 0, 10000, 100),
        GTextOverflowModeFill, GTextAlignmentLeft);
      s_stop_sub_max_scrolls[i] = (ssz.w > eff_w) ? (ssz.w - eff_w) : 0;
      s_stop_sub_scroll_offsets[i] = 0;
      s_stop_sub_scroll_dirs[i] = 1;
      s_stop_sub_pause_ticks[i] = STOPS_SCROLL_PAUSE_TICKS;
    }
  APP_LOG(APP_LOG_LEVEL_INFO, "stops layout: %d stops, full=%d narrow=%d",
          s_stop_count, s_stop_text_w, narrow_w);
}

static void draw_stop_target(GContext *ctx, int cx, int cy, GColor color) {
  GColor contrast = (color.argb == GColorWhite.argb) ? GColorBlack : GColorWhite;

  graphics_context_set_fill_color(ctx, contrast);
  graphics_fill_circle(ctx, GPoint(cx, cy), STOPS_TARGET_RADIUS);

  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_circle(ctx, GPoint(cx, cy), STOPS_TARGET_RADIUS);

  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(cx, cy), STOPS_TARGET_DOT);
}

static bool advance_scroll(int idx, int16_t *offsets, int16_t *maxes,
                           int8_t *dirs, uint8_t *pauses) {
  if (maxes[idx] <= 0) return false;
  if (s_marquee_idle && offsets[idx] == 0) { dirs[idx] = 1; return false; }
  if (pauses[idx] > 0) {
    if (!s_marquee_idle) { pauses[idx]--; return false; }
    pauses[idx] = 0;
  }
  if (dirs[idx] >= 0) {
    offsets[idx] += STOPS_SCROLL_STEP;
    if (offsets[idx] >= maxes[idx]) {
      offsets[idx] = maxes[idx];
      dirs[idx] = -1;
      if (!s_marquee_idle) pauses[idx] = STOPS_SCROLL_PAUSE_TICKS;
    }
  } else {
    offsets[idx] -= STOPS_SCROLL_STEP;
    if (offsets[idx] <= 0) {
      offsets[idx] = 0;
      dirs[idx] = 1;
      if (!s_marquee_idle) pauses[idx] = STOPS_SCROLL_PAUSE_TICKS;
    }
  }
  return true;
}

static bool advance_scroll_one(int16_t *offset, int16_t *max,
                               int8_t *dir, uint8_t *pause) {
  if (*max <= 0) return false;
  if (s_marquee_idle && *offset == 0) { *dir = 1; return false; }
  if (*pause > 0) {
    if (!s_marquee_idle) { (*pause)--; return false; }
    *pause = 0;
  }
  if (*dir >= 0) {
    (*offset)++;
    if (*offset >= *max) {
      *offset = *max;
      *dir = -1;
      if (!s_marquee_idle) *pause = STOPS_SCROLL_PAUSE_TICKS;
    }
  } else {
    (*offset)--;
    if (*offset <= 0) {
      *offset = 0;
      *dir = 1;
      if (!s_marquee_idle) *pause = STOPS_SCROLL_PAUSE_TICKS;
    }
  }
  return true;
}

static void stops_scroll_tick(void *context) {
  s_stops_scroll_timer = NULL;
#if SCREENSHOT_MODE
  return;
#else
  if (!s_sv || !s_sv->window) return;
  if (s_stop_count == 0) return;

  bool any_dirty = false;
  int start = s_marquee_idle ? 0 : s_stop_first_visible;
  int end   = s_marquee_idle ? s_stop_count
                             : (s_stop_first_visible + s_stop_rows_visible);

  for (int disp = start; disp < end; disp++) {
    if (disp < 0 || disp >= s_stop_count) continue;
    int stop_idx = (s_stop_display_anchor + disp) % s_stop_count;
    if (advance_scroll(stop_idx, s_stop_scroll_offsets, s_stop_max_scrolls,
                       s_stop_scroll_dirs, s_stop_pause_ticks)) any_dirty = true;
    if (advance_scroll(stop_idx, s_stop_sub_scroll_offsets, s_stop_sub_max_scrolls,
                       s_stop_sub_scroll_dirs, s_stop_sub_pause_ticks)) any_dirty = true;
  }
  if (any_dirty && s_sv->text_layer) layer_mark_dirty(s_sv->text_layer);

  if (s_marquee_idle) {
    bool any_moving = false;
    for (int i = 0; i < s_stop_count; i++) {
      if (s_stop_max_scrolls[i] > 0 && s_stop_scroll_offsets[i] != 0) {
        any_moving = true; break;
      }
      if (s_stop_sub_max_scrolls[i] > 0 && s_stop_sub_scroll_offsets[i] != 0) {
        any_moving = true; break;
      }
    }
    if (!any_moving) return;
  }
  s_stops_scroll_timer = app_timer_register(STOPS_SCROLL_TICK_MS,
                                            stops_scroll_tick, NULL);
#endif
}

static void start_stops_scroll_timer(void) {
#if SCREENSHOT_MODE
  return;
#endif
  if (s_stops_scroll_timer) return;
  if (!s_sv || !s_sv->window) return;
  if (s_stop_count == 0) return;
  bool any_scroll = false;
  for (int i = 0; i < s_stop_count; i++) {
    if (s_stop_max_scrolls[i] > 0 || s_stop_sub_max_scrolls[i] > 0) {
      any_scroll = true; break;
    }
  }
  if (!any_scroll) return;
  s_stops_scroll_timer = app_timer_register(STOPS_SCROLL_TICK_MS,
                                            stops_scroll_tick, NULL);
}

static void stops_list_scroll_anim_update(Animation *anim, AnimationProgress progress) {
  int32_t target = s_stops_list_scroll_target;
  s_stops_list_scroll_y = (int16_t)(((int64_t)target * (int64_t)progress) / ANIMATION_NORMALIZED_MAX);
  if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
  if (s_sv && s_sv->text_layer) layer_mark_dirty(s_sv->text_layer);
}

static void stops_list_scroll_anim_teardown(Animation *anim) {
  s_stop_first_visible = (uint8_t)s_stop_pending_first_visible;
  s_stops_list_scroll_y = 0;
  s_stops_list_scroll_target = 0;
  s_stops_list_scroll_anim = NULL;
  if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
}

static void commit_pending_stops_scroll(void) {
  if (!s_stops_list_scroll_anim) return;
  Animation *anim = s_stops_list_scroll_anim;
  s_stops_list_scroll_anim = NULL;
  animation_unschedule(anim);
  s_stop_first_visible = (uint8_t)s_stop_pending_first_visible;
  s_stops_list_scroll_y = 0;
  s_stops_list_scroll_target = 0;
  if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
}

static void start_stops_scroll_anim(int delta_fv, int new_fv) {
  if (s_stops_list_scroll_anim) {
    animation_unschedule(s_stops_list_scroll_anim);
    s_stops_list_scroll_anim = NULL;
  }
  s_stop_pending_first_visible = (int16_t)new_fv;
  s_stops_list_scroll_y = 0;
  s_stops_list_scroll_target = (int16_t)(delta_fv * s_stop_row_h);
  static const AnimationImplementation impl = {
    .update = stops_list_scroll_anim_update,
    .teardown = stops_list_scroll_anim_teardown
  };
  s_stops_list_scroll_anim = animation_create();
  animation_set_implementation(s_stops_list_scroll_anim, &impl);
  animation_set_duration(s_stops_list_scroll_anim, STOPS_SCROLL_ANIM_DURATION_MS);
  animation_set_curve(s_stops_list_scroll_anim, AnimationCurveEaseInOut);
  animation_schedule(s_stops_list_scroll_anim);
}

/* ---- Stop row icons ---- */

static GPoint icon_pt(int cx, int cy, int sx, int sy, int size) {
  GPoint p;
  p.x = (int16_t)(cx + ((sx - 125) * size) / 250);
  p.y = (int16_t)(cy + ((sy - 125) * size) / 250);
  return p;
}

static void icon_fill(GContext *ctx, int cx, int cy, int size, GColor color,
                      const int16_t (*pts)[2], int n) {
  GPoint pp[25];
  if (n > 24) n = 24;
  for (int i = 0; i < n; i++) pp[i] = icon_pt(cx, cy, pts[i][0], pts[i][1], size);
  GPathInfo info = { .num_points = (uint16_t)n, .points = pp };
  GPath *path = gpath_create(&info);
  graphics_context_set_fill_color(ctx, color);
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void icon_outline(GContext *ctx, int cx, int cy, int size, GColor color,
                         int stroke_w, const int16_t (*pts)[2], int n) {
  GPoint pp[25];
  if (n > 24) n = 24;
  for (int i = 0; i < n; i++) pp[i] = icon_pt(cx, cy, pts[i][0], pts[i][1], size);
  pp[n] = pp[0];
  GPathInfo info = { .num_points = (uint16_t)(n + 1), .points = pp };
  GPath *path = gpath_create(&info);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, stroke_w);
  gpath_draw_outline(ctx, path);
  gpath_destroy(path);
}

static const int16_t BELL_BODY[13][2] = {
  { 44, 111}, { 57, 100}, { 81,  94}, { 87,  94}, { 91,  95},
  {135,  51}, {176,  45}, {204,  74}, {199, 115}, {155, 158},
  {156, 163}, {150, 192}, {139, 206}
};

static const int16_t BELL_DIAMOND_BL[4][2] = {
  { 77, 159}, { 63, 173}, { 77, 187}, { 91, 173}
};

static const int16_t BELL_SLASH[4][2] = {
  { 99,  88}, { 91,  95}, {155, 159}, {162, 152}
};

static const int16_t BELL_DIAMOND_TR[4][2] = {
  {205,  31}, {190,  46}, {205,  60}, {219,  46}
};

static void draw_bell_icon(GContext *ctx, int cx, int cy, GColor color) {
  int size = STOP_ICON_SIZE;
  GColor fill = (color.argb == GColorWhite.argb) ? GColorBlack : GColorWhite;

  icon_fill(ctx, cx, cy, size, fill, BELL_BODY, 13);
  icon_outline(ctx, cx, cy, size, color, 2, BELL_BODY, 13);
  icon_fill(ctx, cx, cy, size, color, BELL_DIAMOND_BL, 4);
  icon_fill(ctx, cx, cy, size, color, BELL_SLASH, 4);
  icon_fill(ctx, cx, cy, size, color, BELL_DIAMOND_TR, 4);
}

static const int16_t PIN_BODY[17][2] = {
  { 38, 134}, { 50, 124}, { 72, 119}, { 78, 119}, { 82, 120},
  {135,  82}, {136,  77}, {140,  57}, {151,  35}, {215,  99},
  {193, 110}, {173, 114}, {168, 115}, {130, 168}, {131, 172},
  {126, 200}, {116, 212}
};

static const int16_t PIN_HANDLE[4][2] = {
  { 63, 173}, { 20, 216}, { 34, 230}, { 77, 187}
};

static const int16_t PIN_SLASH_1[4][2] = {
  {145,  71}, {138,  78}, {172, 113}, {179, 105}
};

static const int16_t PIN_SLASH_2[4][2] = {
  { 85, 117}, { 78, 124}, {126, 172}, {133, 165}
};

static void draw_pin_icon(GContext *ctx, int cx, int cy, GColor color) {
  int size = STOP_ICON_SIZE;
  GColor fill = (color.argb == GColorWhite.argb) ? GColorBlack : GColorWhite;

  icon_fill(ctx, cx, cy, size, fill, PIN_BODY, 17);
  icon_outline(ctx, cx, cy, size, color, 2, PIN_BODY, 17);
  icon_fill(ctx, cx, cy, size, color, PIN_HANDLE, 4);
  icon_fill(ctx, cx, cy, size, color, PIN_SLASH_1, 4);
  icon_fill(ctx, cx, cy, size, color, PIN_SLASH_2, 4);

  GPoint stem_a = icon_pt(cx, cy,  63, 187, size);
  GPoint stem_b = icon_pt(cx, cy,  28, 216, size);
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, stem_a, stem_b);
}

static void stops_list_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (!s_stops_loaded || s_location_pending || s_stop_count == 0) {
    const char *msg;
    if (!s_stops_loaded)        msg = "Loading stops...";
    else if (s_location_pending) msg = "Locating...";
    else                         msg = "No stops for this route";
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, msg, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                       bounds, GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

#if defined(PBL_BW)
  GColor highlight_bg = GColorBlack;
  GColor highlight_fg = GColorWhite;
#else
  uint32_t route_rgb = s_routes[s_route_selected].color;
  GColor highlight_bg = GColorFromHEX(route_rgb);
  GColor highlight_fg = contrast_gcolor(route_rgb);
#endif
  const GColor strip_gray = GColorLightGray;
  const GColor strip_line_unsel = GColorBlack;

  int row_h = s_stop_row_h;
  int target_cx = STOPS_TARGET_COL_W / 2;
  int scroll_y = s_stops_list_scroll_y;

  for (int r = -1; r <= s_stop_rows_visible; r++) {
    int disp = s_stop_first_visible + r;
    if (disp < 0 || disp >= s_stop_count) continue;
    int y = r * row_h + scroll_y;
    GColor bg = (disp == s_stop_selected) ? highlight_bg : GColorWhite;
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, row_h), 0, GCornerNone);
  }

  for (int r = -1; r <= s_stop_rows_visible; r++) {
    int disp = s_stop_first_visible + r;
    if (disp < 0 || disp >= s_stop_count) continue;
    int y = r * row_h + scroll_y;
    GColor strip = (disp == s_stop_selected) ? highlight_bg : strip_gray;
    graphics_context_set_fill_color(ctx, strip);
    graphics_fill_rect(ctx, GRect(0, y, STOPS_TARGET_COL_W, row_h), 0, GCornerNone);
  }

  for (int r = -1; r <= s_stop_rows_visible; r++) {
    int disp = s_stop_first_visible + r;
    if (disp < 0 || disp >= s_stop_count) continue;
    int y = r * row_h + scroll_y;
    int cy = y + row_h / 2;
    bool sel = (disp == s_stop_selected);
    GColor strip = sel ? highlight_bg : strip_gray;
    GColor draw_color = sel ? highlight_fg : strip_line_unsel;

    graphics_context_set_fill_color(ctx, strip);
    graphics_fill_rect(ctx, GRect(0, y, STOPS_TARGET_COL_W, row_h), 0, GCornerNone);

    graphics_context_set_stroke_color(ctx, draw_color);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(target_cx, y), GPoint(target_cx, y + row_h));

    draw_stop_target(ctx, target_cx, cy, draw_color);
  }
}

static void stops_text_update_proc(Layer *layer, GContext *ctx) {
  if (!s_stops_loaded || s_location_pending || s_stop_count == 0) return;

  GRect bounds = layer_get_bounds(layer);

#if defined(PBL_BW)
  GColor sel_bg = GColorBlack;
  GColor highlight_fg = GColorWhite;
#else
  uint32_t route_rgb = s_routes[s_route_selected].color;
  GColor sel_bg = GColorFromHEX(route_rgb);
  GColor highlight_fg = contrast_gcolor(route_rgb);
#endif
  const GColor normal_fg = GColorBlack;
  const GColor sub_fg    = GColorDarkGray;

  int local_text_x = s_stop_text_x - STOPS_TARGET_COL_W;
  int row_h = s_stop_row_h;
  int scroll_y = s_stops_list_scroll_y;

  GFont name_font = fonts_get_system_font(FONT_STOP_NAME);
  GFont dist_font = fonts_get_system_font(FONT_STOP_DIST);

  const int draw_w = 1000;

  int local_narrow = s_stop_text_w - (STOP_ICON_SIZE + 4);
  if (local_narrow < 1) local_narrow = 1;

  const int right_margin = 4;
  int icon_cx = bounds.size.w - right_margin - STOP_ICON_SIZE / 2;

  const int stack_gap = 2;
  int stack_h = STOP_ICON_SIZE * 2 + stack_gap;
  int stack_top_off = (row_h - stack_h) / 2;

  for (int r = -1; r <= s_stop_rows_visible; r++) {
    int disp = s_stop_first_visible + r;
    if (disp < 0 || disp >= s_stop_count) continue;
    int y = r * row_h + scroll_y;
    bool sel = (disp == s_stop_selected);
    int stop_idx = (s_stop_display_anchor + disp) % s_stop_count;

    int16_t scroll_name = s_stop_scroll_offsets[stop_idx];
    int16_t scroll_sub  = s_stop_sub_scroll_offsets[stop_idx];

    graphics_context_set_text_color(ctx, sel ? highlight_fg : normal_fg);
    graphics_draw_text(ctx, s_stops[stop_idx].name, name_font,
                       GRect(local_text_x - scroll_name, y + 2,
                             draw_w, 24),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);

    graphics_context_set_text_color(ctx, sel ? highlight_fg : sub_fg);
    graphics_draw_text(ctx, s_stop_sub_texts[stop_idx], dist_font,
                       GRect(local_text_x - scroll_sub, y + row_h - 18,
                             draw_w, 20),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);

    int32_t sid = s_stops[stop_idx].stop_id;
    bool has_bell = stop_has_bell(sid);
    bool has_pin  = stop_has_pin(sid);

    if (has_bell || has_pin) {
      GColor bg = sel ? sel_bg : GColorWhite;
      graphics_context_set_fill_color(ctx, bg);
      graphics_fill_rect(ctx,
                         GRect(local_narrow, y,
                               bounds.size.w - local_narrow, row_h),
                         0, GCornerNone);

      GColor icon_color = sel ? highlight_fg : normal_fg;
      int bell_cy = y + stack_top_off + STOP_ICON_SIZE / 2;
      int pin_cy  = y + stack_top_off + STOP_ICON_SIZE + stack_gap
                    + STOP_ICON_SIZE / 2;
      if (has_bell) draw_bell_icon(ctx, icon_cx, bell_cy, icon_color);
      if (has_pin)  draw_pin_icon(ctx, icon_cx, pin_cy, icon_color);
    }
  }
}

static void stops_route_bar_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  GColor bar_color = s_primary_color;
  if (s_route_count > 0 && s_route_selected < s_route_count) {
    bar_color = GColorFromHEX(s_routes[s_route_selected].color);
  }
  graphics_context_set_fill_color(ctx, bar_color);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  if (!s_route_bitmap) return;
  if (s_route_count == 0 || s_route_selected >= s_route_count) return;

  GSize icon_size = gbitmap_get_bounds(s_route_bitmap).size;
  int icon_y = (bounds.size.h - icon_size.h) / 2;
  GRect icon_rect = GRect(LEFT_MARGIN, icon_y, icon_size.w, icon_size.h);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  graphics_draw_bitmap_in_rect(ctx, s_route_bitmap, icon_rect);
}

static void compute_stop_distances(void) {
  if (!s_have_location || s_stop_count == 0) { s_stop_display_anchor = 0; return; }
  if (!s_stop_distances) {
    s_stop_distances = malloc(sizeof(int32_t) * s_stop_count);
    if (!s_stop_distances) { s_stop_display_anchor = 0; return; }
  }
  int16_t best_idx = 0;
  int64_t best_d2 = -1;
  for (int i = 0; i < s_stop_count; i++) {
    int64_t dlat = (int64_t)s_stops[i].lat - s_my_lat;
    int64_t dlon = (int64_t)s_stops[i].lon - s_my_lon;
    int64_t d2 = dlat * dlat + dlon * dlon;
    int64_t ft = (isqrt64(d2) * 365) / 10000;
    if (ft > 2000000000LL) ft = 2000000000LL;
    s_stop_distances[i] = (int32_t)ft;
    if (best_d2 < 0 || d2 < best_d2) { best_d2 = d2; best_idx = i; }
  }
  s_stop_display_anchor = best_idx;
  APP_LOG(APP_LOG_LEVEL_INFO, "stop anchor=%d (nearest)", (int)best_idx);
}

static void stops_move_selection(int delta) {
  if (s_stop_count == 0) return;
  commit_pending_stops_scroll();

  int new_sel = (int)s_stop_selected + delta;
  if (new_sel < 0) new_sel = s_stop_count - 1;
  if (new_sel >= s_stop_count) new_sel = 0;
  s_stop_selected = (uint8_t)new_sel;

  int new_fv = s_stop_first_visible;
  if (s_stop_selected < new_fv) new_fv = s_stop_selected;
  if (s_stop_selected >= new_fv + s_stop_rows_visible)
    new_fv = s_stop_selected - s_stop_rows_visible + 1;

  if (new_fv == s_stop_first_visible) {
    if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
    return;
  }
  int delta_fv = s_stop_first_visible - new_fv;
  if (delta_fv == 1 || delta_fv == -1) {
    start_stops_scroll_anim(delta_fv, new_fv);
  } else {
    s_stop_first_visible = (uint8_t)new_fv;
    s_stops_list_scroll_y = 0;
    if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
  }
}

static void stops_hold_repeat_cb(void *context) {
  s_hold_timer = NULL;
  if (!s_hold_active) return;
  stops_move_selection(s_held_button == BUTTON_ID_UP ? -1 : 1);
  s_hold_timer = app_timer_register(HOLD_REPEAT_MS, stops_hold_repeat_cb, NULL);
}

static void stops_up_press_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_hold_active) return;
  reset_marquee_activity();
  s_hold_active = true;
  s_held_button = BUTTON_ID_UP;
  s_in_alphabet_mode = false;
  stops_move_selection(-1);
  s_hold_timer = app_timer_register(HOLD_INITIAL_REPEAT_MS,
                                    stops_hold_repeat_cb, NULL);
}

static void stops_up_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_held_button != BUTTON_ID_UP) return;
  finish_hold();
}

static void stops_down_press_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_hold_active) return;
  reset_marquee_activity();
  s_hold_active = true;
  s_held_button = BUTTON_ID_DOWN;
  s_in_alphabet_mode = false;
  stops_move_selection(1);
  s_hold_timer = app_timer_register(HOLD_INITIAL_REPEAT_MS,
                                    stops_hold_repeat_cb, NULL);
}

static void stops_down_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_held_button != BUTTON_ID_DOWN) return;
  finish_hold();
}

static void stops_select_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_stop_count == 0) return;
  reset_marquee_activity();
  open_stop_info_window(s_stop_selected);
}

static void stops_click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP,
                             stops_up_press_handler, stops_up_release_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN,
                             stops_down_press_handler, stops_down_release_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, stops_select_handler);
}

static void stops_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  if (!s_sv) { s_sv = calloc(1, sizeof(StopsView)); if (!s_sv) return; }
  s_sv->window = window;

  const int top_bar_height = 20;
  const int bar_h = AGENCY_BAR_H;
  int list_y = top_bar_height + bar_h;
  int list_h = bounds.size.h - list_y;

#if defined(PBL_PLATFORM_GABBRO) || defined(PBL_PLATFORM_EMERY)
  s_stop_rows_visible = 4;
#else
  s_stop_rows_visible = 3;
#endif
  s_stop_row_h = list_h / s_stop_rows_visible;
  s_stop_selected = 0;
  s_stop_first_visible = 0;
  s_stop_pending_first_visible = 0;
  s_stop_display_anchor = 0;
  s_stops_list_scroll_y = 0;
  s_stops_list_scroll_target = 0;

  const int right_margin = 4;
  s_stop_text_x = STOPS_TARGET_COL_W + 4;
  s_stop_text_w = bounds.size.w - s_stop_text_x - right_margin;
  if (s_stop_text_w < 1) s_stop_text_w = 1;

  if (s_have_location) compute_stop_distances();
  if (s_stop_count > 0) recompute_stop_layout();

  s_sv->top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar_height));
  layer_set_update_proc(s_sv->top_bar_layer, top_bar_draw);
  layer_add_child(root, s_sv->top_bar_layer);

  s_sv->clock_layer = text_layer_create(GRect(LEFT_MARGIN, 0, 80, top_bar_height));
  text_layer_set_text_color(s_sv->clock_layer, topbar_text_color());
  text_layer_set_background_color(s_sv->clock_layer, GColorClear);
  text_layer_set_font(s_sv->clock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_sv->clock_layer, GTextAlignmentLeft);
  {
#if SCREENSHOT_MODE
    text_layer_set_text(s_sv->clock_layer,
                        clock_is_24h_style() ? "12:34" : "12:34 PM");
#else
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    static char clock_buf[16];
    if (clock_is_24h_style()) strftime(clock_buf, sizeof(clock_buf), "%H:%M", t);
    else {
      strftime(clock_buf, sizeof(clock_buf), "%I:%M %p", t);
      if (clock_buf[0] == '0') memmove(clock_buf, clock_buf + 1, strlen(clock_buf));
    }
    text_layer_set_text(s_sv->clock_layer, clock_buf);
#endif
  }
  layer_add_child(root, text_layer_get_layer(s_sv->clock_layer));

  const int battery_w = 20, battery_h = 10, rm = 4, ic_size = 16, gap_ib = 4;
  int battery_x = bounds.size.w - rm - battery_w;
  int battery_y = (top_bar_height - battery_h) / 2;
  int icon_x = battery_x - gap_ib - ic_size;
  int icon_y = (top_bar_height - ic_size) / 2;

  s_sv->status_icon_layer = layer_create(GRect(icon_x, icon_y, ic_size, ic_size));
  layer_set_update_proc(s_sv->status_icon_layer, status_icon_update_proc);
  layer_add_child(root, s_sv->status_icon_layer);

  const int nl_gap  = 4;
  int nl_x = icon_x - nl_gap - ic_size;
  int nl_y = (top_bar_height - ic_size) / 2;
  s_sv->no_location_layer = layer_create(GRect(nl_x, nl_y, ic_size, ic_size));
  layer_set_update_proc(s_sv->no_location_layer, no_location_icon_update_proc);
  layer_add_child(root, s_sv->no_location_layer);
  layer_set_hidden(s_sv->no_location_layer, true);

  s_sv->battery_layer = layer_create(GRect(battery_x, battery_y, battery_w, battery_h));
  layer_set_update_proc(s_sv->battery_layer, battery_layer_update_proc);
  layer_add_child(root, s_sv->battery_layer);

  s_sv->route_bar_layer = layer_create(GRect(0, top_bar_height, bounds.size.w, bar_h));
  layer_set_update_proc(s_sv->route_bar_layer, stops_route_bar_update_proc);
  layer_add_child(root, s_sv->route_bar_layer);

  int route_icon_w = s_route_bitmap ? gbitmap_get_bounds(s_route_bitmap).size.w : 25;
  int route_name_x = LEFT_MARGIN + route_icon_w + 6;
  int route_name_w = bounds.size.w - route_name_x - 4;
  if (route_name_w < 1) route_name_w = 1;

  s_sv->route_name_auto = auto_scroll_text_layer_create(
    GRect(route_name_x, top_bar_height, route_name_w, bar_h));
  if (s_sv->route_name_auto) {
    s_sv->route_name_auto->font = fonts_get_system_font(FONT_ROUTE_NAME);
    s_sv->route_name_auto->left_align_when_fits = true;

    GColor route_bg;
    GColor route_fg;
    if (s_route_count > 0) {
      uint32_t rc = s_routes[s_route_selected].color;
      route_bg = GColorFromHEX(rc);
      route_fg = contrast_gcolor(rc);
    } else {
      route_bg = s_primary_color;
      route_fg = primary_text_color();
    }
    s_sv->route_name_auto->background_color = route_bg;
    s_sv->route_name_auto->text_color = route_fg;

    if (s_route_count > 0) {
      auto_scroll_text_layer_set_text(s_sv->route_name_auto,
                                      s_routes[s_route_selected].name);
    }
    layer_add_child(root, s_sv->route_name_auto->layer);
  }

  s_sv->list_layer = layer_create(GRect(0, list_y, bounds.size.w, list_h));
  layer_set_update_proc(s_sv->list_layer, stops_list_update_proc);
  layer_add_child(root, s_sv->list_layer);

  int text_layer_w = (s_stop_text_x - STOPS_TARGET_COL_W) + s_stop_text_w;
  if (text_layer_w < 1) text_layer_w = 1;
  s_sv->text_layer = layer_create(
    GRect(STOPS_TARGET_COL_W, list_y, text_layer_w, list_h));
  layer_set_update_proc(s_sv->text_layer, stops_text_update_proc);
  layer_add_child(root, s_sv->text_layer);

  if (!s_have_location) {
    s_location_pending = true;
    s_sv->fetch_location_timer = app_timer_register(300,
                                                    send_fetch_location_request, NULL);
    s_sv->location_timeout_timer = app_timer_register(20000,
                                                      location_timeout_cb, NULL);
  } else {
    s_location_pending = false;
  }

  window_set_click_config_provider(window, stops_click_config_provider);

  accel_tap_service_subscribe(tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_event_handler, NULL);
#endif

  start_stops_scroll_timer();
  reset_marquee_activity();
}

static void stops_window_unload(Window *window) {
  status_icon_set_loading(false);

  if (s_hold_timer) { app_timer_cancel(s_hold_timer); s_hold_timer = NULL; }
  s_hold_active = false;

  if (s_stops_scroll_timer) {
    app_timer_cancel(s_stops_scroll_timer);
    s_stops_scroll_timer = NULL;
  }
  if (s_stops_list_scroll_anim) {
    animation_unschedule(s_stops_list_scroll_anim);
    s_stops_list_scroll_anim = NULL;
  }

  accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif

  if (s_sv->no_location_layer) {
    layer_destroy(s_sv->no_location_layer);
    s_sv->no_location_layer = NULL;
  }

  if (s_no_location_flash_timer) {
    app_timer_cancel(s_no_location_flash_timer);
    s_no_location_flash_timer = NULL;
  }

  s_have_location = false;
  if (!s_sv) return;

  if (s_sv->fetch_location_timer) {
    app_timer_cancel(s_sv->fetch_location_timer);
    s_sv->fetch_location_timer = NULL;
  }
  if (s_sv->location_timeout_timer) {
    app_timer_cancel(s_sv->location_timeout_timer);
    s_sv->location_timeout_timer = NULL;
  }
  s_location_pending = false;
  if (s_sv->route_name_auto) {
    auto_scroll_text_layer_destroy(s_sv->route_name_auto);
    s_sv->route_name_auto = NULL;
  }
  if (s_sv->route_bar_layer) {
    layer_destroy(s_sv->route_bar_layer); s_sv->route_bar_layer = NULL;
  }
  if (s_sv->text_layer) {
    layer_destroy(s_sv->text_layer); s_sv->text_layer = NULL;
  }
  if (s_sv->clock_layer) {
    text_layer_destroy(s_sv->clock_layer); s_sv->clock_layer = NULL;
  }
  if (s_sv->status_icon_layer) {
    layer_destroy(s_sv->status_icon_layer); s_sv->status_icon_layer = NULL;
  }
  if (s_sv->battery_layer) {
    layer_destroy(s_sv->battery_layer); s_sv->battery_layer = NULL;
  }
  if (s_sv->top_bar_layer) {
    layer_destroy(s_sv->top_bar_layer); s_sv->top_bar_layer = NULL;
  }
  s_sv->window = NULL;

  if (s_stops)          { free(s_stops);          s_stops          = NULL; }
  if (s_stop_distances) { free(s_stop_distances); s_stop_distances = NULL; }

  s_stop_count            = 0;
  s_stop_capacity         = 0;
  s_stops_loaded          = false;
  s_stops_line_len        = 0;
  s_stops_chunks_received = 0;
  s_stops_chunks_total    = 0;
  s_stop_selected         = 0;
  s_stop_first_visible    = 0;
  s_stop_pending_first_visible = 0;
  s_stop_display_anchor   = 0;
  s_stops_list_scroll_y   = 0;
  s_stops_list_scroll_target = 0;
}

static int32_t parse_coord_scaled(const char *s, int len) {
  int i = 0, sign = 1;
  if (i < len && (s[i] == '-' || s[i] == '+')) { if (s[i] == '-') sign = -1; i++; }
  int32_t int_part = 0;
  while (i < len && s[i] >= '0' && s[i] <= '9') {
    int_part = int_part * 10 + (s[i] - '0'); i++;
  }
  int32_t frac = 0;
  int frac_digits = 0;
  if (i < len && s[i] == '.') {
    i++;
    while (i < len && s[i] >= '0' && s[i] <= '9' && frac_digits < 7) {
      frac = frac * 10 + (s[i] - '0'); frac_digits++; i++;
    }
    while (frac_digits < 7) { frac *= 10; frac_digits++; }
  }
  return sign * (int_part * COORD_SCALE + frac);
}

static void parse_stop_line(const char *line, int len) {
  if (s_stop_count >= MAX_STOPS) return;
  if (s_stop_count >= s_stop_capacity) {
    int new_cap = (s_stop_capacity == 0) ? 32 : s_stop_capacity * 2;
    if (new_cap > MAX_STOPS) new_cap = MAX_STOPS;
    StopEntry *grown = realloc(s_stops, new_cap * sizeof(StopEntry));
    if (!grown) return;
    s_stops = grown;
    s_stop_capacity = new_cap;
  }
  int semi1 = -1, semi2 = -1, semi3 = -1;
  for (int i = 0; i < len; i++) {
    if (line[i] == ';') {
      if      (semi1 < 0) semi1 = i;
      else if (semi2 < 0) semi2 = i;
      else { semi3 = i; break; }
    }
  }
  if (semi1 < 0 || semi2 < 0) return;
  StopEntry *s = &s_stops[s_stop_count];
  int name_len = semi1;
  if (name_len >= STOP_NAME_LEN) name_len = STOP_NAME_LEN - 1;
  memcpy(s->name, line, name_len);
  s->name[name_len] = '\0';
  s->lat = parse_coord_scaled(line + semi1 + 1, semi2 - semi1 - 1);
  s->lon = parse_coord_scaled(line + semi2 + 1,
                              (semi3 > 0 ? semi3 : len) - semi2 - 1);
  s->stop_id = 0;
  if (semi3 > 0) {
    const char *p = line + semi3 + 1;
    int32_t v = 0;
    while (p < line + len && *p >= '0' && *p <= '9') {
      v = v * 10 + (*p - '0'); p++;
    }
    s->stop_id = v;
  }
  s_stop_count++;
}

static void handle_stops_chunk(int32_t idx, int32_t total, const char *data) {
  if (!s_line_buf) {
    s_line_buf = malloc(LINE_BUF_SIZE);
    if (!s_line_buf) return;
  }
  if (idx == 0) {
    s_stop_count = 0;
    s_stops_line_len = 0;
    s_stops_chunks_received = 0;
    s_stops_chunks_total = total;
    s_stops_loaded = false;
    s_stop_selected = 0;
    s_stop_first_visible = 0;
    s_stop_pending_first_visible = 0;
    s_stop_display_anchor = 0;
    s_stops_list_scroll_y = 0;
    s_stops_list_scroll_target = 0;
    if (s_stop_distances) { free(s_stop_distances); s_stop_distances = NULL; }
    if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
  }
  if (idx != s_stops_chunks_received) return;
  const char *p = data;
  while (*p) {
    char c = *p++;
    if (c == '\n') {
      s_line_buf[s_stops_line_len] = '\0';
      if (s_stops_line_len > 0) parse_stop_line(s_line_buf, s_stops_line_len);
      s_stops_line_len = 0;
    } else if (s_stops_line_len < LINE_BUF_SIZE - 1) {
      s_line_buf[s_stops_line_len++] = c;
    } else {
      s_stops_line_len = 0;
    }
  }
  s_stops_chunks_received++;
  if (s_stops_chunks_received == s_stops_chunks_total) {
    if (s_stops_line_len > 0) {
      s_line_buf[s_stops_line_len] = '\0';
      parse_stop_line(s_line_buf, s_stops_line_len);
      s_stops_line_len = 0;
    }
    APP_LOG(APP_LOG_LEVEL_INFO, "Parsed %d stops", s_stop_count);
    s_stops_loaded = true;
    try_finish_stops_load();
  }
}

static void send_fetch_location_request(void *context) {
  (void)context;
  if (s_sv) s_sv->fetch_location_timer = NULL;
  if (!s_appmsg_ok) {
    s_location_pending = false;
    try_finish_stops_load();
    return;
  }
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_FETCH_LOCATION, 1);
    result = app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO, "Fetch location sent, result: %d", result);
  } else {
    if (s_sv) s_sv->fetch_location_timer = app_timer_register(
      500, send_fetch_location_request, NULL);
  }
}

static void send_fetch_stops_request(int32_t route_id) {
  if (!s_appmsg_ok) return;
  status_icon_set_loading(true);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_FETCH_STOPS, 1);
    dict_write_int32(iter, MESSAGE_KEY_ROUTE_ID, route_id);
    result = app_message_outbox_send();
    APP_LOG(APP_LOG_LEVEL_INFO,
            "Fetch stops request (route %d) sent, result: %d",
            (int)route_id, result);
  }
}

static void open_stops_window(int route_idx) {
  if (route_idx < 0 || route_idx >= s_route_count) return;
  int32_t route_id = s_routes[route_idx].route_id;
  if (route_id <= 0) return;
  s_stops_loaded = false;
  s_stop_count = 0;
  s_stops_chunks_received = 0;
  s_stops_chunks_total = 0;
  s_stops_line_len = 0;
  s_stop_selected = 0;
  s_stop_first_visible = 0;
  s_stop_pending_first_visible = 0;
  s_stop_display_anchor = 0;
  s_stops_list_scroll_y = 0;
  s_stops_list_scroll_target = 0;
  if (s_stop_distances) { free(s_stop_distances); s_stop_distances = NULL; }
  if (!s_sv) s_sv = calloc(1, sizeof(StopsView));
  if (s_sv && !s_sv->window) {
    s_sv->window = window_create();
    window_set_window_handlers(s_sv->window, (WindowHandlers){
      .load = stops_window_load, .unload = stops_window_unload
    });
  }
  if (!s_sv || !s_sv->window) return;
  send_fetch_stops_request(route_id);
  window_stack_push(s_sv->window, true);
}

static void update_clock(void) {
#if SCREENSHOT_MODE
  /* Fixed time for clean screenshots. Format follows the system 12/24h
     setting so the rendered width is representative. */
  static const char *fixed = "12:34";
  static const char *fixed_12 = "12:34 PM";
  const char *clock_buffer = clock_is_24h_style() ? fixed : fixed_12;
#else
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  static char clock_buf_storage[16];
  char *clock_buffer = clock_buf_storage;
  if (clock_is_24h_style()) {
    strftime(clock_buffer, sizeof(clock_buf_storage), "%H:%M", t);
  } else {
    strftime(clock_buffer, sizeof(clock_buf_storage), "%I:%M %p", t);
    if (clock_buffer[0] == '0')
      memmove(clock_buffer, clock_buffer + 1, strlen(clock_buffer));
  }
#endif

  if (s_clock_layer) text_layer_set_text(s_clock_layer, clock_buffer);
  if (s_routes_clock_layer) text_layer_set_text(s_routes_clock_layer, clock_buffer);
  if (s_sv && s_sv->clock_layer) text_layer_set_text(s_sv->clock_layer, clock_buffer);
  if (s_si && s_si->clock_layer) text_layer_set_text(s_si->clock_layer, clock_buffer);
  if (s_nv && s_nv->clock_layer) text_layer_set_text(s_nv->clock_layer, clock_buffer);
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) { update_clock(); }

/* =====================================================================
   Stop info screen
   ===================================================================== */

static void format_seconds(int32_t s, char *buf, size_t n) {
  if (s < 30)             snprintf(buf, n, "Now");
  else if (s < 60)        snprintf(buf, n, "%ds", (int)s);
  else if (s < 3600)      snprintf(buf, n, "%dm", (int)(s / 60));
  else if (s < 86400)     snprintf(buf, n, "%dh %dm",
                                   (int)(s / 3600), (int)((s % 3600) / 60));
  else                    snprintf(buf, n, ">1d");
}

static void si_header_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  GColor bg, fg;
  if (s_route_count > 0 && s_route_selected < s_route_count) {
    uint32_t rgb = s_routes[s_route_selected].color;
    bg = GColorFromHEX(rgb);
    fg = contrast_gcolor(rgb);
  } else {
    bg = s_primary_color;
    fg = primary_text_color();
  }

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  int target_cx = STOPS_TARGET_COL_W / 2;
  int cy = bounds.size.h / 2;
  graphics_context_set_stroke_color(ctx, fg);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(target_cx, 0), GPoint(target_cx, bounds.size.h));
  draw_stop_target(ctx, target_cx, cy, fg);

#if SI_SHOW_ICONS
  const int btn_w = AGENCY_BAR_H;
  const int btn_h = bounds.size.h;

  int pin_x  = bounds.size.w - btn_w;
  int bell_x = pin_x - btn_w;

  GColor bell_bg = (s_si_focus == SI_FOCUS_BELL) ? GColorWhite : GColorLightGray;
  GColor pin_bg  = (s_si_focus == SI_FOCUS_PIN)  ? GColorWhite : GColorLightGray;

  graphics_context_set_fill_color(ctx, bell_bg);
  graphics_fill_rect(ctx, GRect(bell_x, 0, btn_w, btn_h), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, pin_bg);
  graphics_fill_rect(ctx, GRect(pin_x,  0, btn_w, btn_h), 0, GCornerNone);

  if (s_bell_25_bitmap) {
    GSize sz = gbitmap_get_bounds(s_bell_25_bitmap).size;
    GRect r = GRect(bell_x + (btn_w - sz.w) / 2,
                    (btn_h - sz.h) / 2,
                    sz.w, sz.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_bell_25_bitmap, r);
  }
  if (s_pin_25_bitmap) {
    GSize sz = gbitmap_get_bounds(s_pin_25_bitmap).size;
    GRect r = GRect(pin_x + (btn_w - sz.w) / 2,
                    (btn_h - sz.h) / 2,
                    sz.w, sz.h);
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_pin_25_bitmap, r);
  }
#endif
}

static void si_header_text_update_proc(Layer *layer, GContext *ctx) {
  int row_h = SI_HEADER_H / 2;

  GColor fg;
  if (s_route_count > 0 && s_route_selected < s_route_count) {
    uint32_t rgb = s_routes[s_route_selected].color;
    fg = contrast_gcolor(rgb);
  } else {
    fg = primary_text_color();
  }

  GFont name_font = fonts_get_system_font(FONT_STOP_NAME);
  GFont dist_font = fonts_get_system_font(FONT_STOP_DIST);

  const int draw_w = 1000;

  graphics_context_set_text_color(ctx, fg);
  graphics_draw_text(ctx, s_si_stop_name, name_font,
                     GRect(-s_si_hdr_name_scroll, 2, draw_w, row_h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);

  char dist_buf[24];
  if (s_si_stop_distance_ft >= 0) {
    if (s_si_stop_distance_ft < 1000)
      snprintf(dist_buf, sizeof(dist_buf), "%d ft",
               (int)s_si_stop_distance_ft);
    else {
      int32_t tenths = (s_si_stop_distance_ft * 10 + 2640) / 5280;
      snprintf(dist_buf, sizeof(dist_buf), "%d.%d mi",
               (int)(tenths / 10), (int)(tenths % 10));
    }
  } else {
    snprintf(dist_buf, sizeof(dist_buf), "Distance unknown");
  }
  graphics_draw_text(ctx, dist_buf, dist_font,
                     GRect(-s_si_hdr_dist_scroll, row_h + 2, draw_w, row_h),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

static void si_list_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GFont msg_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  if (s_si_loading) {
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "Loading arrivals...", msg_font, bounds,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }
  if (s_si_bus_count == 0) {
    graphics_context_set_text_color(ctx, GColorDarkGray);
    graphics_draw_text(ctx, "No arrivals at this stop", msg_font, bounds,
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    return;
  }

#if defined(PBL_BW)
  GColor sel_bg = GColorBlack;
  GColor sel_fg = GColorWhite;
#else
  GColor sel_bg, sel_fg;
  if (s_route_count > 0 && s_route_selected < s_route_count) {
    uint32_t rgb = s_routes[s_route_selected].color;
    sel_bg = GColorFromHEX(rgb);
    sel_fg = contrast_gcolor(rgb);
  } else {
    sel_bg = s_primary_color;
    sel_fg = primary_text_color();
  }
#endif
  const GColor normal_fg = GColorBlack;

  int row_h  = s_si_row_h;
  int time_w = 56;

  for (int i = 0; i < s_si_bus_count; i++) {
    int y = i * row_h - s_si_scroll_y;
    if (y + row_h < 0 || y > bounds.size.h) continue;
    bool sel = (i == s_si_selected) && (s_si_focus == SI_FOCUS_LIST);

    GColor bg = sel ? sel_bg : GColorWhite;
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, row_h), 0, GCornerNone);

    if (s_bus_25_bitmap) {
      GSize sz = gbitmap_get_bounds(s_bus_25_bitmap).size;
      int icon_x = (STOPS_TARGET_COL_W - sz.w) / 2;
      int icon_y = y + (row_h - sz.h) / 2;
      GRect r = GRect(icon_x, icon_y, sz.w, sz.h);
      graphics_context_set_compositing_mode(ctx, GCompOpSet);
      graphics_draw_bitmap_in_rect(ctx, s_bus_25_bitmap, r);
    }

    BusEntry *b = &s_si_buses[i];
    char time_buf[16];
    format_seconds(b->seconds, time_buf, sizeof(time_buf));
    graphics_context_set_text_color(ctx, sel ? sel_fg : normal_fg);
    graphics_draw_text(ctx, time_buf, fonts_get_system_font(FONT_STOP_DIST),
                       GRect(bounds.size.w - time_w - 4, y, time_w, row_h),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentRight, NULL);
  }
}

static void si_text_update_proc(Layer *layer, GContext *ctx) {
  if (!s_si || s_si_bus_count == 0) return;

  GRect bounds = layer_get_bounds(layer);
  int row_h = s_si_row_h;
  const int local_text_x = SI_LEFT_PAD;

#if defined(PBL_BW)
  GColor sel_fg = GColorWhite;
#else
  GColor sel_fg;
  if (s_route_count > 0 && s_route_selected < s_route_count) {
    uint32_t rgb = s_routes[s_route_selected].color;
    sel_fg = contrast_gcolor(rgb);
  } else {
    sel_fg = primary_text_color();
  }
#endif
  const GColor normal_fg = GColorBlack;
  const GColor sub_fg    = GColorDarkGray;

  GFont name_font = fonts_get_system_font(FONT_STOP_NAME);
  GFont sub_font  = fonts_get_system_font(FONT_STOP_DIST);

  const int draw_w = 1000;

  for (int i = 0; i < s_si_bus_count; i++) {
    int y = i * row_h - s_si_scroll_y;
    if (y + row_h < 0 || y > bounds.size.h) continue;
    bool sel = (i == s_si_selected) && (s_si_focus == SI_FOCUS_LIST);

    int16_t scroll_name = s_si_scroll_offsets[i];
    int16_t scroll_sub  = s_si_sub_scroll_offsets[i];
    BusEntry *b = &s_si_buses[i];

    graphics_context_set_text_color(ctx, sel ? sel_fg : normal_fg);
    graphics_draw_text(ctx, b->name, name_font,
                       GRect(local_text_x - scroll_name, y + 2, draw_w, 20),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);

    char cap_buf[24];
    if (b->capacity_pct < 0)
      snprintf(cap_buf, sizeof(cap_buf), "Capacity unknown");
    else
      snprintf(cap_buf, sizeof(cap_buf), "%d%% full", b->capacity_pct);

    graphics_context_set_text_color(ctx, sel ? sel_fg : sub_fg);
    graphics_draw_text(ctx, cap_buf, sub_font,
                       GRect(local_text_x - scroll_sub, y + row_h - 18,
                             draw_w, 16),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

static void si_clamp_scroll(void) {
  int max = (s_si_bus_count > s_si_rows_visible)
            ? (s_si_bus_count - s_si_rows_visible) * s_si_row_h
            : 0;
  if (s_si_scroll_y > max) s_si_scroll_y = max;
  if (s_si_scroll_y < 0)   s_si_scroll_y = 0;
}

static void si_move_selection(int delta) {
  if (s_si_bus_count == 0) return;

  int new_sel = (int)s_si_selected + delta;
  if (new_sel < 0) new_sel = 0;
  if (new_sel >= s_si_bus_count) new_sel = s_si_bus_count - 1;
  s_si_selected = (uint8_t)new_sel;

  int sel_top  = s_si_selected * s_si_row_h;
  int sel_bot  = sel_top + s_si_row_h;
  int view_bot = s_si_scroll_y + s_si_rows_visible * s_si_row_h;
  if (sel_top < s_si_scroll_y) {
    s_si_scroll_y = (int16_t)sel_top;
  } else if (sel_bot > view_bot) {
    s_si_scroll_y = (int16_t)(sel_bot - s_si_rows_visible * s_si_row_h);
  }
  si_clamp_scroll();
  if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
  if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
}

static void si_up_handler(ClickRecognizerRef r, void *ctx) {
  reset_marquee_activity();

#if SI_SHOW_ICONS
  if (s_si_focus == SI_FOCUS_LIST) {
    if (s_si_selected > 0) { si_move_selection(-1); return; }
    s_si_focus = SI_FOCUS_PIN;
  } else if (s_si_focus == SI_FOCUS_PIN) {
    s_si_focus = SI_FOCUS_BELL;
  } else {
    s_si_focus = SI_FOCUS_LIST;
    if (s_si_bus_count > 0) {
      s_si_selected = s_si_bus_count - 1;
      int max = (s_si_bus_count > s_si_rows_visible)
                ? (s_si_bus_count - s_si_rows_visible) * s_si_row_h : 0;
      s_si_scroll_y = (int16_t)max;
    }
    if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
    if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
  }
  if (s_si && s_si->header_layer) layer_mark_dirty(s_si->header_layer);
#else
  if (s_si_bus_count == 0) return;
  if (s_si_selected > 0) {
    si_move_selection(-1);
    return;
  }
  s_si_selected = s_si_bus_count - 1;
  int max = (s_si_bus_count > s_si_rows_visible)
            ? (s_si_bus_count - s_si_rows_visible) * s_si_row_h : 0;
  s_si_scroll_y = (int16_t)max;
  if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
  if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
#endif
}

static void si_down_handler(ClickRecognizerRef r, void *ctx) {
  reset_marquee_activity();

#if SI_SHOW_ICONS
  if (s_si_focus == SI_FOCUS_LIST) {
    if (s_si_bus_count == 0) return;
    if (s_si_selected + 1 < s_si_bus_count) {
      si_move_selection(1);
    } else {
      s_si_selected = 0;
      s_si_scroll_y = 0;
      if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
      if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
    }
    return;
  } else if (s_si_focus == SI_FOCUS_BELL) {
    s_si_focus = SI_FOCUS_PIN;
  } else {
    s_si_focus = SI_FOCUS_LIST;
    s_si_selected = 0;
    s_si_scroll_y = 0;
    if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
    if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
  }
  if (s_si && s_si->header_layer) layer_mark_dirty(s_si->header_layer);
#else
  if (s_si_bus_count == 0) return;
  if (s_si_selected + 1 < s_si_bus_count) {
    si_move_selection(1);
    return;
  }
  s_si_selected = 0;
  s_si_scroll_y = 0;
  if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
  if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
#endif
}

static void si_send_arrival_fetch(void) {
  if (!s_appmsg_ok) return;
  if (s_si_stop_id <= 0) return;
  if (s_si_fetch_in_flight) return;
  if (!s_si || !s_si->window) return;

  status_icon_set_loading(true);
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(iter, MESSAGE_KEY_FETCH_STOP_ARRIVALS, 1);
  dict_write_int32(iter, MESSAGE_KEY_ROUTE_ID, s_si_route_id);
  dict_write_int32(iter, MESSAGE_KEY_STOP_ID,  s_si_stop_id);
  if (app_message_outbox_send() != APP_MSG_OK) {
    return;
  }
  s_si_fetch_in_flight = true;

  if (s_si_refresh_watchdog) {
    app_timer_cancel(s_si_refresh_watchdog);
    s_si_refresh_watchdog = NULL;
  }
  s_si_refresh_watchdog = app_timer_register(SI_REFRESH_WATCHDOG_MS,
                                             si_refresh_watchdog_cb, NULL);
}

static void si_schedule_refresh(void) {
  if (s_si_refresh_timer) return;
  if (!s_si || !s_si->window) return;

  uint32_t interval = s_marquee_idle ? SI_REFRESH_IDLE_S : SI_REFRESH_ACTIVE_S;
  for (int i = 0; i < s_si_refresh_failures && i < SI_REFRESH_FAIL_MAX; i++) {
    interval *= 2;
  }
  s_si_refresh_timer = app_timer_register(interval * 1000, si_refresh_cb, NULL);
}

static void si_refresh_cb(void *context) {
  s_si_refresh_timer = NULL;
  if (!s_si || !s_si->window) return;
  si_send_arrival_fetch();
  si_schedule_refresh();
}

static void si_refresh_watchdog_cb(void *context) {
  s_si_refresh_watchdog = NULL;
  if (!s_si_fetch_in_flight) return;
  s_si_fetch_in_flight = false;
  if (s_si_refresh_failures < 255) s_si_refresh_failures++;
  status_icon_set_loading(false);
  si_schedule_refresh();
}

static void si_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  if (!s_si) { s_si = calloc(1, sizeof(StopInfoView)); if (!s_si) return; }
  s_si->window = window;

  const int top_bar_height = 20;
  int list_y = top_bar_height + SI_HEADER_H;
  int list_h = bounds.size.h - list_y;

  s_si_row_h = SI_ROW_H;
  s_si_rows_visible = list_h / s_si_row_h;
  if (s_si_rows_visible < 1) s_si_rows_visible = 1;
  si_clamp_scroll();

  s_si->top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar_height));
  layer_set_update_proc(s_si->top_bar_layer, top_bar_draw);
  layer_add_child(root, s_si->top_bar_layer);

  s_si->clock_layer = text_layer_create(GRect(LEFT_MARGIN, 0, 80, top_bar_height));
  text_layer_set_text_color(s_si->clock_layer, topbar_text_color());
  text_layer_set_background_color(s_si->clock_layer, GColorClear);
  text_layer_set_font(s_si->clock_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_si->clock_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_si->clock_layer));
  update_clock();

  const int battery_w = 20, battery_h = 10, rm = 4, ic_size = 16, gap_ib = 4;
  int battery_x = bounds.size.w - rm - battery_w;
  int battery_y = (top_bar_height - battery_h) / 2;
  int icon_x = battery_x - gap_ib - ic_size;
  int icon_y = (top_bar_height - ic_size) / 2;

  s_si->status_icon_layer = layer_create(GRect(icon_x, icon_y, ic_size, ic_size));
  layer_set_update_proc(s_si->status_icon_layer, status_icon_update_proc);
  layer_add_child(root, s_si->status_icon_layer);

  s_si->battery_layer = layer_create(GRect(battery_x, battery_y, battery_w, battery_h));
  layer_set_update_proc(s_si->battery_layer, battery_layer_update_proc);
  layer_add_child(root, s_si->battery_layer);

  s_si->header_layer = layer_create(GRect(0, top_bar_height, bounds.size.w, SI_HEADER_H));
  layer_set_update_proc(s_si->header_layer, si_header_update_proc);
  layer_add_child(root, s_si->header_layer);

  {
    const int hdr_text_x = STOPS_TARGET_COL_W + SI_LEFT_PAD;
#if SI_SHOW_ICONS
    const int hdr_btn_w  = AGENCY_BAR_H;
    int hdr_text_right   = bounds.size.w - 2 * hdr_btn_w - 4;
#else
    int hdr_text_right   = bounds.size.w - 4;
#endif
    s_si_hdr_text_w = hdr_text_right - hdr_text_x;
    if (s_si_hdr_text_w < 1) s_si_hdr_text_w = 1;

    s_si->header_text_layer = layer_create(
      GRect(hdr_text_x, top_bar_height, s_si_hdr_text_w, SI_HEADER_H));
    layer_set_update_proc(s_si->header_text_layer, si_header_text_update_proc);
    layer_add_child(root, s_si->header_text_layer);
  }

  const int si_time_w = 56;
  s_si_text_w = bounds.size.w - (STOPS_TARGET_COL_W + SI_LEFT_PAD) - si_time_w - 4;
  if (s_si_text_w < 1) s_si_text_w = 1;

  s_si->list_layer = layer_create(GRect(0, list_y, bounds.size.w, list_h));
  layer_set_update_proc(s_si->list_layer, si_list_update_proc);
  layer_add_child(root, s_si->list_layer);

  s_si->text_layer = layer_create(
    GRect(STOPS_TARGET_COL_W, list_y, s_si_text_w, list_h));
  layer_set_update_proc(s_si->text_layer, si_text_update_proc);
  layer_add_child(root, s_si->text_layer);

  window_set_click_config_provider(window, si_click_config_provider);

  start_si_scroll_timer();
  reset_marquee_activity();
}

static void si_window_unload(Window *window) {
  status_icon_set_loading(false);

  if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);

  if (s_si->fetch_timer) {
    app_timer_cancel(s_si->fetch_timer);
    s_si->fetch_timer = NULL;
  }
  if (s_si_scroll_timer) {
    app_timer_cancel(s_si_scroll_timer);
    s_si_scroll_timer = NULL;
  }
  if (s_si_refresh_timer) {
    app_timer_cancel(s_si_refresh_timer);
    s_si_refresh_timer = NULL;
  }
  if (s_si_refresh_watchdog) {
    app_timer_cancel(s_si_refresh_watchdog);
    s_si_refresh_watchdog = NULL;
  }
  s_si_fetch_in_flight = false;

  if (s_si->top_bar_layer)     { layer_destroy(s_si->top_bar_layer);     s_si->top_bar_layer = NULL; }
  if (s_si->header_layer)      { layer_destroy(s_si->header_layer);      s_si->header_layer = NULL; }
  if (s_si->header_text_layer) { layer_destroy(s_si->header_text_layer); s_si->header_text_layer = NULL; }
  if (s_si->list_layer)        { layer_destroy(s_si->list_layer);        s_si->list_layer = NULL; }
  if (s_si->clock_layer)       { text_layer_destroy(s_si->clock_layer);  s_si->clock_layer = NULL; }
  if (s_si->status_icon_layer) { layer_destroy(s_si->status_icon_layer); s_si->status_icon_layer = NULL; }
  if (s_si->battery_layer)     { layer_destroy(s_si->battery_layer);     s_si->battery_layer = NULL; }
  if (s_si->text_layer) { layer_destroy(s_si->text_layer); s_si->text_layer = NULL; }
  s_si->window = NULL;

  if (s_si_buses) { free(s_si_buses); s_si_buses = NULL; }
  s_si_bus_count    = 0;
  s_si_bus_capacity = 0;
  s_si_loaded       = false;
  s_si_loading      = false;
  s_si_scroll_y     = 0;
}

static void recompute_si_header_layout(void) {
  if (s_si_hdr_text_w < 1) return;
  GFont name_font = fonts_get_system_font(FONT_STOP_NAME);
  GFont dist_font = fonts_get_system_font(FONT_STOP_DIST);

  GSize nsz = graphics_text_layout_get_content_size(
    s_si_stop_name, name_font, GRect(0, 0, 10000, 100),
    GTextOverflowModeFill, GTextAlignmentLeft);
  s_si_hdr_name_max = (nsz.w > s_si_hdr_text_w) ? (nsz.w - s_si_hdr_text_w) : 0;
  s_si_hdr_name_scroll = 0;
  s_si_hdr_name_dir = 1;
  s_si_hdr_name_pause = STOPS_SCROLL_PAUSE_TICKS;

  char dist_buf[24];
  if (s_si_stop_distance_ft >= 0) {
    if (s_si_stop_distance_ft < 1000)
      snprintf(dist_buf, sizeof(dist_buf), "%d ft", (int)s_si_stop_distance_ft);
    else {
      int32_t tenths = (s_si_stop_distance_ft * 10 + 2640) / 5280;
      snprintf(dist_buf, sizeof(dist_buf), "%d.%d mi",
               (int)(tenths / 10), (int)(tenths % 10));
    }
  } else {
    snprintf(dist_buf, sizeof(dist_buf), "Distance unknown");
  }
  GSize dsz = graphics_text_layout_get_content_size(
    dist_buf, dist_font, GRect(0, 0, 10000, 100),
    GTextOverflowModeFill, GTextAlignmentLeft);
  s_si_hdr_dist_max = (dsz.w > s_si_hdr_text_w) ? (dsz.w - s_si_hdr_text_w) : 0;
  s_si_hdr_dist_scroll = 0;
  s_si_hdr_dist_dir = 1;
  s_si_hdr_dist_pause = STOPS_SCROLL_PAUSE_TICKS;
}

static void recompute_si_layout(void) {
  if (s_si_text_w < 1) return;
  GFont name_font = fonts_get_system_font(FONT_STOP_NAME);
  GFont sub_font  = fonts_get_system_font(FONT_STOP_DIST);

  int eff_w = s_si_text_w - SI_LEFT_PAD;
  if (eff_w < 1) eff_w = 1;

  for (int i = 0; i < s_si_bus_count; i++) {
    BusEntry *b = &s_si_buses[i];

    GSize nsz = graphics_text_layout_get_content_size(
      b->name, name_font, GRect(0, 0, 10000, 100),
      GTextOverflowModeFill, GTextAlignmentLeft);
    s_si_max_scrolls[i] = (nsz.w > eff_w) ? (nsz.w - eff_w) : 0;
    s_si_scroll_offsets[i] = 0;
    s_si_scroll_dirs[i] = 1;
    s_si_pause_ticks[i] = STOPS_SCROLL_PAUSE_TICKS;

    char sub_buf[24];
    if (b->capacity_pct < 0)
      snprintf(sub_buf, sizeof(sub_buf), "Capacity unknown");
    else
      snprintf(sub_buf, sizeof(sub_buf), "%d%% full", b->capacity_pct);

    GSize ssz = graphics_text_layout_get_content_size(
      sub_buf, sub_font, GRect(0, 0, 10000, 100),
      GTextOverflowModeFill, GTextAlignmentLeft);
    s_si_sub_max_scrolls[i] = (ssz.w > eff_w) ? (ssz.w - eff_w) : 0;
    s_si_sub_scroll_offsets[i] = 0;
    s_si_sub_scroll_dirs[i] = 1;
    s_si_sub_pause_ticks[i] = STOPS_SCROLL_PAUSE_TICKS;
  }

  recompute_si_header_layout();
}

static void si_scroll_tick(void *context) {
  s_si_scroll_timer = NULL;
#if SCREENSHOT_MODE
  return;
#else
  if (!s_si || !s_si->window) return;
  if (s_si_bus_count == 0 && s_si_hdr_name_max <= 0 && s_si_hdr_dist_max <= 0)
    return;

  bool any_dirty = false;
  for (int i = 0; i < s_si_bus_count; i++) {
    if (advance_scroll(i, s_si_scroll_offsets, s_si_max_scrolls,
                       s_si_scroll_dirs, s_si_pause_ticks)) any_dirty = true;
    if (advance_scroll(i, s_si_sub_scroll_offsets, s_si_sub_max_scrolls,
                       s_si_sub_scroll_dirs, s_si_sub_pause_ticks)) any_dirty = true;
  }
  if (any_dirty && s_si->text_layer) layer_mark_dirty(s_si->text_layer);

  bool hdr_dirty = false;
  if (advance_scroll_one(&s_si_hdr_name_scroll, &s_si_hdr_name_max,
                         &s_si_hdr_name_dir, &s_si_hdr_name_pause)) {
    hdr_dirty = true;
  }
  if (advance_scroll_one(&s_si_hdr_dist_scroll, &s_si_hdr_dist_max,
                         &s_si_hdr_dist_dir, &s_si_hdr_dist_pause)) {
    hdr_dirty = true;
  }
  if (hdr_dirty && s_si->header_text_layer)
    layer_mark_dirty(s_si->header_text_layer);

  if (s_marquee_idle) {
    bool any_moving = false;
    for (int i = 0; i < s_si_bus_count; i++) {
      if (s_si_max_scrolls[i] > 0 && s_si_scroll_offsets[i] != 0) {
        any_moving = true; break;
      }
      if (s_si_sub_max_scrolls[i] > 0 && s_si_sub_scroll_offsets[i] != 0) {
        any_moving = true; break;
      }
    }
    if (s_si_hdr_name_max > 0 && s_si_hdr_name_scroll != 0) any_moving = true;
    if (s_si_hdr_dist_max > 0 && s_si_hdr_dist_scroll != 0) any_moving = true;
    if (!any_moving) return;
  }
  s_si_scroll_timer = app_timer_register(STOPS_SCROLL_TICK_MS,
                                         si_scroll_tick, NULL);
#endif
}

static void start_si_scroll_timer(void) {
#if SCREENSHOT_MODE
  return;
#endif
  if (s_si_scroll_timer) return;
  if (!s_si || !s_si->window) return;
  if (s_si_bus_count == 0 && s_si_hdr_name_max <= 0 && s_si_hdr_dist_max <= 0)
    return;
  bool any_scroll = false;
  for (int i = 0; i < s_si_bus_count; i++) {
    if (s_si_max_scrolls[i] > 0 || s_si_sub_max_scrolls[i] > 0) {
      any_scroll = true; break;
    }
  }
  if (s_si_hdr_name_max > 0 || s_si_hdr_dist_max > 0) any_scroll = true;
  if (!any_scroll) return;
  s_si_scroll_timer = app_timer_register(STOPS_SCROLL_TICK_MS,
                                         si_scroll_tick, NULL);
}

static void try_finish_si_load(void) {
  s_si_fetch_in_flight = false;
  s_si_refresh_failures = 0;
  s_si_last_fetch_time = time(NULL);
  if (s_si_refresh_watchdog) {
    app_timer_cancel(s_si_refresh_watchdog);
    s_si_refresh_watchdog = NULL;
  }

  s_si_loaded = true;
  s_si_loading = false;
  status_icon_set_loading(false);
  si_clamp_scroll();
  recompute_si_layout();
  if (s_si && s_si->list_layer) layer_mark_dirty(s_si->list_layer);
  if (s_si && s_si->text_layer) layer_mark_dirty(s_si->text_layer);
  if (s_si && s_si->header_text_layer) layer_mark_dirty(s_si->header_text_layer);
  if (s_si && s_si->header_layer) layer_mark_dirty(s_si->header_layer);
  start_si_scroll_timer();
}

static void si_append_bus(const char *line, int len) {
  if (s_si_bus_count >= SI_MAX_BUSES) return;

  if (s_si_bus_count >= s_si_bus_capacity) {
    int new_cap = (s_si_bus_capacity == 0) ? 8 : s_si_bus_capacity * 2;
    BusEntry *grown = realloc(s_si_buses, new_cap * sizeof(BusEntry));
    if (!grown) return;
    s_si_buses = grown;
    s_si_bus_capacity = new_cap;
  }
  BusEntry *b = &s_si_buses[s_si_bus_count];
  memset(b, 0, sizeof(*b));
  b->capacity_pct = -1;

  int bar1 = -1, bar2 = -1;
  for (int i = 0; i < len; i++) {
    if (line[i] == '|') {
      if (bar1 < 0) bar1 = i;
      else { bar2 = i; break; }
    }
  }
  if (bar1 < 0 || bar2 < 0) return;

  int name_len = bar1;
  if (name_len >= (int)sizeof(b->name)) name_len = sizeof(b->name) - 1;
  memcpy(b->name, line, name_len);
  b->name[name_len] = '\0';

  int32_t sec = 0;
  for (int i = bar1 + 1; i < bar2; i++) {
    if (line[i] >= '0' && line[i] <= '9') sec = sec * 10 + (line[i] - '0');
  }
  b->seconds = sec;

  int neg = 0, val = 0;
  for (int i = bar2 + 1; i < len; i++) {
    if (line[i] == '-') { neg = 1; continue; }
    if (line[i] >= '0' && line[i] <= '9') val = val * 10 + (line[i] - '0');
  }
  b->capacity_pct = (int8_t)(neg ? -1 : val);

  s_si_bus_count++;
}

static void si_handle_arrival_chunk(int32_t idx, int32_t total, const char *data) {
  if (idx == 0) {
    s_si_bus_count = 0;
    s_si_line_len = 0;
    s_si_chunks_received = 0;
    s_si_chunks_total = (uint8_t)total;
  }
  if (idx != s_si_chunks_received) return;
  const char *p = data;
  while (*p) {
    char c = *p++;
    if (c == '\n') {
      s_si_line_buf[s_si_line_len] = '\0';
      if (s_si_line_len > 0) si_append_bus(s_si_line_buf, s_si_line_len);
      s_si_line_len = 0;
    } else if (s_si_line_len < sizeof(s_si_line_buf) - 1) {
      s_si_line_buf[s_si_line_len++] = c;
    } else {
      s_si_line_len = 0;
    }
  }
  s_si_chunks_received++;
  if (s_si_chunks_received == s_si_chunks_total) {
    if (s_si_line_len > 0) {
      s_si_line_buf[s_si_line_len] = '\0';
      si_append_bus(s_si_line_buf, s_si_line_len);
      s_si_line_len = 0;
    }
    try_finish_si_load();
  }
}

static void open_stop_info_window(int display_idx) {
  if (s_stop_count == 0) return;
  int array_idx = (s_stop_display_anchor + display_idx) % s_stop_count;

  s_si_stop_display_idx = (uint8_t)display_idx;
  s_si_stop_array_idx   = (uint8_t)array_idx;
  s_si_route_id  = (s_route_count > 0) ? s_routes[s_route_selected].route_id : 0;
  s_si_stop_id   = s_stops[array_idx].stop_id;
  strncpy(s_si_stop_name, s_stops[array_idx].name, sizeof(s_si_stop_name) - 1);
  s_si_stop_name[sizeof(s_si_stop_name) - 1] = '\0';
  s_si_stop_distance_ft = -1;
  if (s_have_location && s_stop_distances) {
    s_si_stop_distance_ft = s_stop_distances[array_idx];
  }

  if (s_si_buses) { free(s_si_buses); s_si_buses = NULL; }
  s_si_bus_count    = 0;
  s_si_bus_capacity = 0;
  s_si_scroll_y     = 0;
  s_si_focus        = SI_FOCUS_LIST;
  s_si_selected     = 0;
  s_si_line_len     = 0;
  s_si_chunks_received = 0;
  s_si_chunks_total    = 0;
  s_si_hdr_name_scroll = 0;
  s_si_hdr_name_max = 0;
  s_si_hdr_dist_scroll = 0;
  s_si_hdr_dist_max = 0;
  s_si_loaded  = false;
  s_si_loading = true;
  if (s_si_refresh_timer) {
    app_timer_cancel(s_si_refresh_timer); s_si_refresh_timer = NULL;
  }
  if (s_si_refresh_watchdog) {
    app_timer_cancel(s_si_refresh_watchdog); s_si_refresh_watchdog = NULL;
  }
  s_si_fetch_in_flight  = false;
  s_si_refresh_failures = 0;
  s_si_last_fetch_time  = 0;

  if (!s_si) s_si = calloc(1, sizeof(StopInfoView));
  if (s_si && !s_si->window) {
    s_si->window = window_create();
    window_set_window_handlers(s_si->window, (WindowHandlers){
      .load   = si_window_load,
      .unload = si_window_unload
    });
  }
  if (!s_si || !s_si->window) return;

  si_send_arrival_fetch();
  si_schedule_refresh();

  window_stack_push(s_si->window, true);
}

/* =====================================================================
   Notification interval screen
   ===================================================================== */

static uint8_t notif_extended_count(void) {
  if (s_si_stop_id <= 0) return NOTIF_OPTION_COUNT;
  StopSetting *s = find_stop_setting(s_si_stop_id, false);
  if (s && (s->flags & STOP_SETTING_BELL) && s->bell_minutes > 0)
    return NOTIF_OPTION_COUNT + 1;
  return NOTIF_OPTION_COUNT;
}

static uint8_t notif_value_at(int idx) {
  int cnt = notif_extended_count();
  if (cnt == NOTIF_OPTION_COUNT + 1) {
    if (idx <= 0) return 0;
    if (idx > NOTIF_OPTION_COUNT) idx = NOTIF_OPTION_COUNT;
    return NOTIF_OPTIONS[idx - 1];
  }
  if (idx < 0) idx = 0;
  if (idx >= NOTIF_OPTION_COUNT) idx = NOTIF_OPTION_COUNT - 1;
  return NOTIF_OPTIONS[idx];
}

static void notif_label_at(int idx, char *buf, size_t n) {
  uint8_t v = notif_value_at(idx);
  if (v == 0) snprintf(buf, n, "Disable");
  else snprintf(buf, n, "%d mins.", v);
}

static void notif_left_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

#if defined(PBL_BW)
  GColor bg = GColorLightGray;
  GColor fg = GColorBlack;
#else
  uint32_t rgb = 0;
  if (s_route_count > 0 && s_route_selected < s_route_count)
    rgb = s_routes[s_route_selected].color;
  GColor bg = rgb ? GColorFromHEX(rgb) : s_primary_color;
  GColor fg = rgb ? contrast_gcolor(rgb) : primary_text_color();
#endif

  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GFont bold_font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);

  bool disabled = (notif_value_at(s_notif_selected) == 0);

  const char *lines[4];
  int line_count;
  char line_mins[16];

  if (disabled) {
    lines[0] = "Disable";
    lines[1] = "Alert";
    line_count = 2;
  } else {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    snprintf(line_mins, sizeof(line_mins), "%d mins. away",
             notif_value_at(s_notif_selected));
    lines[0] = "Notify when";
    lines[1] = line_mins;
    line_count = 2;
#else
    snprintf(line_mins, sizeof(line_mins), "%d mins.",
             notif_value_at(s_notif_selected));
    lines[0] = "Notify";
    lines[1] = "when";
    lines[2] = line_mins;
    lines[3] = "away";
    line_count = 4;
#endif
  }

  int line_h = 0;
  for (int i = 0; i < line_count; i++) {
    GSize sz = graphics_text_layout_get_content_size(
      lines[i], bold_font, GRect(0, 0, bounds.size.w, 200),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
    if (sz.h > line_h) line_h = sz.h;
  }
  int text_h = line_h * line_count;

  int icon_w = 0, icon_h = 0;
  if (s_bell_80_bitmap) {
    GSize sz = gbitmap_get_bounds(s_bell_80_bitmap).size;
    int max_w = bounds.size.w - 16;
    icon_w = sz.w;
    icon_h = sz.h;
    if (icon_w > max_w) {
      icon_h = (sz.h * max_w) / sz.w;
      icon_w = max_w;
    }
  }

  const int gap = 6;
  int group_h = icon_h + gap + text_h;
  int group_y = (bounds.size.h - group_h) / 2;
  if (group_y < 2) group_y = 2;

  if (s_bell_80_bitmap) {
    int icon_x = (bounds.size.w - icon_w) / 2;
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_bell_80_bitmap,
                                 GRect(icon_x, group_y, icon_w, icon_h));
  }

  int text_y = group_y + icon_h + gap;
  graphics_context_set_text_color(ctx, fg);

  for (int i = 0; i < line_count; i++) {
    graphics_draw_text(ctx, lines[i], bold_font,
                       GRect(0, text_y + i * line_h, bounds.size.w, line_h),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void notif_list_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GFont row_font = fonts_get_system_font(FONT_STOP_NAME);

  int text_y_off = NOTIF_TEXT_INSET_Y;
  int count = notif_extended_count();
  int center_baseline = bounds.size.h / 2 - NOTIF_ROW_H / 2;
  int rows_each_side  = (bounds.size.h / NOTIF_ROW_H) / 2 + 2;

  graphics_context_set_text_color(ctx, GColorDarkGray);

  for (int o = -rows_each_side; o <= rows_each_side; o++) {
    int idx = ((int)s_notif_selected + o + count) % count;

    int y = center_baseline + o * NOTIF_ROW_H + s_notif_scroll_y;
    if (y + NOTIF_ROW_H <= 0 || y >= bounds.size.h) continue;

    char buf[12];
    notif_label_at(idx, buf, sizeof(buf));

    graphics_draw_text(ctx, buf, row_font,
                       GRect(0, y + text_y_off, bounds.size.w, NOTIF_ROW_H),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void notif_hl_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

#if defined(PBL_BW)
  GColor hl_bg = GColorLightGray;
  GColor hl_fg = GColorBlack;
#else
  uint32_t rgb = 0;
  if (s_route_count > 0 && s_route_selected < s_route_count)
    rgb = s_routes[s_route_selected].color;
  GColor hl_bg = rgb ? GColorFromHEX(rgb) : s_primary_color;
  GColor hl_fg = rgb ? contrast_gcolor(rgb) : primary_text_color();
#endif

  graphics_context_set_fill_color(ctx, hl_bg);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  GFont row_font = fonts_get_system_font(FONT_STOP_NAME);

  int text_y_off = NOTIF_TEXT_INSET_Y;

  GRect h_frame = layer_get_frame(layer);
  int hl_y_local = h_frame.origin.y;
  int list_h = layer_get_frame(s_nv->list_layer).size.h;
  int center_baseline = list_h / 2 - NOTIF_ROW_H / 2;

  int count = notif_extended_count();
  int rows_each_side = (list_h / NOTIF_ROW_H) / 2 + 2;

  graphics_context_set_text_color(ctx, hl_fg);

  for (int o = -rows_each_side; o <= rows_each_side; o++) {
    int y_list = center_baseline + o * NOTIF_ROW_H + s_notif_scroll_y;

    if (y_list + NOTIF_ROW_H <= hl_y_local) continue;
    if (y_list >= hl_y_local + bounds.size.h) continue;

    int idx = ((int)s_notif_selected + o + count) % count;

    char buf[12];
    notif_label_at(idx, buf, sizeof(buf));

    int y_local = y_list - hl_y_local;

    graphics_draw_text(ctx, buf, row_font,
                       GRect(0, y_local + text_y_off,
                             bounds.size.w, NOTIF_ROW_H),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
  }
}

static void commit_pending_notif_scroll(void) {
  if (!s_notif_scroll_anim) return;
  Animation *anim = s_notif_scroll_anim;
  s_notif_scroll_anim = NULL;
  animation_unschedule(anim);
  s_notif_selected = s_notif_pending_selected;
  s_notif_scroll_y = 0;
  s_notif_scroll_target = 0;
}

static void notif_scroll_anim_update(Animation *anim, AnimationProgress progress) {
  int32_t target = s_notif_scroll_target;
  s_notif_scroll_y = (int16_t)(((int64_t)target * (int64_t)progress)
                               / ANIMATION_NORMALIZED_MAX);
  if (s_nv && s_nv->list_layer) layer_mark_dirty(s_nv->list_layer);
  if (s_nv && s_nv->hl_layer) layer_mark_dirty(s_nv->hl_layer);
}

static void notif_scroll_anim_teardown(Animation *anim) {
  s_notif_selected = s_notif_pending_selected;
  s_notif_scroll_y = 0;
  s_notif_scroll_target = 0;
  s_notif_scroll_anim = NULL;
  if (s_nv && s_nv->list_layer) layer_mark_dirty(s_nv->list_layer);
  if (s_nv && s_nv->hl_layer) layer_mark_dirty(s_nv->hl_layer);
  if (s_nv && s_nv->left_layer) layer_mark_dirty(s_nv->left_layer);
}

static void start_notif_scroll_anim(int sel_delta) {
  commit_pending_notif_scroll();

  int count = notif_extended_count();
  int new_sel = ((int)s_notif_selected + sel_delta + count) % count;
  s_notif_pending_selected = (uint8_t)new_sel;

  s_notif_scroll_y = 0;
  s_notif_scroll_target = (int16_t)(-sel_delta * NOTIF_ROW_H);

  static const AnimationImplementation impl = {
    .update = notif_scroll_anim_update,
    .teardown = notif_scroll_anim_teardown
  };
  s_notif_scroll_anim = animation_create();
  animation_set_implementation(s_notif_scroll_anim, &impl);
  animation_set_duration(s_notif_scroll_anim, NOTIF_SCROLL_ANIM_DURATION_MS);
  animation_set_curve(s_notif_scroll_anim, AnimationCurveEaseInOut);
  animation_schedule(s_notif_scroll_anim);
}

static void notif_up_handler(ClickRecognizerRef r, void *ctx) {
  reset_marquee_activity();
  start_notif_scroll_anim(-1);
}

static void notif_down_handler(ClickRecognizerRef r, void *ctx) {
  reset_marquee_activity();
  start_notif_scroll_anim(+1);
}

static void notif_select_handler(ClickRecognizerRef r, void *ctx) {
  commit_pending_notif_scroll();
  if (s_si_stop_id > 0) {
    uint8_t v = notif_value_at(s_notif_selected);

    if (v == 0) {
      StopSetting *s = find_stop_setting(s_si_stop_id, false);
      if (s) {
        s->flags &= ~STOP_SETTING_BELL;
        s->bell_minutes = 0;
        if (s->flags == 0) {
          int idx = (int)(s - s_stop_settings);
          for (int i = idx; i < s_stop_settings_count - 1; i++) {
            s_stop_settings[i] = s_stop_settings[i + 1];
          }
          s_stop_settings_count--;
        }
        save_stop_settings();
      }
    } else {
      StopSetting *s = find_stop_setting(s_si_stop_id, true);
      if (s) {
        s->flags |= STOP_SETTING_BELL;
        s->bell_minutes = v;
        save_stop_settings();
      }
    }

    vibes_short_pulse();
    if (s_si && s_si->header_layer) layer_mark_dirty(s_si->header_layer);
    if (s_sv && s_sv->list_layer) layer_mark_dirty(s_sv->list_layer);
  }
  if (s_nv && s_nv->window) window_stack_remove(s_nv->window, true);
}

static void notif_click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP,   100, notif_up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, notif_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, notif_select_handler);
}

static void notif_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  if (!s_nv) { s_nv = calloc(1, sizeof(NotifView)); if (!s_nv) return; }
  s_nv->window = window;

  const int top_bar_height = 20;
  int content_y = top_bar_height;
  int content_h = bounds.size.h - content_y;
  int half_w    = bounds.size.w / 2;
  int right_w   = bounds.size.w - half_w;

  s_nv->top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar_height));
  layer_set_update_proc(s_nv->top_bar_layer, top_bar_draw);
  layer_add_child(root, s_nv->top_bar_layer);

  s_nv->clock_layer = text_layer_create(GRect(LEFT_MARGIN, 0, 80, top_bar_height));
  text_layer_set_text_color(s_nv->clock_layer, topbar_text_color());
  text_layer_set_background_color(s_nv->clock_layer, GColorClear);
  text_layer_set_font(s_nv->clock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_nv->clock_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_nv->clock_layer));
  update_clock();

  const int battery_w = 20, battery_h = 10, rm = 4, ic_size = 16, gap_ib = 4;
  int battery_x = bounds.size.w - rm - battery_w;
  int battery_y = (top_bar_height - battery_h) / 2;
  int icon_x = battery_x - gap_ib - ic_size;
  int icon_y = (top_bar_height - ic_size) / 2;

  s_nv->status_icon_layer = layer_create(GRect(icon_x, icon_y, ic_size, ic_size));
  layer_set_update_proc(s_nv->status_icon_layer, status_icon_update_proc);
  layer_add_child(root, s_nv->status_icon_layer);

  s_nv->battery_layer = layer_create(GRect(battery_x, battery_y, battery_w, battery_h));
  layer_set_update_proc(s_nv->battery_layer, battery_layer_update_proc);
  layer_add_child(root, s_nv->battery_layer);

  s_nv->left_layer = layer_create(GRect(0, content_y, half_w, content_h));
  layer_set_update_proc(s_nv->left_layer, notif_left_update_proc);
  layer_add_child(root, s_nv->left_layer);

  s_nv->list_layer = layer_create(
    GRect(half_w, content_y, right_w, content_h));
  layer_set_update_proc(s_nv->list_layer, notif_list_update_proc);
  layer_add_child(root, s_nv->list_layer);

  int hl_y = content_h / 2 - NOTIF_ROW_H / 2;
  s_nv->hl_layer = layer_create(GRect(0, hl_y, right_w, NOTIF_ROW_H));
  layer_set_update_proc(s_nv->hl_layer, notif_hl_layer_update_proc);
  layer_add_child(s_nv->list_layer, s_nv->hl_layer);

  window_set_click_config_provider(window, notif_click_config_provider);
}

static void notif_window_unload(Window *window) {
  if (s_notif_scroll_anim) {
    animation_unschedule(s_notif_scroll_anim);
    s_notif_scroll_anim = NULL;
  }
  if (s_nv->hl_layer)          { layer_destroy(s_nv->hl_layer);          s_nv->hl_layer = NULL; }
  if (s_nv->list_layer)        { layer_destroy(s_nv->list_layer);        s_nv->list_layer = NULL; }
  if (s_nv->left_layer)        { layer_destroy(s_nv->left_layer);        s_nv->left_layer = NULL; }
  if (s_nv->top_bar_layer)     { layer_destroy(s_nv->top_bar_layer);     s_nv->top_bar_layer = NULL; }
  if (s_nv->clock_layer)       { text_layer_destroy(s_nv->clock_layer);  s_nv->clock_layer = NULL; }
  if (s_nv->status_icon_layer) { layer_destroy(s_nv->status_icon_layer); s_nv->status_icon_layer = NULL; }
  if (s_nv->battery_layer)     { layer_destroy(s_nv->battery_layer);     s_nv->battery_layer = NULL; }
  s_nv->window = NULL;
}

static void open_notif_window(void) {
  s_notif_selected = 0;
  if (s_si_stop_id > 0) {
    StopSetting *s = find_stop_setting(s_si_stop_id, false);
    int cur = (s && (s->flags & STOP_SETTING_BELL)) ? s->bell_minutes : 0;
    if (cur > 0 && notif_extended_count() == NOTIF_OPTION_COUNT + 1) {
      for (int i = 0; i < NOTIF_OPTION_COUNT; i++) {
        if (NOTIF_OPTIONS[i] == cur) { s_notif_selected = i + 1; break; }
      }
    }
  }

  if (s_notif_scroll_anim) {
    animation_unschedule(s_notif_scroll_anim);
    s_notif_scroll_anim = NULL;
  }
  s_notif_scroll_y = 0;
  s_notif_scroll_target = 0;
  s_notif_pending_selected = s_notif_selected;

  if (!s_nv) s_nv = calloc(1, sizeof(NotifView));
  if (s_nv && !s_nv->window) {
    s_nv->window = window_create();
    window_set_window_handlers(s_nv->window, (WindowHandlers){
      .load   = notif_window_load,
      .unload = notif_window_unload
    });
  }
  if (!s_nv || !s_nv->window) return;
  window_stack_push(s_nv->window, true);
}

static void load_saved_agency(void) {
  s_has_saved_agency = persist_read_bool(PERSIST_KEY_HAS_AGENCY);
  if (s_has_saved_agency) {
    persist_read_string(PERSIST_KEY_AGENCY_NAME, s_saved_name, sizeof(s_saved_name));
    persist_read_string(PERSIST_KEY_AGENCY_DOMAIN, s_saved_domain, sizeof(s_saved_domain));
    int color_val = persist_read_int(PERSIST_KEY_AGENCY_COLOR);
    s_primary_color = GColorFromHEX((uint32_t)color_val);
  } else {
    s_primary_color = GColorBlack;
  }
}

static void save_agency(const char *name, const char *domain) {
  strncpy(s_saved_name, name, sizeof(s_saved_name) - 1);
  s_saved_name[sizeof(s_saved_name) - 1] = '\0';
  strncpy(s_saved_domain, domain, sizeof(s_saved_domain) - 1);
  s_saved_domain[sizeof(s_saved_domain) - 1] = '\0';
  s_has_saved_agency = true;
  persist_write_bool(PERSIST_KEY_HAS_AGENCY, true);
  persist_write_string(PERSIST_KEY_AGENCY_NAME, s_saved_name);
  persist_write_string(PERSIST_KEY_AGENCY_DOMAIN, s_saved_domain);
  persist_write_int(PERSIST_KEY_AGENCY_COLOR, 0x000000);
  persist_write_string(PERSIST_KEY_AGENCY_APIKEY, "");
}

static void save_agency_config(const char *apikey, uint32_t color_val) {
  (void)apikey;
  s_primary_color = GColorFromHEX(color_val);
  persist_write_int(PERSIST_KEY_AGENCY_COLOR, (int)color_val);

  if (s_name) s_name->text_color = primary_text_color();
}

static void clear_saved_agency(void) {
  s_has_saved_agency = false;
  s_saved_name[0] = '\0';
  s_saved_domain[0] = '\0';
  s_primary_color = GColorBlack;
  persist_delete(PERSIST_KEY_HAS_AGENCY);
  persist_delete(PERSIST_KEY_AGENCY_NAME);
  persist_delete(PERSIST_KEY_AGENCY_DOMAIN);
  persist_delete(PERSIST_KEY_AGENCY_APIKEY);
  persist_delete(PERSIST_KEY_AGENCY_COLOR);
}

static void hide_selection(void) {
  layer_set_hidden(text_layer_get_layer(s_title_layer), true);
  if (s_name) layer_set_hidden(s_name->layer, true);
  if (s_domain) layer_set_hidden(s_domain->layer, true);
  layer_set_hidden(s_up_arrow_graphics, true);
  layer_set_hidden(s_down_arrow_graphics, true);
  if (s_prev) layer_set_hidden(s_prev->layer, true);
  if (s_next) layer_set_hidden(s_next->layer, true);
#if HAVE_ERROR_LAYER
  if (s_error_layer) layer_set_hidden(text_layer_get_layer(s_error_layer), true);
#endif
}

#if HAVE_ERROR_LAYER
static void show_error(const char *message) {
  if (!s_error_layer) return;
  text_layer_set_text(s_error_layer, message);
  layer_set_hidden(text_layer_get_layer(s_error_layer), false);
  hide_selection();
}
#else
static void show_error(const char *message) { (void)message; }
#endif

static void show_selection(void) {
#if HAVE_ERROR_LAYER
  if (s_error_layer) layer_set_hidden(text_layer_get_layer(s_error_layer), true);
#endif
  layer_set_hidden(text_layer_get_layer(s_title_layer), false);
  if (s_name) layer_set_hidden(s_name->layer, false);
  if (s_domain) layer_set_hidden(s_domain->layer, false);
  layer_set_hidden(s_up_arrow_graphics, false);
  layer_set_hidden(s_down_arrow_graphics, false);
  if (s_prev) layer_set_hidden(s_prev->layer, false);
  if (s_next) layer_set_hidden(s_next->layer, false);
}

static void save_view_timeout_cb(void *context) {
  s_save_view_timeout_timer = NULL;
  if (!s_pending_save_view) return;
  s_pending_save_view = false;
  s_loading = false;
  status_icon_set_loading(false);
  push_routes_window();
}

static void begin_pending_save_view(void) {
  s_pending_save_view = true;
  if (s_save_view_timeout_timer) app_timer_cancel(s_save_view_timeout_timer);
  s_save_view_timeout_timer = app_timer_register(SAVE_VIEW_TIMEOUT_MS,
                                                 save_view_timeout_cb, NULL);
}

static void update_alphabet_display(void) {
  char curr = s_alphabet_letter;
  char prev = curr - 1; if (prev < 'A') prev = 'Z';
  char next = curr + 1; if (next > 'Z') next = 'A';
  char buf[2] = { 0, 0 };
  buf[0] = prev; if (s_prev) auto_scroll_text_layer_set_text(s_prev, buf);
  buf[0] = curr; if (s_name) auto_scroll_text_layer_set_text(s_name, buf);
  buf[0] = next; if (s_next) auto_scroll_text_layer_set_text(s_next, buf);
}

static void advance_alphabet_letter(void) {
  if (s_held_button == BUTTON_ID_UP) {
    s_alphabet_letter--;
    if (s_alphabet_letter < 'A') s_alphabet_letter = 'Z';
  } else {
    s_alphabet_letter++;
    if (s_alphabet_letter > 'Z') s_alphabet_letter = 'A';
  }
  update_alphabet_display();
  vibrate_alphabet_letter();
}

static char current_agency_letter(void) {
  char c = s_name->text[0];
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c < 'A' || c > 'Z') c = 'A';
  return c;
}

static void retry_request(void *context) {
  s_retry_timer = NULL;
  if (s_loading) {
    if (s_last_command == CMD_JUMP_TO_LETTER && s_last_letter != 0)
      send_jump_command(s_last_letter);
    else if (s_last_command == CMD_JUMP_TO_DOMAIN && s_last_domain[0] != '\0')
      send_jump_to_domain_command(s_last_domain);
    else
      send_command(s_last_command);
  }
}

static void send_command(uint8_t command) {
  if (!s_appmsg_ok) return;
  s_last_command = command;
  s_last_letter = 0;
  s_last_domain[0] = '\0';
  s_loading = true;
  status_icon_set_loading(true);
  if (s_is_initial_load) hide_selection();
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_COMMAND, command);
    result = app_message_outbox_send();
  }
  if (s_retry_timer) app_timer_cancel(s_retry_timer);
  s_retry_timer = app_timer_register(2000, retry_request, NULL);
}

static void send_jump_command(uint8_t letter) {
  if (!s_appmsg_ok) return;
  s_last_command = CMD_JUMP_TO_LETTER;
  s_last_letter = letter;
  s_last_domain[0] = '\0';
  s_loading = true;
  status_icon_set_loading(true);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_COMMAND, CMD_JUMP_TO_LETTER);
    dict_write_uint8(iter, MESSAGE_KEY_LETTER, letter);
    result = app_message_outbox_send();
  }
  if (s_retry_timer) app_timer_cancel(s_retry_timer);
  s_retry_timer = app_timer_register(2000, retry_request, NULL);
}

static void send_jump_to_domain_command(const char *domain) {
  if (!s_appmsg_ok) return;
  s_last_command = CMD_JUMP_TO_DOMAIN;
  s_last_letter = 0;
  if (domain != s_last_domain) {
    strncpy(s_last_domain, domain, sizeof(s_last_domain) - 1);
    s_last_domain[sizeof(s_last_domain) - 1] = '\0';
  }
  s_loading = true;
  status_icon_set_loading(true);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_COMMAND, CMD_JUMP_TO_DOMAIN);
    dict_write_cstring(iter, MESSAGE_KEY_JUMP_DOMAIN, domain);
    result = app_message_outbox_send();
  }
  if (s_retry_timer) app_timer_cancel(s_retry_timer);
  s_retry_timer = app_timer_register(2000, retry_request, NULL);
}

static void send_fetch_config_request(void) {
  if (!s_appmsg_ok) return;
  status_icon_set_loading(true);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_FETCH_CONFIG, 1);
    result = app_message_outbox_send();
  }
}

static void send_fetch_routes_request(void) {
  if (!s_appmsg_ok) return;
  status_icon_set_loading(true);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_FETCH_ROUTES, 1);
    result = app_message_outbox_send();
  }
}

static void restore_retry_cb(void *context) {
  s_restore_retry_timer = NULL;
  if (!s_has_saved_agency) return;
  if (s_restore_retry_count >= RESTORE_MAX_RETRIES) {
    s_loading = false;
    s_routes_load_failed = true;
    status_icon_set_loading(false);
    update_route_list();
    return;
  }
  s_restore_retry_count++;
  send_restore_request();
}

static void send_restore_request(void) {
  if (!s_appmsg_ok) return;
  s_routes_load_failed = false;
  status_icon_set_loading(true);
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_FETCH_CONFIG, 1);
    if (s_saved_domain[0] != '\0')
      dict_write_cstring(iter, MESSAGE_KEY_AGENCY_DOMAIN, s_saved_domain);
    result = app_message_outbox_send();
  }
  if (s_restore_retry_timer) app_timer_cancel(s_restore_retry_timer);
  s_restore_retry_timer = app_timer_register(RESTORE_RETRY_INTERVAL_MS,
                                             restore_retry_cb, NULL);
}

static void hold_repeat_cb(void *context) {
  s_hold_timer = NULL;
  if (!s_hold_active) return;
  if (s_in_alphabet_mode) {
    advance_alphabet_letter();
    s_hold_timer = app_timer_register(ALPHABET_ADVANCE_MS, hold_repeat_cb, NULL);
  } else {
    send_command(s_held_button == BUTTON_ID_UP ? CMD_UP : CMD_DOWN);
    s_hold_timer = app_timer_register(HOLD_REPEAT_MS, hold_repeat_cb, NULL);
  }
}

static void route_hold_repeat_cb(void *context) {
  s_hold_timer = NULL;
  if (!s_hold_active) return;
  if (!s_routes_window) { s_hold_active = false; return; }
  route_move_selection(s_held_button == BUTTON_ID_UP ? -1 : 1);
  s_hold_timer = app_timer_register(HOLD_REPEAT_MS, route_hold_repeat_cb, NULL);
}

static void alphabet_trigger_cb(void *context) {
  s_alphabet_trigger = NULL;
  if (!s_hold_active) return;
  s_in_alphabet_mode = true;
  s_alphabet_letter = current_agency_letter();
  update_alphabet_display();
  vibrate_alphabet_letter();
}

static void finish_hold(void) {
  if (s_hold_timer) { app_timer_cancel(s_hold_timer); s_hold_timer = NULL; }
  if (s_alphabet_trigger) {
    app_timer_cancel(s_alphabet_trigger);
    s_alphabet_trigger = NULL;
  }
  if (s_in_alphabet_mode) send_jump_command((uint8_t)s_alphabet_letter);
  s_hold_active = false;
  s_in_alphabet_mode = false;
}

static void route_move_selection(int delta) {
  if (s_route_count == 0) return;
  commit_pending_route_scroll();
  if (s_settings_selected) {
    s_settings_selected = false;
    s_route_selected = (delta < 0) ? (s_route_count - 1) : 0;
  } else if (delta < 0 && s_route_selected == 0) {
    s_settings_selected = true;
  } else {
    s_route_selected += delta;
    if (s_route_selected < 0) s_route_selected = s_route_count - 1;
    if (s_route_selected >= s_route_count) s_route_selected = 0;
  }
  int new_fv = s_route_first_visible;
  if (!s_settings_selected) {
    if (s_route_selected < new_fv) new_fv = s_route_selected;
    if (s_route_selected >= new_fv + s_route_rows_visible)
      new_fv = s_route_selected - s_route_rows_visible + 1;
  }
  start_route_fill_anim();
  if (s_settings_icon_layer) layer_mark_dirty(s_settings_icon_layer);
  int delta_fv = s_route_first_visible - new_fv;
  if (delta_fv == 0) {
    dirty_all_route_text_layers();
    if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
  } else if (delta_fv == 1 || delta_fv == -1) {
    start_route_scroll_anim(new_fv);
  } else {
    s_route_first_visible = new_fv;
    s_route_list_scroll_y = 0;
    set_route_layers_y_offset(0);
    update_route_inv_layers();
    dirty_all_route_text_layers();
    if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);
  }
}

static void up_press_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pending_selection_view) return;
  reset_marquee_activity();
  if (s_pending_save_view) return;
  if (s_hold_active) return;
  s_hold_active = true;
  s_held_button = BUTTON_ID_UP;
  s_in_alphabet_mode = false;
  send_command(CMD_UP);
  s_alphabet_trigger = app_timer_register(ALPHABET_TRIGGER_MS,
                                          alphabet_trigger_cb, NULL);
  s_hold_timer = app_timer_register(HOLD_INITIAL_REPEAT_MS,
                                    hold_repeat_cb, NULL);
}

static void up_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pending_selection_view) return;
  if (s_pending_save_view) return;
  if (s_held_button != BUTTON_ID_UP) return;
  finish_hold();
}

static void down_press_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pending_selection_view) return;
  reset_marquee_activity();
  if (s_pending_save_view) return;
  if (s_hold_active) return;
  s_hold_active = true;
  s_held_button = BUTTON_ID_DOWN;
  s_in_alphabet_mode = false;
  send_command(CMD_DOWN);
  s_alphabet_trigger = app_timer_register(ALPHABET_TRIGGER_MS,
                                          alphabet_trigger_cb, NULL);
  s_hold_timer = app_timer_register(HOLD_INITIAL_REPEAT_MS,
                                    hold_repeat_cb, NULL);
}

static void down_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pending_selection_view) return;
  if (s_pending_save_view) return;
  if (s_held_button != BUTTON_ID_DOWN) return;
  finish_hold();
}

static void si_select_handler(ClickRecognizerRef r, void *ctx) {
  reset_marquee_activity();
  if (s_si_stop_id <= 0) return;

#if SI_SHOW_ICONS
  if (s_si_focus == SI_FOCUS_BELL) {
    open_notif_window();
  } else if (s_si_focus == SI_FOCUS_PIN) {
    stop_toggle_flag(s_si_stop_id, STOP_SETTING_PIN);
    vibes_short_pulse();
    if (s_si && s_si->header_layer) layer_mark_dirty(s_si->header_layer);
  }
#endif
}

static void si_click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP,   100, si_up_handler);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, si_down_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, si_select_handler);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_pending_selection_view) return;
  reset_marquee_activity();
  if (s_pending_save_view) return;
  if (s_in_alphabet_mode) return;
  if (s_loading) return;
  if (s_name->text[0] == '\0') return;
  vibes_short_pulse();
  save_agency(s_name->text, s_domain->text);
  begin_pending_save_view();
  send_fetch_config_request();
}

static void click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP, up_press_handler, up_release_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, down_press_handler, down_release_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

/* =====================================================================
   Routes window handlers
   ===================================================================== */

static void routes_up_press_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_hold_active) return;
  reset_marquee_activity();
  s_hold_active = true;
  s_held_button = BUTTON_ID_UP;
  route_move_selection(-1);
  s_hold_timer = app_timer_register(HOLD_INITIAL_REPEAT_MS,
                                    route_hold_repeat_cb, NULL);
}

static void routes_up_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_held_button != BUTTON_ID_UP) return;
  finish_hold();
}

static void routes_down_press_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_hold_active) return;
  reset_marquee_activity();
  s_hold_active = true;
  s_held_button = BUTTON_ID_DOWN;
  route_move_selection(1);
  s_hold_timer = app_timer_register(HOLD_INITIAL_REPEAT_MS,
                                    route_hold_repeat_cb, NULL);
}

static void routes_down_release_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_held_button != BUTTON_ID_DOWN) return;
  finish_hold();
}

static void routes_select_click_handler(ClickRecognizerRef recognizer, void *context) {
  reset_marquee_activity();

  if (s_settings_selected) {
    char domain_copy[64];
    strncpy(domain_copy, s_saved_domain, sizeof(domain_copy) - 1);
    domain_copy[sizeof(domain_copy) - 1] = '\0';
    clear_saved_agency();
    if (s_top_bar_layer) layer_mark_dirty(s_top_bar_layer);
    if (s_up_arrow_graphics) layer_mark_dirty(s_up_arrow_graphics);
    if (s_down_arrow_graphics) layer_mark_dirty(s_down_arrow_graphics);
    if (s_name) {
      s_name->background_color = s_primary_color;
      s_name->text_color = primary_text_color();
    }
    s_settings_selected = false;
    if (s_settings_icon_layer) layer_mark_dirty(s_settings_icon_layer);

    show_selection();
    s_pending_selection_view = true;

    if (s_routes_window) window_stack_remove(s_routes_window, true);

    if (domain_copy[0] != '\0') send_jump_to_domain_command(domain_copy);
    else send_command(CMD_INIT);
    return;
  }
  if (s_route_count > 0 && s_route_selected < s_route_count) {
    open_stops_window(s_route_selected);
  }
}

static void routes_select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (s_settings_selected) return;
  if (s_route_count == 0) return;
  if (s_route_selected >= s_route_count) return;
  reset_marquee_activity();
  toggle_favorite(s_route_selected);
}

static void routes_back_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
}

static void routes_click_config_provider(void *context) {
  window_raw_click_subscribe(BUTTON_ID_UP,
                             routes_up_press_handler, routes_up_release_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN,
                             routes_down_press_handler, routes_down_release_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, routes_select_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, routes_select_long_click_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, routes_back_click_handler);
}

static void routes_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  window_set_background_color(window, GColorWhite);

  const int top_bar_height = 20;

  s_routes_top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar_height));
  layer_set_update_proc(s_routes_top_bar_layer, top_bar_draw);
  layer_add_child(root, s_routes_top_bar_layer);

  s_routes_clock_layer = text_layer_create(GRect(LEFT_MARGIN, 0, 80, top_bar_height));
  text_layer_set_text_color(s_routes_clock_layer, topbar_text_color());
  text_layer_set_background_color(s_routes_clock_layer, GColorClear);
  text_layer_set_font(s_routes_clock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_routes_clock_layer, GTextAlignmentLeft);
  layer_add_child(root, text_layer_get_layer(s_routes_clock_layer));
  update_clock();

  const int battery_w = 20, battery_h = 10, rm = 4, ic_size = 16, gap_ib = 4;
  int battery_x = bounds.size.w - rm - battery_w;
  int battery_y = (top_bar_height - battery_h) / 2;
  int icon_x = battery_x - gap_ib - ic_size;
  int icon_y = (top_bar_height - ic_size) / 2;

  s_routes_status_icon_layer = layer_create(GRect(icon_x, icon_y, ic_size, ic_size));
  layer_set_update_proc(s_routes_status_icon_layer, status_icon_update_proc);
  layer_add_child(root, s_routes_status_icon_layer);

  s_routes_battery_layer = layer_create(GRect(battery_x, battery_y, battery_w, battery_h));
  layer_set_update_proc(s_routes_battery_layer, battery_layer_update_proc);
  layer_add_child(root, s_routes_battery_layer);

  int list_y = top_bar_height + AGENCY_BAR_H;
  int list_h = bounds.size.h - list_y;

  s_agency_bar_layer = layer_create(GRect(0, top_bar_height, bounds.size.w, AGENCY_BAR_H));
  layer_set_update_proc(s_agency_bar_layer, agency_bar_update_proc);
  layer_add_child(root, s_agency_bar_layer);

  int settings_w = AGENCY_BAR_H;
  int settings_x = bounds.size.w - settings_w;
  int name_x = LEFT_MARGIN;
  int name_w = settings_x - name_x - 4;
  if (name_w < 1) name_w = 1;

  s_saved_name_auto = auto_scroll_text_layer_create(
    GRect(name_x, top_bar_height, name_w, AGENCY_BAR_H));
  if (!s_saved_name_auto) return;

  s_saved_name_auto->font = fonts_get_system_font(FONT_NAME);
  s_saved_name_auto->text_color = AGENCY_BAR_TEXT_COLOR;
  s_saved_name_auto->background_color = AGENCY_BAR_BG_COLOR;
  s_saved_name_auto->left_align_when_fits = true;
  auto_scroll_text_layer_set_text(s_saved_name_auto, s_saved_name);
  layer_add_child(root, s_saved_name_auto->layer);

  s_settings_icon_layer = layer_create(GRect(settings_x, top_bar_height, settings_w, settings_w));
  layer_set_update_proc(s_settings_icon_layer, settings_icon_update_proc);
  layer_add_child(root, s_settings_icon_layer);

  s_agency_bar_x_orig    = layer_get_frame(s_agency_bar_layer).origin.x;
  s_settings_icon_x_orig = layer_get_frame(s_settings_icon_layer).origin.x;
  s_saved_name_x_orig    = layer_get_frame(s_saved_name_auto->layer).origin.x;

  s_route_text_layers = calloc(ROUTE_LAYER_COUNT, sizeof(Layer *));
  s_route_inv_layers  = calloc(ROUTE_LAYER_COUNT, sizeof(Layer *));
  if (!s_route_text_layers || !s_route_inv_layers) return;

  s_route_list_layer = layer_create(GRect(0, list_y, bounds.size.w, list_h));
  layer_set_update_proc(s_route_list_layer, route_list_update_proc);
  layer_add_child(root, s_route_list_layer);

  for (int i = 0; i < ROUTE_LAYER_COUNT; i++) {
    int j = i - 1;
    GRect tf = GRect(s_route_text_x, j * s_route_row_h, s_route_text_w, s_route_row_h);
    s_route_text_layers[i] = layer_create_with_data(tf, sizeof(int));
    *(int *)layer_get_data(s_route_text_layers[i]) = j;
    layer_set_update_proc(s_route_text_layers[i], route_text_layer_update_proc);
    layer_add_child(s_route_list_layer, s_route_text_layers[i]);

    GRect invf = GRect(s_route_text_x, j * s_route_row_h, 0, s_route_row_h);
    s_route_inv_layers[i] = layer_create_with_data(invf, sizeof(int));
    *(int *)layer_get_data(s_route_inv_layers[i]) = j;
    layer_set_update_proc(s_route_inv_layers[i], route_inv_layer_update_proc);
    layer_add_child(s_route_list_layer, s_route_inv_layers[i]);
    layer_set_hidden(s_route_inv_layers[i], true);
  }
  s_route_list_x_orig = layer_get_frame(s_route_list_layer).origin.x;

  update_route_inv_layers();
  dirty_all_route_text_layers();
  if (s_route_list_layer) layer_mark_dirty(s_route_list_layer);

  window_set_click_config_provider(window, routes_click_config_provider);
  reset_marquee_activity();
}

static void routes_window_appear(Window *window) {
  s_showing_saved_view = true;
  accel_tap_service_subscribe(tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_event_handler, NULL);
#endif
  reset_marquee_activity();
}

static void routes_window_disappear(Window *window) {
  s_showing_saved_view = false;
  accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
}

static void routes_window_unload(Window *window) {
  if (s_route_fill_anim) {
    animation_unschedule(s_route_fill_anim); s_route_fill_anim = NULL;
  }
  if (s_route_list_scroll_anim) {
    animation_unschedule(s_route_list_scroll_anim);
    s_route_list_scroll_anim = NULL;
  }
  if (s_route_scroll_timer) {
    app_timer_cancel(s_route_scroll_timer); s_route_scroll_timer = NULL;
  }

  if (s_route_text_layers && s_route_inv_layers) {
    for (int i = 0; i < ROUTE_LAYER_COUNT; i++) {
      if (s_route_text_layers[i]) {
        layer_destroy(s_route_text_layers[i]); s_route_text_layers[i] = NULL;
      }
      if (s_route_inv_layers[i]) {
        layer_destroy(s_route_inv_layers[i]); s_route_inv_layers[i] = NULL;
      }
    }
  }
  if (s_route_text_layers) { free(s_route_text_layers); s_route_text_layers = NULL; }
  if (s_route_inv_layers) { free(s_route_inv_layers); s_route_inv_layers = NULL; }

  if (s_saved_name_auto) { auto_scroll_text_layer_destroy(s_saved_name_auto); s_saved_name_auto = NULL; }
  if (s_agency_bar_layer) { layer_destroy(s_agency_bar_layer); s_agency_bar_layer = NULL; }
  if (s_settings_icon_layer) { layer_destroy(s_settings_icon_layer); s_settings_icon_layer = NULL; }
  if (s_route_list_layer) { layer_destroy(s_route_list_layer); s_route_list_layer = NULL; }

  if (s_routes_top_bar_layer) { layer_destroy(s_routes_top_bar_layer); s_routes_top_bar_layer = NULL; }
  if (s_routes_clock_layer) { text_layer_destroy(s_routes_clock_layer); s_routes_clock_layer = NULL; }
  if (s_routes_status_icon_layer) { layer_destroy(s_routes_status_icon_layer); s_routes_status_icon_layer = NULL; }
  if (s_routes_battery_layer) { layer_destroy(s_routes_battery_layer); s_routes_battery_layer = NULL; }
}

static void push_routes_window(void) {
  if (!s_routes_window) {
    s_routes_window = window_create();
    window_set_window_handlers(s_routes_window, (WindowHandlers){
      .load      = routes_window_load,
      .appear    = routes_window_appear,
      .disappear = routes_window_disappear,
      .unload    = routes_window_unload
    });
  }
  if (s_routes_window) window_stack_push(s_routes_window, true);
}

static void deferred_push_routes_window(void *context) {
  (void)context;
  s_routes_push_timer = NULL;
  push_routes_window();
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  if (s_retry_timer) { app_timer_cancel(s_retry_timer); s_retry_timer = NULL; }

  bool has_config = false;
  char apikey_buf[24] = {0};
  uint32_t color_val = 0;
  char *prev_name = NULL, *current_name = NULL, *current_url = NULL, *next_name = NULL;
  bool has_error = false;
  char error_message[32] = "Network error";
  int32_t chunk_idx = -1, chunk_total = 0;
  char *chunk_data = NULL;
  int32_t stop_chunk_idx = -1, stop_chunk_total = 0;
  char *stop_chunk_data = NULL;
  int32_t arrival_chunk_idx = -1, arrival_chunk_total = 0;
  char *arrival_chunk_data = NULL;
  bool has_location = false;

  Tuple *t = dict_read_first(iterator);
  while (t != NULL) {
    if (t->key == MESSAGE_KEY_API_KEY) {
      has_config = true;
      if (t->length > 0) {
        strncpy(apikey_buf, t->value->cstring, sizeof(apikey_buf) - 1);
        apikey_buf[sizeof(apikey_buf) - 1] = '\0';
      }
    } else if (t->key == MESSAGE_KEY_PRIMARY_COLOR) {
      has_config = true; color_val = (uint32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_ERROR) {
      has_error = true;
      if (t->length > 0) {
        strncpy(error_message, t->value->cstring, sizeof(error_message) - 1);
        error_message[sizeof(error_message) - 1] = '\0';
      }
    } else if (t->key == MESSAGE_KEY_PREV_NAME) { prev_name = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_CURRENT_NAME) { current_name = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_CURRENT_URL) { current_url = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_NEXT_NAME) { next_name = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_CHUNK_IDX) { chunk_idx = (int32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_CHUNK_TOTAL) { chunk_total = (int32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_CHUNK_DATA) { chunk_data = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_STOP_CHUNK_IDX) { stop_chunk_idx = (int32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_STOP_CHUNK_TOTAL) { stop_chunk_total = (int32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_STOP_CHUNK_DATA) { stop_chunk_data = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_ARRIVAL_CHUNK_IDX) { arrival_chunk_idx = (int32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_ARRIVAL_CHUNK_TOTAL) { arrival_chunk_total = (int32_t)t->value->int32;
    } else if (t->key == MESSAGE_KEY_ARRIVAL_CHUNK_DATA) { arrival_chunk_data = t->value->cstring;
    } else if (t->key == MESSAGE_KEY_LOCATION_LAT) {
      has_location = true; s_my_lat = (int32_t)t->value->int32; s_have_location = true;
    } else if (t->key == MESSAGE_KEY_LOCATION_LON) {
      has_location = true; s_my_lon = (int32_t)t->value->int32; s_have_location = true;
    } else if (t->key == MESSAGE_KEY_LOCATION_ERROR) { has_location = true; }
    t = dict_read_next(iterator);
  }

  if (has_location) {
    if (s_location_pending) {
      s_location_pending = false;
      if (s_sv && s_sv->location_timeout_timer) {
        app_timer_cancel(s_sv->location_timeout_timer);
        s_sv->location_timeout_timer = NULL;
      }
    }
    try_finish_stops_load();
    return;
  }
  if (arrival_chunk_idx >= 0 && arrival_chunk_total > 0 &&
      arrival_chunk_data != NULL) {
    si_handle_arrival_chunk(arrival_chunk_idx, arrival_chunk_total,
                            arrival_chunk_data);
    return;
  }
  if (stop_chunk_idx >= 0 && stop_chunk_total > 0 && stop_chunk_data != NULL) {
    handle_stops_chunk(stop_chunk_idx, stop_chunk_total, stop_chunk_data); return;
  }
  if (chunk_idx >= 0 && chunk_total > 0 && chunk_data != NULL) {
    handle_routes_chunk(chunk_idx, chunk_total, chunk_data); return;
  }
  if (has_config) {
    if (s_restore_retry_timer) {
      app_timer_cancel(s_restore_retry_timer);
      s_restore_retry_timer = NULL;
    }
    s_restore_retry_count = 0;
    save_agency_config(apikey_buf, color_val);
    if (s_top_bar_layer) layer_mark_dirty(s_top_bar_layer);
    if (s_routes_top_bar_layer) layer_mark_dirty(s_routes_top_bar_layer);
    if (s_sv && s_sv->top_bar_layer) layer_mark_dirty(s_sv->top_bar_layer);
    if (s_si && s_si->top_bar_layer) layer_mark_dirty(s_si->top_bar_layer);
    if (s_up_arrow_graphics) layer_mark_dirty(s_up_arrow_graphics);
    if (s_down_arrow_graphics) layer_mark_dirty(s_down_arrow_graphics);
    if (s_name) s_name->background_color = s_primary_color;
    if (!s_pending_save_view) { s_loading = false; status_icon_set_loading(false); }
    send_fetch_routes_request();
    return;
  }

  s_loading = false;
  status_icon_set_loading(false);
  if (s_in_alphabet_mode) return;
  if (has_error) { if (!s_showing_saved_view) show_error(error_message); return; }

  auto_scroll_text_layer_set_text(s_prev, prev_name ? prev_name : "");
  auto_scroll_text_layer_set_text(s_name, current_name ? current_name : "No data");
  auto_scroll_text_layer_set_text(s_next, next_name ? next_name : "");
  auto_scroll_text_layer_set_text(s_domain, current_url ? current_url : "");
  reset_marquee_activity();

  bool was_initial_load = s_is_initial_load;
  s_is_initial_load = false;
  if (s_pending_selection_view) {
    s_pending_selection_view = false;
    return;
  }
  if (!was_initial_load && !s_showing_saved_view) vibrate_scroll();
  if (!s_showing_saved_view && !s_pending_save_view) show_selection();
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped: %d (will retry)", reason);
}

static void outbox_failed_callback(DictionaryIterator *iterator,
                                   AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d (will retry)", reason);
}

static void window_appear(Window *window) {
  accel_tap_service_subscribe(tap_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(touch_event_handler, NULL);
#endif
  reset_marquee_activity();
}

static void window_disappear(Window *window) {
  accel_tap_service_unsubscribe();
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(window, GColorWhite);
  s_pending_selection_view = false;
  s_routes_load_failed = false;
  load_saved_agency();
  load_favorites();
  load_stop_settings();

  const int top_bar_height = 20;
  const int spacing        = 2;
  const int arrow_height   = 8;
  const int domain_height  = 20;

  s_top_bar_layer = layer_create(GRect(0, 0, bounds.size.w, top_bar_height));
  layer_set_update_proc(s_top_bar_layer, top_bar_draw);
  layer_add_child(window_layer, s_top_bar_layer);

  s_clock_layer = text_layer_create(GRect(LEFT_MARGIN, 0, 80, top_bar_height));
  text_layer_set_text_color(s_clock_layer, topbar_text_color());
  text_layer_set_background_color(s_clock_layer, GColorClear);
  text_layer_set_font(s_clock_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  text_layer_set_text_alignment(s_clock_layer, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_clock_layer));
  update_clock();

  const int battery_w = 20, battery_h = 10, right_margin = 4, icon_size = 16, gap_icon_battery = 4;
  int battery_x = bounds.size.w - right_margin - battery_w;
  int battery_y = (top_bar_height - battery_h) / 2;
  int icon_x = battery_x - gap_icon_battery - icon_size;
  int icon_y = (top_bar_height - icon_size) / 2;

  s_status_icon_layer = layer_create(GRect(icon_x, icon_y, icon_size, icon_size));
  layer_set_update_proc(s_status_icon_layer, status_icon_update_proc);
  layer_add_child(window_layer, s_status_icon_layer);

  s_battery_layer = layer_create(GRect(battery_x, battery_y, battery_w, battery_h));
  layer_set_update_proc(s_battery_layer, battery_layer_update_proc);
  layer_add_child(window_layer, s_battery_layer);

  battery_state_service_subscribe(battery_state_handler);
  battery_state_handler(battery_state_service_peek());

  int selection_block_height = arrow_height + spacing + PREV_HEIGHT + spacing +
                               NAME_HEIGHT    + spacing + NEXT_HEIGHT + spacing + arrow_height;
  int screen_center = bounds.size.h / 2;
  int up_arrow_y    = screen_center - selection_block_height / 2;
  int title_top    = top_bar_height;
  int title_bottom = up_arrow_y;
  int title_y      = title_top + (title_bottom - title_top - TITLE_HEIGHT) / 2;
  if (title_y < title_top) title_y = title_top;

  s_title_layer = text_layer_create(GRect(0, title_y, bounds.size.w, TITLE_HEIGHT));
  text_layer_set_text(s_title_layer, "Agency Selection");
  text_layer_set_text_color(s_title_layer, GColorBlack);
  text_layer_set_background_color(s_title_layer, GColorClear);
  text_layer_set_font(s_title_layer, fonts_get_system_font(FONT_TITLE));
  text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_title_layer));

  int current_y = up_arrow_y;
  s_up_arrow_graphics = layer_create(GRect(0, current_y, bounds.size.w, arrow_height));
  layer_set_update_proc(s_up_arrow_graphics, draw_up_arrow);
  layer_add_child(window_layer, s_up_arrow_graphics);
  current_y += arrow_height + spacing;

  s_prev = auto_scroll_text_layer_create(GRect(0, current_y, bounds.size.w, PREV_HEIGHT));
  s_prev->font = fonts_get_system_font(FONT_BODY);
  s_prev->text_color = GColorDarkGray;
  s_prev->background_color = GColorClear;
  layer_add_child(window_layer, s_prev->layer);
  current_y += PREV_HEIGHT + spacing;

  s_name = auto_scroll_text_layer_create(GRect(0, current_y, bounds.size.w, NAME_HEIGHT));
  s_name->font = fonts_get_system_font(FONT_NAME);
  s_name->text_color = primary_text_color();
  s_name->background_color = s_primary_color;
  layer_add_child(window_layer, s_name->layer);
  current_y += NAME_HEIGHT + spacing;

  s_next = auto_scroll_text_layer_create(GRect(0, current_y, bounds.size.w, NEXT_HEIGHT));
  s_next->font = fonts_get_system_font(FONT_BODY);
  s_next->text_color = GColorDarkGray;
  s_next->background_color = GColorClear;
  layer_add_child(window_layer, s_next->layer);
  current_y += NEXT_HEIGHT + spacing;

  s_down_arrow_graphics = layer_create(GRect(0, current_y, bounds.size.w, arrow_height));
  layer_set_update_proc(s_down_arrow_graphics, draw_down_arrow);
  layer_add_child(window_layer, s_down_arrow_graphics);

  int down_arrow_bottom = current_y + arrow_height;
  int space_below = bounds.size.h - down_arrow_bottom;
  int domain_y = down_arrow_bottom + (space_below - domain_height) / 2;

  s_domain = auto_scroll_text_layer_create(GRect(0, domain_y, bounds.size.w, domain_height));
  s_domain->font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_domain->text_color = GColorBlack;
  s_domain->background_color = GColorClear;
  layer_add_child(window_layer, s_domain->layer);

  {
    int list_h = bounds.size.h - top_bar_height - AGENCY_BAR_H;
    s_route_rows_visible = ROUTE_ROWS_VISIBLE;
    s_route_row_h = list_h / s_route_rows_visible;
    s_route_full_w = bounds.size.w;
    s_route_square_w = s_route_row_h;
    const int heart_size_win = 15;
    int heart_reserve = HEART_RIGHT_MARGIN + heart_size_win + LEFT_MARGIN;
    s_route_text_x = s_route_row_h + LEFT_MARGIN;
    s_route_text_w = bounds.size.w - s_route_text_x - heart_reserve;
    if (s_route_text_w < 1) s_route_text_w = 1;
  }

  s_route_bitmap = gbitmap_create_with_resource(ROUTE_RES);
  s_settings_gear_bitmap = gbitmap_create_with_resource(RESOURCE_ID_GEAR_25);
  s_bell_25_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BELL_25);
  s_pin_25_bitmap  = gbitmap_create_with_resource(RESOURCE_ID_PIN_25);
  s_bus_25_bitmap  = gbitmap_create_with_resource(RESOURCE_ID_BUS_25);
  s_bell_80_bitmap = gbitmap_create_with_resource(BELL_NOTIF_RES);

#if HAVE_ERROR_LAYER
  {
    s_error_layer = text_layer_create(
      GRect(10, (bounds.size.h - 40) / 2, bounds.size.w - 20, 40));
    text_layer_set_text(s_error_layer, "Error");
    text_layer_set_text_alignment(s_error_layer, GTextAlignmentCenter);
    text_layer_set_font(s_error_layer,
                        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_color(s_error_layer, GColorRed);
    text_layer_set_background_color(s_error_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(s_error_layer));
    layer_set_hidden(text_layer_get_layer(s_error_layer), true);
  }
#endif

  hide_selection();

  window_set_click_config_provider(window, click_config_provider);
  tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

  if (s_has_saved_agency) {
    send_restore_request();
    if (s_routes_push_timer) app_timer_cancel(s_routes_push_timer);
    s_routes_push_timer = app_timer_register(400, deferred_push_routes_window, NULL);
  } else {
    send_command(CMD_INIT);
  }
}

static void window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();

  if (s_retry_timer) app_timer_cancel(s_retry_timer);
  if (s_restore_retry_timer) {
    app_timer_cancel(s_restore_retry_timer); s_restore_retry_timer = NULL;
  }
  if (s_marquee_idle_timer) {
    app_timer_cancel(s_marquee_idle_timer); s_marquee_idle_timer = NULL;
  }
  if (s_spinner_timer) { app_timer_cancel(s_spinner_timer); s_spinner_timer = NULL; }
  if (s_hold_timer) { app_timer_cancel(s_hold_timer); s_hold_timer = NULL; }
  if (s_alphabet_trigger) {
    app_timer_cancel(s_alphabet_trigger); s_alphabet_trigger = NULL;
  }
  if (s_save_view_timeout_timer) {
    app_timer_cancel(s_save_view_timeout_timer);
    s_save_view_timeout_timer = NULL;
  }
  if (s_routes_push_timer) {
    app_timer_cancel(s_routes_push_timer);
    s_routes_push_timer = NULL;
  }

  layer_destroy(s_top_bar_layer);
  text_layer_destroy(s_clock_layer);
  layer_destroy(s_status_icon_layer);
  layer_destroy(s_battery_layer);
  text_layer_destroy(s_title_layer);
  layer_destroy(s_up_arrow_graphics);
  layer_destroy(s_down_arrow_graphics);
#if HAVE_ERROR_LAYER
  if (s_error_layer) text_layer_destroy(s_error_layer);
#endif
  auto_scroll_text_layer_destroy(s_prev);
  auto_scroll_text_layer_destroy(s_name);
  auto_scroll_text_layer_destroy(s_next);
  auto_scroll_text_layer_destroy(s_domain);

  if (s_route_bitmap) { gbitmap_destroy(s_route_bitmap); s_route_bitmap = NULL; }
  if (s_settings_gear_bitmap) {
    gbitmap_destroy(s_settings_gear_bitmap); s_settings_gear_bitmap = NULL;
  }
  if (s_bell_25_bitmap) { gbitmap_destroy(s_bell_25_bitmap); s_bell_25_bitmap = NULL; }
  if (s_pin_25_bitmap)  { gbitmap_destroy(s_pin_25_bitmap);  s_pin_25_bitmap  = NULL; }
  if (s_bus_25_bitmap)  { gbitmap_destroy(s_bus_25_bitmap);  s_bus_25_bitmap  = NULL; }
  if (s_bell_80_bitmap) { gbitmap_destroy(s_bell_80_bitmap); s_bell_80_bitmap = NULL; }
  if (s_routes) { free(s_routes); s_routes = NULL; s_route_capacity = 0; }
}

static bool open_app_message(void) {
  AppMessageResult r = app_message_open(1024, 512);
  return r == APP_MSG_OK;
}

static void init(void) {
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);

  g = malloc(sizeof(AppState));
  if (!g) return;
  memset(g, 0, sizeof(AppState));

  s_appmsg_ok = open_app_message();

  s_main_window = window_create();
  if (!s_main_window) return;
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload
  });

#if SCREENSHOT_MODE
  light_enable(true);
#endif

  window_stack_push(s_main_window, true);
}

static void deinit(void) {
  if (s_routes_window) { window_destroy(s_routes_window); s_routes_window = NULL; }
  if (s_main_window) { window_destroy(s_main_window); s_main_window = NULL; }
  if (s_si) { if (s_si->window) window_destroy(s_si->window); free(s_si); s_si = NULL; }
  if (s_nv) { if (s_nv->window) window_destroy(s_nv->window); free(s_nv); s_nv = NULL; }
  if (s_stop_distances) { free(s_stop_distances); s_stop_distances = NULL; }
  if (s_si_buses) { free(s_si_buses); s_si_buses = NULL; }
  if (s_sv) { free(s_sv); s_sv = NULL; }
  if (g) { free(g); g = NULL; }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
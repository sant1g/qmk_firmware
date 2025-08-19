#include <stdatomic.h>
#include <stdint.h>
#include QMK_KEYBOARD_H
#include "oled.c"
#include "print.h"

#define HOME_TIMEOUT 2000
#define WPM_MIN_COUNT 20
#define MAX_WPM_LEVEL 120
#define NUM_ENC_MODES 4

enum custom_keycodes {
    ENC_TOGG_R = SAFE_RANGE,
    ENC_TOGG_L,
    ENC_TOGG_SCREEN_R,
    ENC_TOGG_SCREEN_L,
};

enum encoder_modes {
    VOLUME = 0,
    SCROLL,
    MOVE,
    MEDIA,
};

enum oled_screens {
    HOME = 0,
    VOL,
    NOW_PLAYING,
    TYPING,
    SYSTEM_INFO,
};

typedef enum {
    _TIME = 0xAA, // random value that does not conflict with VIA, must match companion app
    _VOLUME,
    _LAYOUT,
    _MEDIA_ARTIST,
    _MEDIA_TITLE,
    _DATE,
    _CPU_USAGE,
    _RAM_USAGE,
    _NETWORK_RX,
    _NETWORK_TX,
    _SPACE,
    _ENCODER_MODE,
} hid_data_type;

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT_0(
        KC_ESC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINS, KC_EQL, KC_BSLS, KC_GRV,
        KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBRC, KC_RBRC, KC_BSPC,
        KC_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, KC_ENT,
        KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT, MO(1),
        ENC_TOGG_L, KC_LGUI, KC_LALT, KC_SPC, KC_RGUI, KC_UP, KC_DOWN, ENC_TOGG_R
    ),
    [1] = LAYOUT_0(
        KC_TRNS, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12, KC_MUTE, KC_MPLY,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_DEL,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_ENT,
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
        ENC_TOGG_SCREEN_L, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_LEFT, KC_RIGHT, ENC_TOGG_SCREEN_R
    )
};

const char* mode_names[] = { "VOL", "SCR", "MOV", "MED" };

static uint8_t oled_screen = HOME;
static uint8_t encoder_mode_left = MOVE;
static uint8_t encoder_mode_right = VOLUME;

static char time_label[32] = "";
static char date_label[32] = "";
static char media_title_label[32] = "";
static char media_artist_label[32] = "";
static char cpu_usage_label[32] = "CPU:  0%";
static char ram_usage_label[32] = "RAM:  0%";
static char network_rx_label[32] = "NRX: 0 MB/s";
static char network_tx_label[32] = "NTX: 0 MB/s";

static int volume_level = 0;

void clear_text(void) {
    oled_set_cursor(0, 2);
    oled_write_ln("", false);
    oled_set_cursor(0, 3);
    oled_write_ln("", false);
}

bool is_apple(void) {
    os_variant_t os = detected_host_os();

    return os == OS_MACOS || os == OS_IOS;
}

void send_encoder_mode(uint8_t index, uint8_t mode) {
  uint8_t data[32];
  memset(data, 0, 32);
  data[0] = _ENCODER_MODE;
  data[1] = index;
  data[2] = mode;

  host_raw_hid_send(data, 32);

  dprint("sending: ");
  for (int i = 0; i < 31; i++) {
      dprintf("%u ", data[i]);
  }
  dprint("\n");
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case ENC_TOGG_L:
            if (record->event.pressed) {
                encoder_mode_left = (encoder_mode_left + 1) % NUM_ENC_MODES;
                send_encoder_mode(0, encoder_mode_left);
            }
            break;
        case ENC_TOGG_R:
            if (record->event.pressed) {
                encoder_mode_right = (encoder_mode_right + 1) % NUM_ENC_MODES;
                send_encoder_mode(1, encoder_mode_right);
            }
            break;
        case ENC_TOGG_SCREEN_L:
            if (record->event.pressed) {
                if (oled_screen != SYSTEM_INFO) {
                    clear_text();
                    oled_screen = SYSTEM_INFO;
                } else {
                    clear_text();
                    oled_screen = HOME;
                }
            }
            break;
        case ENC_TOGG_SCREEN_R:
            if (record->event.pressed) {
                if (oled_screen != NOW_PLAYING) {
                    clear_text();
                    oled_screen = NOW_PLAYING;
                } else {
                    clear_text();
                    oled_screen = HOME;
                }
            }
            break;
    }

  return true;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) {
        switch(encoder_mode_left) {
            case VOLUME:
                if (clockwise) {
                    tap_code(KC_VOLU);
                } else {
                    tap_code(KC_VOLD);
                }
                break;

            case SCROLL:
                if (clockwise) {
                    tap_code(KC_MS_WH_LEFT);
                } else {
                    tap_code(KC_MS_WH_RIGHT);
                }
                break;

            case MOVE:
                if (clockwise) {
                    tap_code(KC_RIGHT);
                } else {
                    tap_code(KC_LEFT);
                }
                break;

            case MEDIA:
                if (clockwise) {
                    tap_code(KC_MNXT);
                } else {
                    tap_code(KC_MPRV);
                }
                break;
        }
    } else if (index == 1) {
        switch(encoder_mode_right) {
            case VOLUME:
                if (clockwise) {
                    tap_code(KC_VOLU);
                } else {
                    tap_code(KC_VOLD);
                }
                break;

            case SCROLL:
                if (clockwise) {
                    tap_code(KC_MS_WH_UP);
                } else {
                    tap_code(KC_MS_WH_DOWN);
                }
                break;

            case MOVE:
                if (clockwise) {
                    tap_code(KC_DOWN);
                } else {
                    tap_code(KC_UP);
                }
                break;

            case MEDIA:
                if (clockwise) {
                    tap_code(KC_MNXT);
                } else {
                    tap_code(KC_MPRV);
                }
                break;
        }
    }

    return false;
}

void draw_home(void) {
    if (is_apple()) {
        oled_write_raw_P(oled_home_mac, sizeof(oled_home_mac));
    } else {
        oled_write_raw_P(oled_home, sizeof(oled_home));
    }

    oled_set_cursor(8, 2);
    oled_write(date_label, false);

    oled_set_cursor(0, 3);
    char enc_left[32];
    snprintf(enc_left, 32, "< %s", mode_names[encoder_mode_left]);
    oled_write(enc_left, false);

    oled_set_cursor(8, 3);
    oled_write(time_label, false);

    oled_set_cursor(16, 3);
    char enc_right[32];
    snprintf(enc_right, 32, "%s >", mode_names[encoder_mode_right]);
    oled_write(enc_right, false);
}

void draw_line(int current, int max) {
    int level = 0;

    if (current != 0) {
        level = (21 * current) / max;
    }

    if (level > 21) {
        level = 21;
    }

    const char* label[] = {
        "",
        " ",
        "  ",
        "   ",
        "    ",
        "     ",
        "      ",
        "       ",
        "        ",
        "         ",
        "          ",
        "           ",
        "            ",
        "             ",
        "              ",
        "               ",
        "                ",
        "                 ",
        "                  ",
        "                   ",
        "                    ",
        "                     ",
    };

    oled_set_cursor(0, 3);
    oled_write(label[level], true);

    if (level != 0 && level < 21) {
        oled_set_cursor(level, 3);
        oled_write_ln(" ", false);
    }
}

void draw_volume(void) {
    if (is_apple()) {
        oled_write_raw_P(oled_volume_mac, sizeof(oled_volume_mac));
    } else {
        oled_write_raw_P(oled_volume, sizeof(oled_volume));
    }

    oled_set_cursor(7, 2);
    char volume_label[32];
    snprintf(volume_label, 32, "VOL%3d%%", volume_level);
    oled_write(volume_label, false);

    draw_line(volume_level, 100);
}

void draw_now_playing(void) {
    if (is_apple()) {
        oled_write_raw_P(oled_now_playing_mac, sizeof(oled_now_playing_mac));
    } else {
        oled_write_raw_P(oled_now_playing, sizeof(oled_now_playing));
    }

    oled_set_cursor(0, 2);
    oled_write(media_artist_label, false);

    oled_set_cursor(0, 3);
    oled_write(media_title_label, false);
}

void draw_system_info(void) {
    if (is_apple()) {
        oled_write_raw_P(oled_system_info_mac, sizeof(oled_system_info_mac));
    } else {
        oled_write_raw_P(oled_system_info, sizeof(oled_system_info));
    }

    oled_set_cursor(0, 2);
    oled_write(cpu_usage_label, false);

    oled_set_cursor(10, 2);
    oled_write(network_rx_label, false);

    oled_set_cursor(0, 3);
    oled_write(ram_usage_label, false);

    oled_set_cursor(10, 3);
    oled_write(network_tx_label, false);
}

void draw_typing(void) {
    if (is_apple()) {
        oled_write_raw_P(oled_typing_mac, sizeof(oled_typing_mac));
    } else {
        oled_write_raw_P(oled_typing, sizeof(oled_typing));
    }

    oled_set_cursor(9, 2);
    char wpm_label[32];
    snprintf(wpm_label, 32, "%03d", get_current_wpm());
    oled_write(wpm_label, false);

    draw_line(get_current_wpm(), MAX_WPM_LEVEL);
}

uint32_t home_timeout = 0;

bool oled_task_user(void) {
    // Set Typing screen when WPM_MIN_COUNT threshold is crossed and updates the timeout to home.
    if (get_current_wpm() > WPM_MIN_COUNT) {
        if (oled_screen != TYPING) {
            clear_text();
            oled_screen = TYPING;
        }

        home_timeout = timer_read32();
    }

    switch (oled_screen) {
        case HOME:
            draw_home();
            break;
        case VOL:
            draw_volume();
            break;
        case NOW_PLAYING:
            draw_now_playing();
            break;
        case TYPING:
            draw_typing();
            break;
        case SYSTEM_INFO:
            draw_system_info();
            break;
    }

    // Return to home if current screen is Volume or Typing and timeout to home is reached.
    if ((oled_screen == VOL || oled_screen == TYPING) && timer_elapsed32(home_timeout) > HOME_TIMEOUT) {
        clear_text();
        oled_screen = HOME;
    }

    return false;
}

void read_string(uint8_t *data, char *string_data) {
    uint8_t data_length = data[1];
    memcpy(string_data, data + 2, data_length);
    string_data[data_length] = '\0';
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    uint8_t data_type = data[0];
    char    string_data[length - 2];

    dprint("receiving: ");
    for (int i = 0; i < 31; i++) {
        dprintf("%u ", data[i]);
    }
    dprint("\n");

    switch (data_type) {
        case _TIME:
            snprintf(time_label, 32, "%02d:%02d", data[1], data[2]);
            break;
        case _DATE:
            snprintf(date_label, 32, "%02d/%02d", data[1], data[2]);
            break;
        case _VOLUME:
            volume_level = data[1];
            if (oled_screen != VOL) {
                clear_text();
                oled_screen = VOL;
            }
            home_timeout = timer_read32();
            break;
        case _MEDIA_ARTIST:
            read_string(data, string_data);
            if (oled_screen == NOW_PLAYING) {
                clear_text();
            }
            snprintf(media_artist_label, 32, "%.21s", string_data);
            break;
        case _MEDIA_TITLE:
            read_string(data, string_data);
            if (oled_screen == NOW_PLAYING) {
                clear_text();
            }
            snprintf(media_title_label, 32, "%.21s", string_data);
            break;
        case _CPU_USAGE:
            if (oled_screen == SYSTEM_INFO) {
                clear_text();
            }
            snprintf(cpu_usage_label, 32, "CPU:%3d%%", data[1]);
            break;
        case _RAM_USAGE:
            if (oled_screen == SYSTEM_INFO) {
                clear_text();
            }
            snprintf(ram_usage_label, 32, "RAM:%3d%%", data[1]);
            break;
        case _NETWORK_RX:
            if (oled_screen == SYSTEM_INFO) {
                clear_text();
            }
            snprintf(network_rx_label, 32, "NRX:%2d MB/s", data[1]);
            break;
        case _NETWORK_TX:
            if (oled_screen == SYSTEM_INFO) {
                clear_text();
            }
            snprintf(network_tx_label, 32, "NTX:%2d MB/s", data[1]);
            break;
    }
}

void keyboard_post_init_user(void) {
  debug_enable=true;
}

#include QMK_KEYBOARD_H
#include "oled.c"

enum custom_keycodes {
    ENC_TOGG = SAFE_RANGE,
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    LAYOUT_hhkb(
        KC_ESC, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9, KC_0, KC_MINS, KC_EQL, KC_BSLS, KC_GRV,
        KC_TAB, KC_Q, KC_W, KC_E, KC_R, KC_T, KC_Y, KC_U, KC_I, KC_O, KC_P, KC_LBRC, KC_RBRC, KC_BSPC,
        KC_LCTL, KC_A, KC_S, KC_D, KC_F, KC_G, KC_H, KC_J, KC_K, KC_L, KC_SCLN, KC_QUOT, KC_ENT,
        KC_LSFT, KC_Z, KC_X, KC_C, KC_V, KC_B, KC_N, KC_M, KC_COMM, KC_DOT, KC_SLSH, KC_RSFT, KC_TRNS,
        KC_LALT, KC_LGUI, KC_SPC, KC_RGUI, KC_TRNS, KC_RALT)
};

#define NUM_ENC_MODES 3
enum encoder_modes {
    VOLUME,
    MOVE,
    OLED,
};

const char* modeNames[] = {"VOL", "MOV", "BRI", };

static uint8_t encoder_mode = VOLUME;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
  switch (keycode) {
    case ENC_TOGG:
      if (record->event.pressed) {
          encoder_mode = (encoder_mode + 1) % NUM_ENC_MODES;
      }
      break;
  }

  return false;
}

bool incr_oled_brightness(void) {
    uint8_t current_brightness = oled_get_brightness();

    if (current_brightness <= 223) {
        oled_set_brightness(current_brightness + 32);
    }

    return false;
}

bool decr_oled_brightness(void) {
    uint8_t current_brightness = oled_get_brightness();

    if (current_brightness >= 32) {
        oled_set_brightness(current_brightness - 32);
    }

    return false;
}

bool encoder_update_user(uint8_t index, bool clockwise) {
    if (index == 0) { /* First encoder */
        switch(encoder_mode) {
            case VOLUME:
            if (clockwise) {
                tap_code(KC_VOLU);
            } else {
                tap_code(KC_VOLD);
            }
            break;
            case MOVE:
            if (clockwise) {
                tap_code(KC_DOWN);
            } else {
                tap_code(KC_UP);
            }
            break;
            case OLED:
            if (clockwise) {
                incr_oled_brightness();
            } else {
                decr_oled_brightness();
            }
            break;
        }
    } else if (index == 1) {
        switch(encoder_mode) {
            case MOVE:
            if (clockwise) {
                tap_code(KC_RIGHT);
            } else {
                tap_code(KC_LEFT);
            }
            break;
        }
    }

    return false;
}

bool oled_task_user(void) {
    char wpm_str[10];
    char layer_str[10];

    render_anim();

    oled_set_cursor(0, 0);
    sprintf(wpm_str, "WPM:%03d", get_current_wpm());
    oled_write(wpm_str, false);

    oled_set_cursor(0, 1);
    sprintf(layer_str, "MOD:%s", modeNames[encoder_mode]);
    oled_write_P(layer_str, false);

    return false;
}

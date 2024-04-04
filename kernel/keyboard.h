//
// Created by Jannik on 02.04.2024.
//

#ifndef NIGHTOS_KEYBOARD_H
#define NIGHTOS_KEYBOARD_H

#include <stdbool.h>

#define KEY_A 65
#define KEY_B 66
#define KEY_C 67
#define KEY_D 68
#define KEY_E 69
#define KEY_F 70
#define KEY_G 71
#define KEY_H 72
#define KEY_I 73
#define KEY_J 74
#define KEY_K 75
#define KEY_L 76
#define KEY_M 77
#define KEY_N 78
#define KEY_O 79
#define KEY_P 80
#define KEY_Q 81
#define KEY_R 82
#define KEY_S 83
#define KEY_T 84
#define KEY_U 85
#define KEY_V 86
#define KEY_W 87
#define KEY_X 88
#define KEY_Y 89
#define KEY_Z 90

#define KEY_a 97
#define KEY_b 98
#define KEY_c 99
#define KEY_d 100
#define KEY_e 101
#define KEY_f 102
#define KEY_g 103
#define KEY_h 104
#define KEY_i 105
#define KEY_j 106
#define KEY_k 107
#define KEY_l 108
#define KEY_m 109
#define KEY_n 110
#define KEY_o 111
#define KEY_p 112
#define KEY_q 113
#define KEY_r 114
#define KEY_s 115
#define KEY_t 116
#define KEY_u 117
#define KEY_v 118
#define KEY_w 119
#define KEY_x 120
#define KEY_y 121
#define KEY_z 122

#define KEY_0 48
#define KEY_1 49
#define KEY_2 50
#define KEY_3 51
#define KEY_4 52
#define KEY_5 53
#define KEY_6 54
#define KEY_7 55
#define KEY_8 56
#define KEY_9 57
#define KEY_ENTER


typedef struct KeyEvent {
    int keyCode;
    bool isDown;
} key_event_t;

typedef struct KeyboardState {
    int kbd_extended_state;

    bool lshift;
    bool lctrl;
    bool lalt;

    bool rshift;
    bool rctrl;
    bool ralt;
} keyboard_state_t;

typedef void (*keyboard_event_handler_t)(key_event_t* keyEvent);

bool IsKeyDown(int keyCode);
void registerKeyEventHandler(keyboard_event_handler_t handler);
void ps2_init();

#endif //NIGHTOS_KEYBOARD_H

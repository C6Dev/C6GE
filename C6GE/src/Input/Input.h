#pragma once
#include <GLFW/glfw3.h>
#include "../Window/Window.h"

namespace input {

struct Keyboard {
    bool space;
    bool apostrophe;
    bool comma;
    bool minus;
    bool period;
    bool slash;
    bool zero;
    bool one;
    bool two;
    bool three;
    bool four;
    bool five;
    bool six;
    bool seven;
    bool eight;
    bool nine;
    bool semicolon;
    bool equal;
    bool a;
    bool b;
    bool c;
    bool d;
    bool e;
    bool f;
    bool g;
    bool h;
    bool i;
    bool j;
    bool k;
    bool l;
    bool m;
    bool n;
    bool o;
    bool p;
    bool q;
    bool r;
    bool s;
    bool t;
    bool u;
    bool v;
    bool w;
    bool x;
    bool y;
    bool z;
    bool left_bracket;
    bool backslash;
    bool right_bracket;
    bool grave_accent;
    bool world_1;
    bool world_2;
    bool escape;
    bool enter;
    bool tab;
    bool backspace;
    bool insert;
    bool del;
    bool right;
    bool left;
    bool down;
    bool up;
    bool page_up;
    bool page_down;
    bool home;
    bool end;
    bool caps_lock;
    bool scroll_lock;
    bool num_lock;
    bool print_screen;
    bool pause;
    bool f1;
    bool f2;
    bool f3;
    bool f4;
    bool f5;
    bool f6;
    bool f7;
    bool f8;
    bool f9;
    bool f10;
    bool f11;
    bool f12;
    bool f13;
    bool f14;
    bool f15;
    bool f16;
    bool f17;
    bool f18;
    bool f19;
    bool f20;
    bool f21;
    bool f22;
    bool f23;
    bool f24;
    bool f25;
    bool kp_0;
    bool kp_1;
    bool kp_2;
    bool kp_3;
    bool kp_4;
    bool kp_5;
    bool kp_6;
    bool kp_7;
    bool kp_8;
    bool kp_9;
    bool kp_decimal;
    bool kp_divide;
    bool kp_multiply;
    bool kp_subtract;
    bool kp_add;
    bool kp_enter;
    bool kp_equal;
    bool left_shift;
    bool left_control;
    bool left_alt;
    bool left_super;
    bool right_shift;
    bool right_control;
    bool right_alt;
    bool right_super;
    bool menu;
};

struct Mouse {
    bool button1; // Left
    bool button2; // Right
    bool button3; // Middle
    bool button4;
    bool button5;
    bool button6;
    bool button7;
    bool button8;
    double delta_x;
    double delta_y;
};

extern Keyboard key;
extern Mouse mouse;

void Update();
void EnableMouseCapture(bool enable);

}
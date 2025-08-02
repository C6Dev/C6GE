#include "Input.h"

namespace input {

Keyboard key;
Mouse mouse;

void Update() {
    GLFWwindow* win = C6GE::GetWindow();

    // Keyboard
    key.space = glfwGetKey(win, GLFW_KEY_SPACE) == GLFW_PRESS;
    key.apostrophe = glfwGetKey(win, GLFW_KEY_APOSTROPHE) == GLFW_PRESS;
    key.comma = glfwGetKey(win, GLFW_KEY_COMMA) == GLFW_PRESS;
    key.minus = glfwGetKey(win, GLFW_KEY_MINUS) == GLFW_PRESS;
    key.period = glfwGetKey(win, GLFW_KEY_PERIOD) == GLFW_PRESS;
    key.slash = glfwGetKey(win, GLFW_KEY_SLASH) == GLFW_PRESS;
    key.zero = glfwGetKey(win, GLFW_KEY_0) == GLFW_PRESS;
    key.one = glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS;
    key.two = glfwGetKey(win, GLFW_KEY_2) == GLFW_PRESS;
    key.three = glfwGetKey(win, GLFW_KEY_3) == GLFW_PRESS;
    key.four = glfwGetKey(win, GLFW_KEY_4) == GLFW_PRESS;
    key.five = glfwGetKey(win, GLFW_KEY_5) == GLFW_PRESS;
    key.six = glfwGetKey(win, GLFW_KEY_6) == GLFW_PRESS;
    key.seven = glfwGetKey(win, GLFW_KEY_7) == GLFW_PRESS;
    key.eight = glfwGetKey(win, GLFW_KEY_8) == GLFW_PRESS;
    key.nine = glfwGetKey(win, GLFW_KEY_9) == GLFW_PRESS;
    key.semicolon = glfwGetKey(win, GLFW_KEY_SEMICOLON) == GLFW_PRESS;
    key.equal = glfwGetKey(win, GLFW_KEY_EQUAL) == GLFW_PRESS;
    key.a = glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS;
    key.b = glfwGetKey(win, GLFW_KEY_B) == GLFW_PRESS;
    key.c = glfwGetKey(win, GLFW_KEY_C) == GLFW_PRESS;
    key.d = glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS;
    key.e = glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS;
    key.f = glfwGetKey(win, GLFW_KEY_F) == GLFW_PRESS;
    key.g = glfwGetKey(win, GLFW_KEY_G) == GLFW_PRESS;
    key.h = glfwGetKey(win, GLFW_KEY_H) == GLFW_PRESS;
    key.i = glfwGetKey(win, GLFW_KEY_I) == GLFW_PRESS;
    key.j = glfwGetKey(win, GLFW_KEY_J) == GLFW_PRESS;
    key.k = glfwGetKey(win, GLFW_KEY_K) == GLFW_PRESS;
    key.l = glfwGetKey(win, GLFW_KEY_L) == GLFW_PRESS;
    key.m = glfwGetKey(win, GLFW_KEY_M) == GLFW_PRESS;
    key.n = glfwGetKey(win, GLFW_KEY_N) == GLFW_PRESS;
    key.o = glfwGetKey(win, GLFW_KEY_O) == GLFW_PRESS;
    key.p = glfwGetKey(win, GLFW_KEY_P) == GLFW_PRESS;
    key.q = glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS;
    key.r = glfwGetKey(win, GLFW_KEY_R) == GLFW_PRESS;
    key.s = glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS;
    key.t = glfwGetKey(win, GLFW_KEY_T) == GLFW_PRESS;
    key.u = glfwGetKey(win, GLFW_KEY_U) == GLFW_PRESS;
    key.v = glfwGetKey(win, GLFW_KEY_V) == GLFW_PRESS;
    key.w = glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS;
    key.x = glfwGetKey(win, GLFW_KEY_X) == GLFW_PRESS;
    key.y = glfwGetKey(win, GLFW_KEY_Y) == GLFW_PRESS;
    key.z = glfwGetKey(win, GLFW_KEY_Z) == GLFW_PRESS;
    key.left_bracket = glfwGetKey(win, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    key.backslash = glfwGetKey(win, GLFW_KEY_BACKSLASH) == GLFW_PRESS;
    key.right_bracket = glfwGetKey(win, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    key.grave_accent = glfwGetKey(win, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS;
    key.world_1 = glfwGetKey(win, GLFW_KEY_WORLD_1) == GLFW_PRESS;
    key.world_2 = glfwGetKey(win, GLFW_KEY_WORLD_2) == GLFW_PRESS;
    key.escape = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    key.enter = glfwGetKey(win, GLFW_KEY_ENTER) == GLFW_PRESS;
    key.tab = glfwGetKey(win, GLFW_KEY_TAB) == GLFW_PRESS;
    key.backspace = glfwGetKey(win, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
    key.insert = glfwGetKey(win, GLFW_KEY_INSERT) == GLFW_PRESS;
    key.del = glfwGetKey(win, GLFW_KEY_DELETE) == GLFW_PRESS;
    key.right = glfwGetKey(win, GLFW_KEY_RIGHT) == GLFW_PRESS;
    key.left = glfwGetKey(win, GLFW_KEY_LEFT) == GLFW_PRESS;
    key.down = glfwGetKey(win, GLFW_KEY_DOWN) == GLFW_PRESS;
    key.up = glfwGetKey(win, GLFW_KEY_UP) == GLFW_PRESS;
    key.page_up = glfwGetKey(win, GLFW_KEY_PAGE_UP) == GLFW_PRESS;
    key.page_down = glfwGetKey(win, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;
    key.home = glfwGetKey(win, GLFW_KEY_HOME) == GLFW_PRESS;
    key.end = glfwGetKey(win, GLFW_KEY_END) == GLFW_PRESS;
    key.caps_lock = glfwGetKey(win, GLFW_KEY_CAPS_LOCK) == GLFW_PRESS;
    key.scroll_lock = glfwGetKey(win, GLFW_KEY_SCROLL_LOCK) == GLFW_PRESS;
    key.num_lock = glfwGetKey(win, GLFW_KEY_NUM_LOCK) == GLFW_PRESS;
    key.print_screen = glfwGetKey(win, GLFW_KEY_PRINT_SCREEN) == GLFW_PRESS;
    key.pause = glfwGetKey(win, GLFW_KEY_PAUSE) == GLFW_PRESS;
    key.f1 = glfwGetKey(win, GLFW_KEY_F1) == GLFW_PRESS;
    key.f2 = glfwGetKey(win, GLFW_KEY_F2) == GLFW_PRESS;
    key.f3 = glfwGetKey(win, GLFW_KEY_F3) == GLFW_PRESS;
    key.f4 = glfwGetKey(win, GLFW_KEY_F4) == GLFW_PRESS;
    key.f5 = glfwGetKey(win, GLFW_KEY_F5) == GLFW_PRESS;
    key.f6 = glfwGetKey(win, GLFW_KEY_F6) == GLFW_PRESS;
    key.f7 = glfwGetKey(win, GLFW_KEY_F7) == GLFW_PRESS;
    key.f8 = glfwGetKey(win, GLFW_KEY_F8) == GLFW_PRESS;
    key.f9 = glfwGetKey(win, GLFW_KEY_F9) == GLFW_PRESS;
    key.f10 = glfwGetKey(win, GLFW_KEY_F10) == GLFW_PRESS;
    key.f11 = glfwGetKey(win, GLFW_KEY_F11) == GLFW_PRESS;
    key.f12 = glfwGetKey(win, GLFW_KEY_F12) == GLFW_PRESS;
    key.f13 = glfwGetKey(win, GLFW_KEY_F13) == GLFW_PRESS;
    key.f14 = glfwGetKey(win, GLFW_KEY_F14) == GLFW_PRESS;
    key.f15 = glfwGetKey(win, GLFW_KEY_F15) == GLFW_PRESS;
    key.f16 = glfwGetKey(win, GLFW_KEY_F16) == GLFW_PRESS;
    key.f17 = glfwGetKey(win, GLFW_KEY_F17) == GLFW_PRESS;
    key.f18 = glfwGetKey(win, GLFW_KEY_F18) == GLFW_PRESS;
    key.f19 = glfwGetKey(win, GLFW_KEY_F19) == GLFW_PRESS;
    key.f20 = glfwGetKey(win, GLFW_KEY_F20) == GLFW_PRESS;
    key.f21 = glfwGetKey(win, GLFW_KEY_F21) == GLFW_PRESS;
    key.f22 = glfwGetKey(win, GLFW_KEY_F22) == GLFW_PRESS;
    key.f23 = glfwGetKey(win, GLFW_KEY_F23) == GLFW_PRESS;
    key.f24 = glfwGetKey(win, GLFW_KEY_F24) == GLFW_PRESS;
    key.f25 = glfwGetKey(win, GLFW_KEY_F25) == GLFW_PRESS;
    key.kp_0 = glfwGetKey(win, GLFW_KEY_KP_0) == GLFW_PRESS;
    key.kp_1 = glfwGetKey(win, GLFW_KEY_KP_1) == GLFW_PRESS;
    key.kp_2 = glfwGetKey(win, GLFW_KEY_KP_2) == GLFW_PRESS;
    key.kp_3 = glfwGetKey(win, GLFW_KEY_KP_3) == GLFW_PRESS;
    key.kp_4 = glfwGetKey(win, GLFW_KEY_KP_4) == GLFW_PRESS;
    key.kp_5 = glfwGetKey(win, GLFW_KEY_KP_5) == GLFW_PRESS;
    key.kp_6 = glfwGetKey(win, GLFW_KEY_KP_6) == GLFW_PRESS;
    key.kp_7 = glfwGetKey(win, GLFW_KEY_KP_7) == GLFW_PRESS;
    key.kp_8 = glfwGetKey(win, GLFW_KEY_KP_8) == GLFW_PRESS;
    key.kp_9 = glfwGetKey(win, GLFW_KEY_KP_9) == GLFW_PRESS;
    key.kp_decimal = glfwGetKey(win, GLFW_KEY_KP_DECIMAL) == GLFW_PRESS;
    key.kp_divide = glfwGetKey(win, GLFW_KEY_KP_DIVIDE) == GLFW_PRESS;
    key.kp_multiply = glfwGetKey(win, GLFW_KEY_KP_MULTIPLY) == GLFW_PRESS;
    key.kp_subtract = glfwGetKey(win, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
    key.kp_add = glfwGetKey(win, GLFW_KEY_KP_ADD) == GLFW_PRESS;
    key.kp_enter = glfwGetKey(win, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
    key.kp_equal = glfwGetKey(win, GLFW_KEY_KP_EQUAL) == GLFW_PRESS;
    key.left_shift = glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
    key.left_control = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    key.left_alt = glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;
    key.left_super = glfwGetKey(win, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS;
    key.right_shift = glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    key.right_control = glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    key.right_alt = glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    key.right_super = glfwGetKey(win, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;
    key.menu = glfwGetKey(win, GLFW_KEY_MENU) == GLFW_PRESS;

    // Mouse
    mouse.button1 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_1) == GLFW_PRESS;
    mouse.button2 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_2) == GLFW_PRESS;
    mouse.button3 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_3) == GLFW_PRESS;
    mouse.button4 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_4) == GLFW_PRESS;
    mouse.button5 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_5) == GLFW_PRESS;
    mouse.button6 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_6) == GLFW_PRESS;
    mouse.button7 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_7) == GLFW_PRESS;
    mouse.button8 = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_8) == GLFW_PRESS;

    // Mouse movement
    static bool firstMouse = true;
    static double lastX = 0.0;
    static double lastY = 0.0;
    double xpos, ypos;
    glfwGetCursorPos(win, &xpos, &ypos);
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
        mouse.delta_x = 0.0;
        mouse.delta_y = 0.0;
    } else {
        mouse.delta_x = xpos - lastX;
        mouse.delta_y = lastY - ypos; // Reversed for typical camera control
        lastX = xpos;
        lastY = ypos;
    }
}

void EnableMouseCapture(bool enable) {
    GLFWwindow* win = C6GE::GetWindow();
    glfwSetInputMode(win, GLFW_CURSOR, enable ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

}
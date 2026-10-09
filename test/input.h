#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

typedef struct {
    struct libinput* libinput;
    struct udev* udev;

    void (*mouse_motion)(int, int, void*);
    void (*mouse_btn_pressed)(uint32_t, void*);
    void (*mouse_btn_released)(uint32_t, void*);
    void (*mouse_scroll_vertical)(double, void*);
    void (*mouse_scroll_horisontal)(double, void*);
    void (*keyboard_key_pressed)(uint32_t, void*);
    void (*keyboard_key_released)(uint32_t, void*);
} WLL_InputHandler;

void WLL_InputHandlerInit(char* seat, WLL_InputHandler* handler);
void WLL_InputHandle(WLL_InputHandler* handler, void* data); 
#endif 

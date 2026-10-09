    #include "input.h"
#include <errno.h>
#include <fcntl.h>
#include <libinput.h>
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <unistd.h>

static int WLL_InputHandlerOpenRestricted(const char* path, int flags, void* data) {
     int fd = open(path, flags);
     return fd < 0 ? -errno : fd;
}

static void WLL_InputHandlerCloseRestricted(int fd, void* data) {
    close(fd);
}

static const struct libinput_interface WLL_InputHandlerInterface = {
    .open_restricted = WLL_InputHandlerOpenRestricted,
    .close_restricted = WLL_InputHandlerCloseRestricted,
};

void WLL_InputHandlerInit(char* seat, WLL_InputHandler* handler) {
    handler->udev = udev_new();
    if (!handler->udev) {
        printf("failed to create udev\n");
        exit(1);
    }

    handler->libinput = libinput_udev_create_context(&WLL_InputHandlerInterface, NULL, handler->udev);
    if (!handler->libinput) {
        printf("failed to create libinput context\n");
        exit(1);
    }

    if (libinput_udev_assign_seat(handler->libinput, seat) != 0) {
        printf("failed to assign seat\n");
        exit(1);
    }
}

void WLL_InputHandle(WLL_InputHandler* handler, void* data) {
    libinput_dispatch (handler->libinput);
    struct pollfd fd = {libinput_get_fd(handler->libinput), POLLIN};
    poll(&fd, 1, 0);
    struct libinput_event* event;

    while((event = libinput_get_event(handler->libinput)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(event);
        switch (type) {
            case LIBINPUT_EVENT_POINTER_MOTION: {
                struct libinput_event_pointer* ptr = libinput_event_get_pointer_event(event);
                if (!handler->mouse_motion) {
                    printf("plese implement mouse_motion in input_handler\n");
                    exit(1);
                }
                handler->mouse_motion(
                        libinput_event_pointer_get_dx(ptr), 
                        libinput_event_pointer_get_dy(ptr),
                        data
                        );
                break;
            }

            case LIBINPUT_EVENT_POINTER_BUTTON: {
                struct libinput_event_pointer* ptr = libinput_event_get_pointer_event(event);
                uint32_t button = libinput_event_pointer_get_button(ptr);
                enum libinput_button_state state = libinput_event_pointer_get_button_state(ptr);

                if (state == LIBINPUT_BUTTON_STATE_PRESSED) {
                    if (!handler->mouse_btn_pressed) {
                        printf("plese implement mouse_btn_pressed in input_handler\n");
                        exit(1);
                    }
                    handler->mouse_btn_pressed(button, data);
                } else {
                    if (!handler->mouse_btn_released) {
                        printf("plese implement mouse_btn_released in input_handler\n");
                        exit(1);
                    }
                    handler->mouse_btn_released(button, data);
                }
                break;
            }

            case LIBINPUT_EVENT_POINTER_AXIS: {
                struct libinput_event_pointer *ptr = 
                libinput_event_get_pointer_event(event);

              if (libinput_event_pointer_has_axis(ptr, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
                  double value = libinput_event_pointer_get_axis_value(ptr, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
                  if (!handler->mouse_scroll_vertical) {
                    printf("plese implement mouse_scroll_vertical in input_handler\n");
                    exit(1);
                  }
                  handler->mouse_scroll_vertical(value, data);
              }

              if (libinput_event_pointer_has_axis(ptr, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
                      double value = libinput_event_pointer_get_axis_value(ptr, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
                      if (!handler->mouse_scroll_horisontal) {
                        printf("plese implement mouse_scroll_horisontal in input_handler\n");
                        exit(1);
                        }
                      handler->mouse_scroll_horisontal(value, data);
                }
                  break;
            }

          case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *kbd = libinput_event_get_keyboard_event(event);
            uint32_t keycode = libinput_event_keyboard_get_key(kbd);
            enum libinput_key_state state = libinput_event_keyboard_get_key_state(kbd);

            if (state == LIBINPUT_KEY_STATE_PRESSED) {
                if (!handler->keyboard_key_pressed) {
                    printf("plese implement keyboard_key_pressed in input_handler\n");
                    exit(1);
                }
                handler->keyboard_key_pressed(keycode, data);
            } else {
                if (!handler->keyboard_key_released) {
                    printf("plese implement keyboard_key_released in input_handler\n");
                    exit(1);
                }
                handler->keyboard_key_released(keycode, data);
            }
            break;
        }

        default:
            printf("event %d is not supported\n", libinput_event_get_type(event));
        }
        }
        libinput_event_destroy(event);
}

#include <GLES2/gl2.h>
#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <unistd.h>
#include <libinput.h>
#include <libudev.h>
#include <poll.h>
#include <linux/input-event-codes.h>
#include <wayland-server.h>
#include "drm.h"
#include "input.h"
#include <pixman.h>


void keypressed(uint32_t key, void* data) {
    printf("key pressed: %d\n", key);

    if (key == KEY_Q) {
        exit(0);
    }
}
void keyreleased(uint32_t key, void* data) {
    printf("key relased: %d\n", key);
}

typedef struct {
    struct wl_display* display;
    struct wl_event_loop* event_loop;
    struct wl_list surfaces;
} Server;

typedef struct {
    struct wl_list link;
    struct wl_resource* resource;
    struct wl_resource* buffer;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    struct pixman_region32 pending_damage;
    struct pixman_region32 current_damage;
} Surface;

void WLL_SurfaceAttach(struct wl_client* client, struct wl_resource* resource,
        struct wl_resource* buffer, int32_t x, int32_t y) {
    Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        printf("failed to get surface while attaching\n");
        return;
    }

    surface->buffer = buffer;
    surface->x = x;
    surface->y = y;
}

void WLL_SurfaceCommit(struct wl_client* client, struct wl_resource* resource) {
    Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        printf("failed to get surface while commiting\n");
        return;
    }

    if (!surface->buffer) return; 
    if (pixman_region32_empty(&surface->pending_damage)) return;
    
    if (output_size_changed) {
        if (!surface->current_damage.data) pixman_region32_init(&surface->current_damage);
        pixman_region32_clear(&surface->current_damage);
        pixman_region32_union(&surface->current_damage, &surface->current_damage, &surface->pending_damage);
        pixman_region32_clear(&surface->pending_damage);
        pixman_region32_intersect_rect(
                &surface->current_damage, 
                &surface->current_damage, 
                0, 0, 
                surface->width,  surface->height
                );
    }

    implement the renderer;

    wl_buffer_send_release(surface->buffer);

    pixman_region32_clear(&surface->current_damage);
    pixman_region32_union_rect(&surface->current_damage,
                               &surface->current_damage, 0, 0, surface->width, surface->height);
    pixman_region32_clear(&surface->pending_damage);

    set damage regions;

}

void WLL_SurfaceFrame(struct wl_client* client, struct wl_resource* resource, uint32_t callback) {
    Surface* surface = wl_resource_get_user_data(resource);
    if (!surface) {
        printf("failed to get surface while commiting\n");
        return;
    }

}

struct wl_surface_interface WLL_surface_impl = {
    .attach = WLL_SurfaceAttach,
    .commit = WLL_SurfaceCommit,
};

void WLL_DestroySurface(struct wl_resource* resource) {
    Surface* surface = wl_resource_get_user_data(resource);

    pixman_region32_fini(&surface->pending_damage);
    pixman_region32_fini(&surface->current_damage);

    wl_list_remove(&surface->link);
    wl_resource_destroy(surface->resource);
    free(surface);
}

void WLL_CreateSurface(struct wl_client* client, struct wl_resource* resource, uint32_t id) {
    Server* server = wl_resource_get_user_data(resource);

    Surface* surface = malloc(sizeof(Surface));

    surface->resource = wl_resource_create(
        client,
        &wl_surface_interface,
        wl_resource_get_version(resource),
        id
    );

    if(!surface->resource) {
        wl_client_post_no_memory(client);
        return;
    }

    pixman_region32_init(&surface->pending_damage);
    pixman_region32_init(&surface->current_damage);

    wl_resource_set_implementation(surface->resource,  &WLL_surface_impl, surface, WLL_DestroySurface);

    wl_list_init(&server->surfaces);
    wl_list_insert(&server->surfaces, &surface->link);
}

void WLL_CreateRegion(struct wl_client* client, struct wl_resource* resource, uint32_t id) {}

static const struct wl_compositor_interface WLL_compositor_impl = {
    .create_surface = WLL_CreateSurface,
    .create_region = WLL_CreateRegion,
};

void WLL_CompositorBind(struct wl_client* client, void* data, uint32_t version, uint32_t id) {
    struct wl_resource* res = wl_resource_create(client, &wl_compositor_interface, version, id);
    if (res == NULL)  {
        wl_client_post_no_memory(client);
        return;
    }
    wl_resource_set_implementation(res, &WLL_compositor_impl, data, NULL);
}

int main() {
    WLL_Monitor mon = {0};
    WLL_DrmInit("/dev/dri/card0", &mon);
    WLL_GbmInit(&mon);
    WLL_EglInit(&mon);
    WLL_DrmModeset(&mon);
    WLL_InputHandler input_handler = {0};

    input_handler.keyboard_key_pressed = keypressed;
    input_handler.keyboard_key_released = keyreleased;
    WLL_InputHandlerInit("seat0", &input_handler);

    Server server = {0};
    server.display = wl_display_create();
    if (!server.display) {
        printf("failed to create display\n");
        exit(1);
    }

    const char* socket = wl_display_add_socket_auto(server.display);
    if (!socket) {
        printf("failed to add socket\n");
        wl_display_destroy(server.display);
        exit(1);
    }

    wl_list_init(&server.surfaces);

    setenv("WAYLAND_DISPLAY", socket, 1);
server.event_loop = wl_display_get_event_loop(server.display);
    struct pollfd loopfd = {wl_event_loop_get_fd(server.event_loop), POLLIN};
    wl_global_create(server.display, &wl_compositor_interface, 4, NULL, WLL_CompositorBind);

    while(1) {
        WLL_InputHandle(&input_handler, NULL);
        glClearColor(0.6, 0.9, 0.4, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
WLL_DrmPresent(&mon);
        
        poll(&loopfd, 1, 0);
        wl_event_loop_dispatch(server.event_loop, 0);
    }
    return 0;
}

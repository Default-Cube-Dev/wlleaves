#include "drm_fourcc.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <errno.h>
#include <fcntl.h>
#include <gbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "drm.h"

void WLL_DrmInit(const char* path, WLL_Monitor* mon) {
    mon->gpu_fd = open(path, O_RDWR | O_NONBLOCK);
    if (mon->gpu_fd < 0) {
        printf("failed to open %s: %s\n", path, strerror(errno));
        exit(1);
    }

    if (drmSetMaster(mon->gpu_fd)) {
        printf("failed to set DRM master: %s\n", strerror(errno));
        exit(1);
    }

    mon->resources = drmModeGetResources(mon->gpu_fd);
    if (!mon->resources) {
        printf("failed to get drm resources for %s: %s\n", path, strerror(errno));
        exit(1);
    }

    mon->connector = NULL;
    for (int i = 0; i < mon->resources->count_connectors; i++) {
        drmModeConnector* connector =
            drmModeGetConnector(mon->gpu_fd, mon->resources->connectors[i]);

        if (!connector)
            continue;

        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
            mon->connector = connector;
            break;
        }

        drmModeFreeConnector(connector);
    }
    if (!mon->connector) {
        printf("failed to find a connector for %s: %s\n", path, strerror(errno));
        exit(1);
    }

    mon->mode = NULL;
    for (int i = 0; i < mon->connector->count_modes; i++) {
        if (mon->connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
            mon->mode = &mon->connector->modes[i];
            break;
        }
    }

    if (mon->mode == NULL) {
        printf("failed to find a mode for %s: %s\n", path, strerror(errno));
        exit(1);
    }

    mon->encoder = NULL;
    for (int i = 0; i < mon->resources->count_encoders; i++) {
        drmModeEncoder* encoder = drmModeGetEncoder(mon->gpu_fd, mon->resources->encoders[i]);
        if (!encoder)
            continue;
        if (encoder->encoder_id == mon->connector->encoder_id) {
            mon->encoder = encoder;
            break;
        }
        drmModeFreeEncoder(encoder);
    }

    if (!mon->encoder) {
        for (int i = 0; i < mon->resources->count_encoders; i++) {
            drmModeEncoder* encoder = drmModeGetEncoder(mon->gpu_fd, mon->resources->encoders[i]);
            if (!encoder)
                continue;
            mon->encoder = encoder;
            break;
        }
    }

    if (!mon->encoder) {
        printf("failed to find an encoder for %s: %s\n", path, strerror(errno));
        exit(1);
    }

    mon->crtc = drmModeGetCrtc(mon->gpu_fd, mon->encoder->crtc_id);
    if (!mon->crtc) {
        printf("failed to find a crtc for %s: %s\n", path, strerror(errno));
        exit(1);
    }

}

void WLL_GbmInit(WLL_Monitor* mon) {
    mon->egl.gbm = gbm_create_device(mon->gpu_fd);
    if (!mon->egl.gbm) {
        printf("failed to create gbm device: %s\n", strerror(errno));
        exit(1);
    }

    mon->egl.gbm_surface = gbm_surface_create(
            mon->egl.gbm,
            mon->mode->hdisplay, mon->mode->vdisplay,
            GBM_BO_FORMAT_XRGB8888,
            GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT
            );
    if (!mon->egl.gbm_surface) {
        printf("failed to create gbm surface: %s\n", strerror(errno));
        exit(1);
    }

}

void WLL_EglInit(WLL_Monitor* mon) {
    static const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };
    static const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE,
    };
    EGLint major, minor;
    EGLint matched;

    mon->egl.get_platform_display_ext =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
        mon->egl.display = mon->egl.get_platform_display_ext(EGL_PLATFORM_GBM_KHR, (void*)mon->egl.gbm, NULL);

    if (mon->egl.display == EGL_NO_DISPLAY) {
        printf("casting\n");
        mon->egl.display = mon->egl.get_platform_display_ext(EGL_PLATFORM_GBM_KHR, mon->egl.gbm, NULL);
    }
    if (mon->egl.display == EGL_NO_DISPLAY) {
        fprintf(stderr, "eglGetDisplay failed\n");
        exit(1);
    }

    if (!eglInitialize(mon->egl.display, &major, &minor)) {
        printf("failed to init egl\n");
        exit(1);
    }

    printf("EGL v%d.%d\n", major, minor);

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		printf("failed to bind egl api \n");
        exit(1);
	}

    EGLint num_configs;
    if (!eglGetConfigs(mon->egl.display, NULL, 0, &num_configs)) {
        printf("failed to get configs\n");
        exit(1);
    }

    EGLConfig* configs = malloc(sizeof(EGLConfig)*num_configs);

    if (!eglGetConfigs(mon->egl.display, configs, num_configs, &num_configs)) {
        printf("failed to get configs (this time the second time)\n");
        exit(1);
    }

    for (EGLint i = 0; i<num_configs; i++) {
        EGLint visual = 0;
        if (!eglGetConfigAttrib(mon->egl.display, configs[i], EGL_NATIVE_VISUAL_ID, &visual)) {
            printf("failed to get visual id for config: %d\n", i);
            exit(1);
        }

        if (visual == DRM_FORMAT_XRGB8888) {
            mon->egl.config = configs[i];
            free(configs);
            configs = NULL;
            printf("found config!\n");
            break;
        }
    }

    if (configs != NULL) {
        printf("failed to match visual id\n");
        printf("%d\n", num_configs);
        exit(1);
    }

    mon->egl.context = eglCreateContext(mon->egl.display, mon->egl.config,
            EGL_NO_CONTEXT, context_attribs);

    if (mon->egl.context == EGL_NO_CONTEXT) {
        printf("failed to create context\n");
        exit(1);
    }

     if (mon->egl.gbm_surface) {
        mon->egl.surface = eglCreateWindowSurface(mon->egl.display, mon->egl.config,
                (EGLNativeWindowType)mon->egl.gbm_surface, NULL);
    } else {
        printf("failed to create egl surface: no gbm surface");
        exit(1);
    }

     if (mon->egl.surface == EGL_NO_SURFACE) {

        printf("failed to create egl surface: display: %p, config: %p, gbm_surface: %p\n", mon->egl.display, mon->egl.config, mon->egl.gbm_surface);
        exit(1);
        
     }

    if(!eglMakeCurrent(mon->egl.display, mon->egl.surface, mon->egl.surface, mon->egl.context)) {
        printf("failed to eglMakeCurrent: 0x%x\n", eglGetError());
        exit(1);
    }
}
static void WLL_DrmPageFlipHandler(int fd, unsigned int frame,
                              unsigned int sec, unsigned int usec, void *data) {
    int *flipped = data;
    *flipped = 1;
}

void WLL_DrmPresent(WLL_Monitor* mon) {
    drmEventContext ev = {
        .version = DRM_EVENT_CONTEXT_VERSION,
        .page_flip_handler = WLL_DrmPageFlipHandler
    };

    if (!eglSwapBuffers(mon->egl.display, mon->egl.surface)) {
        fprintf(stderr, "eglSwapBuffers failed: 0x%x\n", eglGetError());
        exit(1);
    }

    struct gbm_bo *bo = gbm_surface_lock_front_buffer(mon->egl.gbm_surface);
    if (!bo) {
        fprintf(stderr, "gbm_surface_lock_front_buffer failed: %s\n", strerror(errno));
        exit(1);
    }

    uint32_t fb = 0;
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t stride = gbm_bo_get_stride(bo);

    if (drmModeAddFB2(mon->gpu_fd,
                      gbm_bo_get_width(bo),
                      gbm_bo_get_height(bo),
                      DRM_FORMAT_XRGB8888,
                      (uint32_t[]){ handle, 0, 0, 0 },
                      (uint32_t[]){ stride, 0, 0, 0 },
                      (uint32_t[]){ 0, 0, 0, 0 },
                      &fb,
                      0)) {
        fprintf(stderr, "drmModeAddFB2 failed: %s\n", strerror(errno));
        gbm_surface_release_buffer(mon->egl.gbm_surface, bo);
        exit(1);
    }

    int flipped = 0;
    if (drmModePageFlip(mon->gpu_fd, mon->crtc->crtc_id, fb,
                        DRM_MODE_PAGE_FLIP_EVENT, &flipped)) {
        fprintf(stderr, "drmModePageFlip failed: %s\n", strerror(errno));
        drmModeRmFB(mon->gpu_fd, fb);
        gbm_surface_release_buffer(mon->egl.gbm_surface, bo);
        exit(1);
    }

    while (!flipped) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(mon->gpu_fd, &fds);

        int ret = select(mon->gpu_fd + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "select failed: %s\n", strerror(errno));
            drmModeRmFB(mon->gpu_fd, fb);
            gbm_surface_release_buffer(mon->egl.gbm_surface, bo);
            exit(1);
        }

        if (FD_ISSET(mon->gpu_fd, &fds)) {
            drmHandleEvent(mon->gpu_fd, &ev);
        }
    }

    if (mon->egl.fb) {
        drmModeRmFB(mon->gpu_fd, mon->egl.fb);
    }
    if (mon->egl.gbm_bo) {
        gbm_surface_release_buffer(mon->egl.gbm_surface, mon->egl.gbm_bo);
    }

    mon->egl.fb = fb;
    mon->egl.gbm_bo = bo;
}

void WLL_DrmModeset(WLL_Monitor* mon) {
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    WLL_DrmPresent(mon);

    if (drmModeSetCrtc(mon->gpu_fd, mon->crtc->crtc_id, mon->egl.fb, 0, 0, &mon->connector->connector_id, 1, mon->mode)) {
        printf("failed to Modeset: %s\n", strerror(errno));
        exit(1);
    }

}

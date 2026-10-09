#ifndef DRM_H
#define DRM_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <xf86drmMode.h>
typedef struct {
    struct gbm_device* gbm;
    struct gbm_surface* gbm_surface;
    struct gbm_bo* gbm_bo;
    uint32_t fb;
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display_ext;
} WLL_Egl;

typedef struct {
    int gpu_fd;
    drmModeRes* resources;
    drmModeConnector* connector;
    drmModeModeInfo* mode;
    drmModeEncoder* encoder;
    drmModeCrtc* crtc;
    WLL_Egl egl;
} WLL_Monitor;

void WLL_DrmInit(const char* path, WLL_Monitor* mon);
void WLL_GbmInit(WLL_Monitor* mon);
void WLL_EglInit(WLL_Monitor* mon);
void WLL_DrmPresent(WLL_Monitor* mon);
void WLL_DrmModeset(WLL_Monitor* mon); 

#endif

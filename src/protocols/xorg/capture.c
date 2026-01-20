/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"

#include "capture.h"
#include "client.h"

#include <guacamole/client.h>
#include <guacamole/rect.h>

#include <X11/Xutil.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <string.h>

static int guac_xorg_xshm_error = 0;

static int guac_xorg_xshm_error_handler(Display* display, XErrorEvent* error) {
    (void) display;
    if (error->error_code == BadAccess || error->error_code == BadImplementation)
        guac_xorg_xshm_error = 1;
    return 0;
}

static void guac_xorg_capture_destroy_shm(guac_xorg_capture* capture,
        Display* display) {

    if (capture->shm_attached) {
        XShmDetach(display, &capture->shm_info);
        capture->shm_attached = 0;
    }

    if (capture->shm_info.shmaddr != (char*) -1
            && capture->shm_info.shmaddr != NULL) {
        shmdt(capture->shm_info.shmaddr);
        capture->shm_info.shmaddr = (char*) -1;
    }

    if (capture->shm_info.shmid != -1) {
        shmctl(capture->shm_info.shmid, IPC_RMID, NULL);
        capture->shm_info.shmid = -1;
    }

    if (capture->shm_image != NULL) {
        XDestroyImage(capture->shm_image);
        capture->shm_image = NULL;
    }

    capture->shm_width = 0;
    capture->shm_height = 0;
}

int guac_xorg_capture_init(guac_client* client, guac_xorg_client* xorg_client) {

    guac_xorg_capture* capture = &xorg_client->capture;
    memset(capture, 0, sizeof(*capture));
    capture->shm_info.shmid = -1;
    capture->shm_info.shmaddr = (char*) -1;

    int damage_error_base = 0;
    if (XDamageQueryExtension(xorg_client->x_display,
                &capture->damage_event_base, &damage_error_base)) {
        capture->damage_available = 1;
        capture->damage = XDamageCreate(xorg_client->x_display,
                xorg_client->root_window, XDamageReportNonEmpty);
        XSync(xorg_client->x_display, False);
    }
    else
        guac_client_log(client, GUAC_LOG_WARNING,
                "XDamage extension unavailable; falling back to full capture.");

    capture->xshm_available = XShmQueryExtension(xorg_client->x_display);
    const char* disable_xshm = getenv("GUAC_XORG_DISABLE_XSHM");
    if (disable_xshm != NULL && *disable_xshm != '\0'
            && strcmp(disable_xshm, "0") != 0)
        capture->xshm_available = 0;
    if (!capture->xshm_available)
        guac_client_log(client, GUAC_LOG_DEBUG,
                "MIT-SHM unavailable; using XGetImage.");

    return 0;
}

void guac_xorg_capture_free(guac_xorg_client* xorg_client) {
    if (xorg_client == NULL)
        return;

    guac_xorg_capture* capture = &xorg_client->capture;

    if (capture->damage_available)
        XDamageDestroy(xorg_client->x_display, capture->damage);

    if (capture->xshm_available)
        guac_xorg_capture_destroy_shm(capture, xorg_client->x_display);

    capture->damage_available = 0;
    capture->xshm_available = 0;
}

static int guac_xorg_capture_prepare_shm(guac_xorg_capture* capture,
        Display* display, int width, int height) {

    if (!capture->xshm_available)
        return 0;

    if (capture->shm_image != NULL
            && capture->shm_width == width
            && capture->shm_height == height)
        return 1;

    guac_xorg_capture_destroy_shm(capture, display);

    capture->shm_image = XShmCreateImage(display, DefaultVisual(display, 0),
            DefaultDepth(display, 0), ZPixmap, NULL,
            &capture->shm_info, width, height);
    if (capture->shm_image == NULL)
        goto fail;

    int size = capture->shm_image->bytes_per_line * capture->shm_image->height;
    capture->shm_info.shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
    if (capture->shm_info.shmid < 0)
        goto fail;

    capture->shm_info.shmaddr = (char*) shmat(capture->shm_info.shmid, NULL, 0);
    capture->shm_info.readOnly = False;
    if (capture->shm_info.shmaddr == (char*) -1)
        goto fail;

    capture->shm_image->data = capture->shm_info.shmaddr;

    guac_xorg_xshm_error = 0;
    XErrorHandler previous = XSetErrorHandler(guac_xorg_xshm_error_handler);
    if (!XShmAttach(display, &capture->shm_info))
        goto fail;
    XSync(display, False);
    XSetErrorHandler(previous);
    if (guac_xorg_xshm_error) {
        capture->xshm_available = 0;
        goto fail;
    }
    capture->shm_attached = 1;
    capture->shm_width = width;
    capture->shm_height = height;
    return 1;

fail:
    guac_xorg_capture_destroy_shm(capture, display);
    return 0;
}

int guac_xorg_capture_image(guac_xorg_client* xorg_client,
        const guac_rect* src_rect, XImage** out_image, int* out_owned) {

    if (xorg_client == NULL || out_image == NULL || out_owned == NULL)
        return 1;

    int width = src_rect->right - src_rect->left;
    int height = src_rect->bottom - src_rect->top;
    if (width <= 0 || height <= 0)
        return 1;

    guac_xorg_capture* capture = &xorg_client->capture;

    if (capture->xshm_available
            && guac_xorg_capture_prepare_shm(capture,
                xorg_client->x_display, width, height)) {

        guac_xorg_xshm_error = 0;
        XErrorHandler previous = XSetErrorHandler(guac_xorg_xshm_error_handler);
        if (!XShmGetImage(xorg_client->x_display, xorg_client->root_window,
                    capture->shm_image, src_rect->left, src_rect->top,
                    AllPlanes)) {
            XSync(xorg_client->x_display, False);
            XSetErrorHandler(previous);
            return 1;
        }
        XSync(xorg_client->x_display, False);
        XSetErrorHandler(previous);
        if (guac_xorg_xshm_error) {
            capture->xshm_available = 0;
            return 1;
        }

        *out_image = capture->shm_image;
        *out_owned = 0;
        return 0;
    }

    XImage* image = XGetImage(xorg_client->x_display,
            xorg_client->root_window, src_rect->left, src_rect->top,
            width, height, AllPlanes, ZPixmap);
    if (image == NULL)
        return 1;

    *out_image = image;
    *out_owned = 1;
    return 0;
}

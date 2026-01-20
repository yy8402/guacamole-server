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

#include "display.h"
#include "capture.h"
#include "cursor.h"
#include "scale.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/rect.h>
#include <guacamole/socket.h>

#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static void guac_xorg_free_maps(guac_xorg_client* xorg_client) {
    free(xorg_client->x_map);
    free(xorg_client->y_map);
    xorg_client->x_map = NULL;
    xorg_client->y_map = NULL;
    xorg_client->map_output_width = 0;
    xorg_client->map_output_height = 0;
    xorg_client->map_capture_width = 0;
    xorg_client->map_capture_height = 0;
}

static void guac_xorg_update_maps(guac_xorg_client* xorg_client) {
    int output_width = xorg_client->width;
    int output_height = xorg_client->height;
    int capture_width = xorg_client->capture_width;
    int capture_height = xorg_client->capture_height;

    if (output_width <= 0 || output_height <= 0
            || capture_width <= 0 || capture_height <= 0) {
        guac_xorg_free_maps(xorg_client);
        return;
    }

    if (xorg_client->map_output_width == output_width
            && xorg_client->map_output_height == output_height
            && xorg_client->map_capture_width == capture_width
            && xorg_client->map_capture_height == capture_height)
        return;

    guac_xorg_free_maps(xorg_client);

    xorg_client->x_map = calloc(output_width, sizeof(int));
    xorg_client->y_map = calloc(output_height, sizeof(int));
    if (xorg_client->x_map == NULL || xorg_client->y_map == NULL) {
        guac_xorg_free_maps(xorg_client);
        return;
    }

    for (int x = 0; x < output_width; x++) {
        int src_x = (int) (((long long) x * capture_width) / output_width);
        if (src_x >= capture_width)
            src_x = capture_width - 1;
        xorg_client->x_map[x] = src_x;
    }

    for (int y = 0; y < output_height; y++) {
        int src_y = (int) (((long long) y * capture_height) / output_height);
        if (src_y >= capture_height)
            src_y = capture_height - 1;
        xorg_client->y_map[y] = src_y;
    }

    xorg_client->map_output_width = output_width;
    xorg_client->map_output_height = output_height;
    xorg_client->map_capture_width = capture_width;
    xorg_client->map_capture_height = capture_height;
}

static int guac_xorg_mask_shift(unsigned long mask) {
    int shift = 0;
    if (mask == 0)
        return 0;
    while ((mask & 1) == 0) {
        mask >>= 1;
        shift++;
    }
    return shift;
}

static unsigned long guac_xorg_mask_max(unsigned long mask, int shift) {
    return (mask >> shift);
}

static int guac_xorg_prepare_format(guac_xorg_client* xorg_client,
        XImage* image) {

    if (image->bits_per_pixel < 24)
        return 0;

    xorg_client->red_mask = image->red_mask;
    xorg_client->green_mask = image->green_mask;
    xorg_client->blue_mask = image->blue_mask;

    xorg_client->red_shift = guac_xorg_mask_shift(image->red_mask);
    xorg_client->green_shift = guac_xorg_mask_shift(image->green_mask);
    xorg_client->blue_shift = guac_xorg_mask_shift(image->blue_mask);

    xorg_client->red_max = guac_xorg_mask_max(image->red_mask,
            xorg_client->red_shift);
    xorg_client->green_max = guac_xorg_mask_max(image->green_mask,
            xorg_client->green_shift);
    xorg_client->blue_max = guac_xorg_mask_max(image->blue_mask,
            xorg_client->blue_shift);

    if (xorg_client->red_max == 0 || xorg_client->green_max == 0
            || xorg_client->blue_max == 0)
        return 0;

    return 1;
}

static void guac_xorg_update_dimensions(guac_xorg_client* xorg_client,
        const XWindowAttributes* attrs) {

    xorg_client->capture_width = attrs->width;
    xorg_client->capture_height = attrs->height;

    if (xorg_client->settings->width == 0)
        xorg_client->width = attrs->width;
    if (xorg_client->settings->height == 0)
        xorg_client->height = attrs->height;
}

static void guac_xorg_union_damage(guac_xorg_client* xorg_client,
        const guac_rect* rect) {

    if (!xorg_client->damage_pending) {
        xorg_client->damage_rect = *rect;
        xorg_client->damage_pending = 1;
        clock_gettime(CLOCK_MONOTONIC, &xorg_client->last_damage_time);
        return;
    }

    guac_rect_extend(&xorg_client->damage_rect, rect);
}

static void guac_xorg_map_damage(const guac_xorg_client* xorg_client,
        const guac_rect* src_rect, guac_rect* dst_rect) {

    int capture_width = xorg_client->capture_width;
    int capture_height = xorg_client->capture_height;
    int output_width = xorg_client->width;
    int output_height = xorg_client->height;

    int dst_left = (int) (((long long) src_rect->left * output_width)
            / capture_width);
    int dst_top = (int) (((long long) src_rect->top * output_height)
            / capture_height);
    int dst_right = (int) (((long long) src_rect->right * output_width)
            / capture_width);
    int dst_bottom = (int) (((long long) src_rect->bottom * output_height)
            / capture_height);

    if (dst_right <= dst_left)
        dst_right = dst_left + 1;
    if (dst_bottom <= dst_top)
        dst_bottom = dst_top + 1;

    dst_rect->left = dst_left;
    dst_rect->top = dst_top;
    dst_rect->right = dst_right;
    dst_rect->bottom = dst_bottom;
}

#define GUAC_XORG_DAMAGE_COALESCE_US 12000

void* guac_xorg_display_thread(void* arg) {

    guac_client* client = (guac_client*) arg;
    guac_xorg_client* xorg_client = (guac_xorg_client*) client->data;

    int fps = xorg_client->settings->fps > 0
        ? xorg_client->settings->fps
        : 15;
    int frame_delay_us = 1000000 / fps;
    struct timespec delay;
    delay.tv_sec = frame_delay_us / 1000000;
    delay.tv_nsec = (frame_delay_us % 1000000) * 1000;
    struct timespec last_frame = { 0, 0 };

    xorg_client->damage_pending = 1;
    guac_rect_init(&xorg_client->damage_rect, 0, 0,
            xorg_client->capture_width, xorg_client->capture_height);

    while (client->state == GUAC_CLIENT_RUNNING && !xorg_client->stop) {

        XLockDisplay(xorg_client->x_display);

        XEvent event;
        while (XPending(xorg_client->x_display)) {
            XNextEvent(xorg_client->x_display, &event);
            if (xorg_client->capture.damage_available
                    && event.type == xorg_client->capture.damage_event_base
                    + XDamageNotify) {
                XDamageNotifyEvent* damage =
                    (XDamageNotifyEvent*) &event;
                guac_rect rect;
                guac_rect_init(&rect, damage->area.x, damage->area.y,
                        damage->area.width, damage->area.height);
                guac_xorg_union_damage(xorg_client, &rect);
            }
            guac_xorg_cursor_handle_event(xorg_client, &event);
        }

        int prev_capture_width = xorg_client->capture_width;
        int prev_capture_height = xorg_client->capture_height;
        XWindowAttributes attrs;
        if (XGetWindowAttributes(xorg_client->x_display,
                    xorg_client->root_window, &attrs)) {
            guac_xorg_update_dimensions(xorg_client, &attrs);
        }
        XUnlockDisplay(xorg_client->x_display);

        if (xorg_client->capture_width != prev_capture_width
                || xorg_client->capture_height != prev_capture_height) {
            guac_rect full;
            guac_rect_init(&full, 0, 0,
                    xorg_client->capture_width,
                    xorg_client->capture_height);
            xorg_client->damage_rect = full;
            xorg_client->damage_pending = 1;
        }

        if (!xorg_client->damage_pending
                && xorg_client->capture.damage_available) {
            nanosleep(&delay, NULL);
            continue;
        }

        if (xorg_client->damage_pending
                && xorg_client->capture.damage_available) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            long long since_damage_us =
                (now.tv_sec - xorg_client->last_damage_time.tv_sec) * 1000000LL
                + (now.tv_nsec - xorg_client->last_damage_time.tv_nsec) / 1000;
            if (since_damage_us < GUAC_XORG_DAMAGE_COALESCE_US) {
                struct timespec remaining = {
                    .tv_sec = 0,
                    .tv_nsec = (GUAC_XORG_DAMAGE_COALESCE_US - since_damage_us)
                        * 1000
                };
                nanosleep(&remaining, NULL);
                continue;
            }
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long elapsed_us = (now.tv_sec - last_frame.tv_sec) * 1000000LL
            + (now.tv_nsec - last_frame.tv_nsec) / 1000;
        if (elapsed_us < frame_delay_us) {
            struct timespec remaining = {
                .tv_sec = 0,
                .tv_nsec = (frame_delay_us - elapsed_us) * 1000
            };
            nanosleep(&remaining, NULL);
            continue;
        }

        if (xorg_client->width <= 0 || xorg_client->height <= 0
                || xorg_client->capture_width <= 0
                || xorg_client->capture_height <= 0) {
            nanosleep(&delay, NULL);
            continue;
        }

        guac_display_layer* default_layer =
            guac_display_default_layer(xorg_client->display);
        guac_rect bounds;
        guac_display_layer_get_bounds(default_layer, &bounds);
        int current_width = bounds.right - bounds.left;
        int current_height = bounds.bottom - bounds.top;
        if (current_width != xorg_client->width
                || current_height != xorg_client->height) {
            guac_display_layer_resize(default_layer,
                    xorg_client->width, xorg_client->height);
        }
        guac_xorg_update_maps(xorg_client);

        guac_rect src_rect = xorg_client->damage_pending
            ? xorg_client->damage_rect
            : (guac_rect) {
                .left = 0,
                .top = 0,
                .right = xorg_client->capture_width,
                .bottom = xorg_client->capture_height
            };

        if (src_rect.left < 0)
            src_rect.left = 0;
        if (src_rect.top < 0)
            src_rect.top = 0;
        if (src_rect.right > xorg_client->capture_width)
            src_rect.right = xorg_client->capture_width;
        if (src_rect.bottom > xorg_client->capture_height)
            src_rect.bottom = xorg_client->capture_height;

        if (src_rect.right <= src_rect.left
                || src_rect.bottom <= src_rect.top) {
            xorg_client->damage_pending = 0;
            nanosleep(&delay, NULL);
            continue;
        }

        guac_rect dst_rect;
        guac_xorg_map_damage(xorg_client, &src_rect, &dst_rect);

        if (dst_rect.left < 0)
            dst_rect.left = 0;
        if (dst_rect.top < 0)
            dst_rect.top = 0;
        if (dst_rect.right > xorg_client->width)
            dst_rect.right = xorg_client->width;
        if (dst_rect.bottom > xorg_client->height)
            dst_rect.bottom = xorg_client->height;

        if (dst_rect.right <= dst_rect.left
                || dst_rect.bottom <= dst_rect.top) {
            xorg_client->damage_pending = 0;
            nanosleep(&delay, NULL);
            continue;
        }

        XLockDisplay(xorg_client->x_display);
        if (xorg_client->capture.damage_available) {
            XDamageSubtract(xorg_client->x_display,
                    xorg_client->capture.damage, None, None);
        }
        XUnlockDisplay(xorg_client->x_display);

        XImage* image = NULL;
        int image_owned = 0;

        XLockDisplay(xorg_client->x_display);
        if (guac_xorg_capture_image(xorg_client, &src_rect,
                    &image, &image_owned) != 0) {
            XUnlockDisplay(xorg_client->x_display);
            nanosleep(&delay, NULL);
            continue;
        }
        XUnlockDisplay(xorg_client->x_display);

        if (xorg_client->red_max == 0 && !guac_xorg_prepare_format(
                    xorg_client, image)) {
            if (image_owned)
                XDestroyImage(image);
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Unsupported XImage format.");
            guac_client_stop(client);
            break;
        }

        guac_display_layer_raw_context* context =
            guac_display_layer_open_raw(default_layer);
        guac_xorg_scale_image(xorg_client, image, &src_rect, &dst_rect, context);
        guac_display_layer_close_raw(default_layer, context);

        if (image_owned)
            XDestroyImage(image);

        last_frame = now;
        guac_xorg_cursor_update(xorg_client);

        guac_display_end_frame(xorg_client->display);
        guac_socket_flush(client->socket);

        xorg_client->damage_pending = 0;
    }

    return NULL;
}

int guac_xorg_display_init(guac_client* client, guac_xorg_client* xorg_client) {

    if (!XInitThreads())
        guac_client_log(client, GUAC_LOG_WARNING,
                "XInitThreads failed; Xlib may not be thread-safe.");

    const char* display_name = xorg_client->settings->display;
    if (display_name == NULL || *display_name == '\0')
        display_name = getenv("DISPLAY");

    xorg_client->x_display = XOpenDisplay(display_name);
    if (xorg_client->x_display == NULL) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to open X display (DISPLAY=%s).",
                display_name != NULL ? display_name : "(null)");
        return 1;
    }

    xorg_client->root_window = DefaultRootWindow(xorg_client->x_display);

    XWindowAttributes attrs;
    if (!XGetWindowAttributes(xorg_client->x_display,
                xorg_client->root_window, &attrs)) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to query root window attributes.");
        XCloseDisplay(xorg_client->x_display);
        xorg_client->x_display = NULL;
        return 1;
    }

    xorg_client->width = xorg_client->settings->width > 0
        ? xorg_client->settings->width
        : attrs.width;

    xorg_client->height = xorg_client->settings->height > 0
        ? xorg_client->settings->height
        : attrs.height;

    xorg_client->capture_width = attrs.width;
    xorg_client->capture_height = attrs.height;

    int xtest_event_base = 0;
    int xtest_error_base = 0;
    int xtest_major = 0;
    int xtest_minor = 0;
    xorg_client->xtest_available = XTestQueryExtension(xorg_client->x_display,
            &xtest_event_base, &xtest_error_base, &xtest_major, &xtest_minor);
    if (!xorg_client->xtest_available)
        guac_client_log(client, GUAC_LOG_WARNING,
                "XTest extension unavailable; input will be disabled.");

    guac_xorg_capture_init(client, xorg_client);
    guac_xorg_cursor_init(client, xorg_client);

    xorg_client->display = guac_display_alloc(client);
    if (xorg_client->display == NULL) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to allocate Guacamole display.");
        XCloseDisplay(xorg_client->x_display);
        xorg_client->x_display = NULL;
        return 1;
    }

    guac_display_layer_resize(guac_display_default_layer(xorg_client->display),
            xorg_client->width, xorg_client->height);

    xorg_client->display_thread_running = 1;
    if (pthread_create(&xorg_client->display_thread, NULL,
                guac_xorg_display_thread, client)) {
        guac_client_log(client, GUAC_LOG_ERROR,
                "Unable to start display thread.");
        guac_display_free(xorg_client->display);
        xorg_client->display = NULL;
        XCloseDisplay(xorg_client->x_display);
        xorg_client->x_display = NULL;
        return 1;
    }

    return 0;
}

void guac_xorg_display_free(guac_xorg_client* xorg_client) {

    if (xorg_client == NULL)
        return;

    xorg_client->stop = 1;

    if (xorg_client->display_thread_running) {
        pthread_join(xorg_client->display_thread, NULL);
        xorg_client->display_thread_running = 0;
    }

    guac_xorg_capture_free(xorg_client);

    if (xorg_client->display != NULL) {
        guac_display_free(xorg_client->display);
        xorg_client->display = NULL;
    }

    if (xorg_client->x_display != NULL) {
        XCloseDisplay(xorg_client->x_display);
        xorg_client->x_display = NULL;
    }

    guac_xorg_free_maps(xorg_client);

    guac_xorg_settings_free(xorg_client->settings);
    xorg_client->settings = NULL;
}

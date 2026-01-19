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

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/rect.h>
#include <guacamole/socket.h>

#include <X11/Xutil.h>

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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

static void guac_xorg_draw_image(guac_xorg_client* xorg_client,
        XImage* image) {

    guac_display_layer* default_layer =
        guac_display_default_layer(xorg_client->display);
    guac_display_layer_raw_context* context =
        guac_display_layer_open_raw(default_layer);

    int x;
    int y;

    for (y = 0; y < xorg_client->height; y++) {
        uint32_t* buffer_current =
            (uint32_t*) (context->buffer + (context->stride * y));

        for (x = 0; x < xorg_client->width; x++) {
            unsigned long pixel = XGetPixel(image, x, y);

            unsigned long red = (pixel & xorg_client->red_mask)
                    >> xorg_client->red_shift;
            unsigned long green = (pixel & xorg_client->green_mask)
                    >> xorg_client->green_shift;
            unsigned long blue = (pixel & xorg_client->blue_mask)
                    >> xorg_client->blue_shift;

            red = (red * 255) / xorg_client->red_max;
            green = (green * 255) / xorg_client->green_max;
            blue = (blue * 255) / xorg_client->blue_max;

            *(buffer_current++) = (red << 16) | (green << 8) | blue;
        }
    }

    guac_rect full_rect;
    guac_rect_init(&full_rect, 0, 0, xorg_client->width, xorg_client->height);
    guac_rect_extend(&context->dirty, &full_rect);

    guac_display_layer_close_raw(default_layer, context);
}

void* guac_xorg_display_thread(void* arg) {

    guac_client* client = (guac_client*) arg;
    guac_xorg_client* xorg_client = (guac_xorg_client*) client->data;

    int frame_delay_us = 1000000 / xorg_client->settings->fps;
    struct timespec delay;
    delay.tv_sec = frame_delay_us / 1000000;
    delay.tv_nsec = (frame_delay_us % 1000000) * 1000;

    while (client->state == GUAC_CLIENT_RUNNING && !xorg_client->stop) {

        XImage* image = XGetImage(xorg_client->x_display,
                xorg_client->root_window, 0, 0, xorg_client->width,
                xorg_client->height, AllPlanes, ZPixmap);

        if (image == NULL) {
            nanosleep(&delay, NULL);
            continue;
        }

        if (xorg_client->red_max == 0 && !guac_xorg_prepare_format(
                    xorg_client, image)) {
            XDestroyImage(image);
            guac_client_log(client, GUAC_LOG_ERROR,
                    "Unsupported XImage format.");
            guac_client_stop(client);
            break;
        }

        guac_xorg_draw_image(xorg_client, image);
        XDestroyImage(image);

        guac_display_end_frame(xorg_client->display);
        guac_socket_flush(client->socket);

        nanosleep(&delay, NULL);
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

    if (xorg_client->display != NULL) {
        guac_display_free(xorg_client->display);
        xorg_client->display = NULL;
    }

    if (xorg_client->x_display != NULL) {
        XCloseDisplay(xorg_client->x_display);
        xorg_client->x_display = NULL;
    }

    guac_xorg_settings_free(xorg_client->settings);
    xorg_client->settings = NULL;
}

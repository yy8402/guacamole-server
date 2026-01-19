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

#ifndef GUAC_XORG_CLIENT_H
#define GUAC_XORG_CLIENT_H

#include "settings.h"

#include <guacamole/client.h>
#include <guacamole/display.h>

#include <X11/Xlib.h>

#include <pthread.h>

typedef struct guac_xorg_client {

    guac_xorg_settings* settings;

    guac_display* display;

    Display* x_display;
    Window root_window;

    int width;
    int height;

    unsigned long red_mask;
    unsigned long green_mask;
    unsigned long blue_mask;
    int red_shift;
    int green_shift;
    int blue_shift;
    unsigned long red_max;
    unsigned long green_max;
    unsigned long blue_max;

    pthread_t display_thread;
    int display_thread_running;
    int stop;

} guac_xorg_client;

int guac_client_init(guac_client* client);
int guac_xorg_client_free_handler(guac_client* client);

#endif

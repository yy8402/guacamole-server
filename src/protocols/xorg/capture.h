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

#ifndef GUAC_XORG_CAPTURE_H
#define GUAC_XORG_CAPTURE_H

#include <guacamole/client.h>
#include <guacamole/rect.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xdamage.h>

struct guac_xorg_client;

typedef struct guac_xorg_capture {
    int damage_available;
    int damage_event_base;
    Damage damage;

    int xshm_available;
    XImage* shm_image;
    XShmSegmentInfo shm_info;
    int shm_attached;
    int shm_width;
    int shm_height;
} guac_xorg_capture;

int guac_xorg_capture_init(guac_client* client,
        struct guac_xorg_client* xorg_client);
void guac_xorg_capture_free(struct guac_xorg_client* xorg_client);

int guac_xorg_capture_image(struct guac_xorg_client* xorg_client,
        const guac_rect* src_rect, XImage** out_image, int* out_owned);

#endif

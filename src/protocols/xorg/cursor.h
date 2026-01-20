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

#ifndef GUAC_XORG_CURSOR_H
#define GUAC_XORG_CURSOR_H

#include <guacamole/client.h>
#include <X11/extensions/Xfixes.h>

struct guac_xorg_client;

typedef struct guac_xorg_cursor {
    int xfixes_available;
    int xfixes_event_base;
    int cursor_dirty;
} guac_xorg_cursor;

int guac_xorg_cursor_init(guac_client* client,
        struct guac_xorg_client* xorg_client);
void guac_xorg_cursor_handle_event(struct guac_xorg_client* xorg_client,
        XEvent* event);
void guac_xorg_cursor_update(struct guac_xorg_client* xorg_client);

#endif

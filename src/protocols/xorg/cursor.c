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

#include "cursor.h"
#include "client.h"

#include <guacamole/display.h>
#include <guacamole/rect.h>
#include <guacamole/socket.h>
#include <X11/Xlib.h>
#include <stdint.h>
#include <string.h>

int guac_xorg_cursor_init(guac_client* client, guac_xorg_client* xorg_client) {

    guac_xorg_cursor* cursor = &xorg_client->cursor;
    cursor->xfixes_available = 0;
    cursor->cursor_dirty = 1;

    int xfixes_error_base = 0;
    if (XFixesQueryExtension(xorg_client->x_display,
                &cursor->xfixes_event_base, &xfixes_error_base)) {
        cursor->xfixes_available = 1;
        XFixesSelectCursorInput(xorg_client->x_display,
                xorg_client->root_window, XFixesDisplayCursorNotifyMask);
    }
    else
        guac_client_log(client, GUAC_LOG_DEBUG,
                "XFixes unavailable; cursor updates disabled.");

    return 0;
}

void guac_xorg_cursor_handle_event(guac_xorg_client* xorg_client,
        XEvent* event) {

    if (!xorg_client->cursor.xfixes_available || event == NULL)
        return;

    if (event->type == xorg_client->cursor.xfixes_event_base
            + XFixesCursorNotify)
        xorg_client->cursor.cursor_dirty = 1;
}

void guac_xorg_cursor_update(guac_xorg_client* xorg_client) {

    if (!xorg_client->cursor.xfixes_available
            || !xorg_client->cursor.cursor_dirty)
        return;

    XLockDisplay(xorg_client->x_display);
    XFixesCursorImage* cursor =
        XFixesGetCursorImage(xorg_client->x_display);
    XUnlockDisplay(xorg_client->x_display);
    if (cursor == NULL)
        return;

    guac_display_layer* cursor_layer =
        guac_display_cursor(xorg_client->display);
    guac_display_layer_resize(cursor_layer, cursor->width, cursor->height);

    guac_display_layer_raw_context* context =
        guac_display_layer_open_raw(cursor_layer);

    for (int y = 0; y < cursor->height; y++) {
        uint32_t* dst_row =
            (uint32_t*) (context->buffer + (context->stride * y));
        uint32_t* src_row = (uint32_t*) (cursor->pixels + (y * cursor->width));
        memcpy(dst_row, src_row, cursor->width * sizeof(uint32_t));
    }

    guac_rect dirty = {
        .left = 0,
        .top = 0,
        .right = cursor->width,
        .bottom = cursor->height
    };
    guac_rect_extend(&context->dirty, &dirty);

    guac_display_layer_close_raw(cursor_layer, context);
    guac_display_set_cursor_hotspot(xorg_client->display,
            cursor->xhot, cursor->yhot);
    guac_display_end_mouse_frame(xorg_client->display);

    XFree(cursor);
    xorg_client->cursor.cursor_dirty = 0;
}

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

#include "input.h"
#include "client.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdlib.h>

static guac_xorg_input_state* guac_xorg_get_input_state(guac_user* user) {
    if (user->data == NULL) {
        guac_xorg_input_state* state = calloc(1, sizeof(guac_xorg_input_state));
        user->data = state;
    }
    return (guac_xorg_input_state*) user->data;
}

static void guac_xorg_send_button(Display* display, int button, int pressed) {
    XTestFakeButtonEvent(display, button, pressed, CurrentTime);
}

int guac_xorg_user_mouse_handler(guac_user* user, int x, int y, int mask) {

    guac_xorg_client* xorg_client = (guac_xorg_client*) user->client->data;
    if (xorg_client == NULL || xorg_client->x_display == NULL)
        return 0;

    guac_xorg_input_state* state = guac_xorg_get_input_state(user);
    int last_mask = state->last_mask;

    XLockDisplay(xorg_client->x_display);

    XTestFakeMotionEvent(xorg_client->x_display, -1, x, y, CurrentTime);

    int button_map[5] = { 1, 2, 3, 4, 5 };
    int mask_bits[5] = { 1, 2, 4, 8, 16 };

    for (int i = 0; i < 5; i++) {
        int bit = mask_bits[i];
        if ((mask & bit) != (last_mask & bit)) {
            int pressed = (mask & bit) ? 1 : 0;
            guac_xorg_send_button(xorg_client->x_display, button_map[i],
                    pressed);
        }
    }

    XFlush(xorg_client->x_display);
    XUnlockDisplay(xorg_client->x_display);

    state->last_mask = mask;

    return 0;
}

int guac_xorg_user_key_handler(guac_user* user, int keysym, int pressed) {

    guac_xorg_client* xorg_client = (guac_xorg_client*) user->client->data;
    if (xorg_client == NULL || xorg_client->x_display == NULL)
        return 0;

    KeySym sym = (KeySym) keysym;
    KeyCode code = XKeysymToKeycode(xorg_client->x_display, sym);
    if (code == 0)
        return 0;

    XLockDisplay(xorg_client->x_display);
    XTestFakeKeyEvent(xorg_client->x_display, code, pressed, CurrentTime);
    XFlush(xorg_client->x_display);
    XUnlockDisplay(xorg_client->x_display);

    return 0;
}

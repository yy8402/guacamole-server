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

#include "client.h"
#include "display.h"
#include "input.h"
#include "settings.h"
#include "user.h"

#include <guacamole/client.h>
#include <guacamole/display.h>
#include <guacamole/socket.h>
#include <stdlib.h>

int guac_xorg_user_join_handler(guac_user* user, int argc, char** argv) {

    guac_client* client = user->client;
    guac_xorg_client* xorg_client = (guac_xorg_client*) client->data;

    if (xorg_client->settings == NULL) {
        xorg_client->settings = guac_xorg_parse_args(user, argc,
                (const char**) argv);
        if (xorg_client->settings == NULL)
            return 1;

        guac_user_log(user, GUAC_LOG_INFO,
                "Xorg settings: display=%s width=%d height=%d fps=%d",
                xorg_client->settings->display != NULL
                    ? xorg_client->settings->display
                    : "(null)",
                xorg_client->settings->width,
                xorg_client->settings->height,
                xorg_client->settings->fps);

        if (guac_xorg_display_init(client, xorg_client)) {
            guac_xorg_settings_free(xorg_client->settings);
            xorg_client->settings = NULL;
            return 1;
        }
    }

    guac_display_dup(xorg_client->display, user->socket);
    guac_socket_flush(user->socket);

    user->mouse_handler = guac_xorg_user_mouse_handler;
    user->key_handler = guac_xorg_user_key_handler;

    return 0;
}

int guac_xorg_user_leave_handler(guac_user* user) {
    free(user->data);
    user->data = NULL;
    return 0;
}

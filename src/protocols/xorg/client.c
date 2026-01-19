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
#include "user.h"
#include "xorg.h"

#include <guacamole/client.h>

#include <stdlib.h>

int guac_client_init(guac_client* client) {

    client->args = GUAC_XORG_CLIENT_ARGS;

    guac_xorg_client* xorg_client = calloc(1, sizeof(guac_xorg_client));
    client->data = xorg_client;

    client->join_handler = guac_xorg_user_join_handler;
    client->leave_handler = guac_xorg_user_leave_handler;
    client->free_handler = guac_xorg_client_free_handler;

    return 0;
}

int guac_xorg_client_free_handler(guac_client* client) {

    guac_xorg_client* xorg_client = (guac_xorg_client*) client->data;
    if (xorg_client == NULL)
        return 0;

    guac_xorg_display_free(xorg_client);

    free(xorg_client);
    client->data = NULL;

    return 0;
}


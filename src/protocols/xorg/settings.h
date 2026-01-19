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

#ifndef GUAC_XORG_SETTINGS_H
#define GUAC_XORG_SETTINGS_H

#include <guacamole/user.h>

typedef struct guac_xorg_settings {

    /**
     * X display string (NULL uses DISPLAY env).
     */
    char* display;

    /**
     * Width to capture (0 uses full screen width).
     */
    int width;

    /**
     * Height to capture (0 uses full screen height).
     */
    int height;

    /**
     * Target frames per second.
     */
    int fps;

} guac_xorg_settings;

/**
 * Env var name containing optional config file path.
 */
#define GUAC_XORG_CONFIG_ENV "GUAC_XORG_CONFIG"
#define GUAC_XORG_CONFIG_PATH "/etc/guacamole/xorg.conf"

guac_xorg_settings* guac_xorg_parse_args(guac_user* user,
        int argc, const char** argv);

void guac_xorg_settings_free(guac_xorg_settings* settings);

#endif

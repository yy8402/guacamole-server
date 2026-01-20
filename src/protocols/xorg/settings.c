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

#include "settings.h"
#include "xorg.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* GUAC_XORG_CLIENT_ARGS[] = {
    "display",
    "width",
    "height",
    "fps",
    NULL
};

enum GUAC_XORG_ARGS_IDX {
    IDX_DISPLAY,
    IDX_WIDTH,
    IDX_HEIGHT,
    IDX_FPS,
    GUAC_XORG_ARGS_COUNT
};

static void guac_xorg_trim(char* value) {
    char* end = value + strlen(value);
    while (end > value && isspace((unsigned char) *(end - 1)))
        *(--end) = '\0';
    while (*value && isspace((unsigned char) *value))
        memmove(value, value + 1, strlen(value));
}

static int guac_xorg_parse_int(const char* value, int fallback) {
    char* end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value || parsed < 0 || parsed > 100000)
        return fallback;
    return (int) parsed;
}

static void guac_xorg_apply_kv(guac_xorg_settings* settings,
        const char* key, const char* value) {

    if (settings == NULL || key == NULL || value == NULL)
        return;

    if (strcmp(key, "display") == 0 && settings->display == NULL)
        settings->display = strdup(value);
    else if (strcmp(key, "width") == 0 && settings->width == 0)
        settings->width = guac_xorg_parse_int(value, 0);
    else if (strcmp(key, "height") == 0 && settings->height == 0)
        settings->height = guac_xorg_parse_int(value, 0);
    else if (strcmp(key, "fps") == 0 && settings->fps == 0)
        settings->fps = guac_xorg_parse_int(value, 0);
}

static void guac_xorg_load_config(guac_xorg_settings* settings,
        const char* path) {

    if (path == NULL || *path == '\0')
        return;

    FILE* file = fopen(path, "r");
    if (file == NULL)
        return;

    char line[512];
    while (fgets(line, sizeof(line), file) != NULL) {
        char* trimmed = line;
        while (*trimmed && isspace((unsigned char) *trimmed))
            trimmed++;

        if (*trimmed == '\0' || *trimmed == '#' || *trimmed == ';')
            continue;

        char* equals = strchr(trimmed, '=');
        if (equals == NULL)
            continue;

        *equals = '\0';
        char* key = trimmed;
        char* value = equals + 1;

        guac_xorg_trim(key);
        guac_xorg_trim(value);

        if (*key == '\0' || *value == '\0')
            continue;

        guac_xorg_apply_kv(settings, key, value);
    }

    fclose(file);
}

guac_xorg_settings* guac_xorg_parse_args(guac_user* user,
        int argc, const char** argv) {

    if (argc != GUAC_XORG_ARGS_COUNT) {
        guac_user_log(user, GUAC_LOG_WARNING,
                "Incorrect number of connection parameters provided: "
                "expected %i, got %i.", GUAC_XORG_ARGS_COUNT, argc);
        return NULL;
    }

    guac_xorg_settings* settings = calloc(1, sizeof(guac_xorg_settings));

    settings->display =
        guac_user_parse_args_string(user, GUAC_XORG_CLIENT_ARGS, argv,
                IDX_DISPLAY, NULL);

    settings->width =
        guac_user_parse_args_int(user, GUAC_XORG_CLIENT_ARGS, argv,
                IDX_WIDTH, 0);

    settings->height =
        guac_user_parse_args_int(user, GUAC_XORG_CLIENT_ARGS, argv,
                IDX_HEIGHT, 0);

    settings->fps =
        guac_user_parse_args_int(user, GUAC_XORG_CLIENT_ARGS, argv,
                IDX_FPS, 30);

    const char* env_display = getenv("GUAC_XORG_DISPLAY");
    const char* env_width = getenv("GUAC_XORG_WIDTH");
    const char* env_height = getenv("GUAC_XORG_HEIGHT");
    const char* env_fps = getenv("GUAC_XORG_FPS");
    const char* env_config = getenv(GUAC_XORG_CONFIG_ENV);
    const char* config_path = env_config != NULL ? env_config
            : GUAC_XORG_CONFIG_PATH;

    if (settings->display == NULL && env_display != NULL)
        settings->display = strdup(env_display);
    if (settings->width == 0 && env_width != NULL)
        settings->width = guac_xorg_parse_int(env_width, 0);
    if (settings->height == 0 && env_height != NULL)
        settings->height = guac_xorg_parse_int(env_height, 0);
    if (settings->fps == 0 && env_fps != NULL)
        settings->fps = guac_xorg_parse_int(env_fps, 0);

    if (settings->display == NULL || settings->width == 0
            || settings->height == 0 || settings->fps == 0)
        guac_xorg_load_config(settings, config_path);

    if (settings->fps <= 0)
        settings->fps = 30;

    return settings;

}

void guac_xorg_settings_free(guac_xorg_settings* settings) {
    if (settings == NULL)
        return;

    free(settings->display);
    free(settings);
}

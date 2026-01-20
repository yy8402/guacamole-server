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

#ifndef GUAC_XORG_SCALE_H
#define GUAC_XORG_SCALE_H

#include <guacamole/display.h>
#include <guacamole/rect.h>
#include <X11/Xutil.h>

struct guac_xorg_client;

void guac_xorg_scale_image(struct guac_xorg_client* xorg_client, XImage* image,
        const guac_rect* src_rect, const guac_rect* dst_rect,
        guac_display_layer_raw_context* context);

#endif

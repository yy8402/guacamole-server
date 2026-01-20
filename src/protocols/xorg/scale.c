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

#include "scale.h"
#include "client.h"

#include <guacamole/rect.h>
#include <string.h>
#include <stdint.h>

static unsigned long guac_xorg_get_pixel_fast(const XImage* image, int x, int y) {
    int bpp = image->bits_per_pixel;
    const unsigned char* row = (const unsigned char*) image->data
        + (y * image->bytes_per_line);
    const unsigned char* pixel = row + (x * (bpp / 8));

    if (bpp == 32) {
        if (image->byte_order == LSBFirst)
            return (unsigned long) ((uint32_t) pixel[0]
                    | ((uint32_t) pixel[1] << 8)
                    | ((uint32_t) pixel[2] << 16)
                    | ((uint32_t) pixel[3] << 24));
        return (unsigned long) ((uint32_t) pixel[3]
                | ((uint32_t) pixel[2] << 8)
                | ((uint32_t) pixel[1] << 16)
                | ((uint32_t) pixel[0] << 24));
    }

    if (bpp == 24) {
        if (image->byte_order == LSBFirst)
            return (unsigned long) ((uint32_t) pixel[0]
                    | ((uint32_t) pixel[1] << 8)
                    | ((uint32_t) pixel[2] << 16));
        return (unsigned long) ((uint32_t) pixel[2]
                | ((uint32_t) pixel[1] << 8)
                | ((uint32_t) pixel[0] << 16));
    }

    return XGetPixel((XImage*) image, x, y);
}

static int guac_xorg_can_blit_direct(const guac_xorg_client* xorg_client,
        const XImage* image, const guac_rect* src_rect,
        const guac_rect* dst_rect) {

    if (xorg_client->width != xorg_client->capture_width
            || xorg_client->height != xorg_client->capture_height)
        return 0;

    if (src_rect->left != dst_rect->left
            || src_rect->top != dst_rect->top
            || src_rect->right != dst_rect->right
            || src_rect->bottom != dst_rect->bottom)
        return 0;

    if (image->bits_per_pixel != 32 || image->byte_order != LSBFirst)
        return 0;

    if (xorg_client->red_mask != 0x00ff0000
            || xorg_client->green_mask != 0x0000ff00
            || xorg_client->blue_mask != 0x000000ff)
        return 0;

    return 1;
}

void guac_xorg_scale_image(guac_xorg_client* xorg_client, XImage* image,
        const guac_rect* src_rect, const guac_rect* dst_rect,
        guac_display_layer_raw_context* context) {

    int output_width = xorg_client->width;
    int output_height = xorg_client->height;
    int capture_width = xorg_client->capture_width;
    int capture_height = xorg_client->capture_height;

    if (output_width <= 0 || output_height <= 0
            || capture_width <= 0 || capture_height <= 0)
        return;

    if (guac_xorg_can_blit_direct(xorg_client, image, src_rect, dst_rect)) {
        int row_bytes = (src_rect->right - src_rect->left) * sizeof(uint32_t);
        const unsigned char* src_row = (const unsigned char*) image->data
            + (src_rect->top * image->bytes_per_line)
            + (src_rect->left * sizeof(uint32_t));
        unsigned char* dst_row = context->buffer
            + (dst_rect->top * context->stride)
            + (dst_rect->left * sizeof(uint32_t));

        for (int y = src_rect->top; y < src_rect->bottom; y++) {
            memcpy(dst_row, src_row, row_bytes);
            src_row += image->bytes_per_line;
            dst_row += context->stride;
        }

        guac_rect_extend(&context->dirty, dst_rect);
        return;
    }

    int use_x_map = (xorg_client->x_map != NULL
            && xorg_client->y_map != NULL
            && xorg_client->map_output_width == output_width
            && xorg_client->map_output_height == output_height
            && xorg_client->map_capture_width == capture_width
            && xorg_client->map_capture_height == capture_height);

    for (int dy = dst_rect->top; dy < dst_rect->bottom; dy++) {
        int src_abs_y = use_x_map
            ? xorg_client->y_map[dy]
            : (int) (((long long) dy * capture_height) / output_height);
        if (src_abs_y < src_rect->top || src_abs_y >= src_rect->bottom)
            continue;

        int src_y = src_abs_y - src_rect->top;
        uint32_t* dst_row =
            (uint32_t*) (context->buffer + (context->stride * dy)
            + (dst_rect->left * sizeof(uint32_t)));

        for (int dx = dst_rect->left; dx < dst_rect->right; dx++) {
            int src_abs_x = use_x_map
                ? xorg_client->x_map[dx]
                : (int) (((long long) dx * capture_width) / output_width);
            if (src_abs_x < src_rect->left || src_abs_x >= src_rect->right) {
                dst_row++;
                continue;
            }

            int src_x = src_abs_x - src_rect->left;
            unsigned long pixel = guac_xorg_get_pixel_fast(image, src_x, src_y);

            unsigned long red = (pixel & xorg_client->red_mask)
                    >> xorg_client->red_shift;
            unsigned long green = (pixel & xorg_client->green_mask)
                    >> xorg_client->green_shift;
            unsigned long blue = (pixel & xorg_client->blue_mask)
                    >> xorg_client->blue_shift;

            red = (red * 255) / xorg_client->red_max;
            green = (green * 255) / xorg_client->green_max;
            blue = (blue * 255) / xorg_client->blue_max;

            *(dst_row++) = (red << 16) | (green << 8) | blue;
        }
    }

    guac_rect_extend(&context->dirty, dst_rect);
}

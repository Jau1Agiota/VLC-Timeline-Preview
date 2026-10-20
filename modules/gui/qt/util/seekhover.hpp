/*****************************************************************************
 * seekhover.hpp : read-only timeline hover geometry
 *****************************************************************************
 * Copyright (C) 2026 VLC Timeline Preview contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. See COPYING.
 *****************************************************************************/
#ifndef VLC_QT_SEEKHOVER_HPP_
#define VLC_QT_SEEKHOVER_HPP_

#include <stdint.h>

namespace SeekHover
{
struct Position
{
    int start;
    int end;
    int span;
    bool inside;
    int64_t timestampMs; // -1 means geometry or duration unavailable
};

/* Endpoints come from the style's min/max handle centers, in widget-local
 * Qt logical pixels. No slider-value quantization or playback side effects.
 * Geometry only: caller separately checks input state and seekability. */
inline Position map( int x, int y, int width, int height, int start, int end,
                     int64_t durationMs, bool horizontal )
{
    Position result = { start, end, 0, false, -1 };
    if( !horizontal || width <= 0 || height <= 0 || start < 0 || end < 0 ||
        start >= width || end >= width || start == end )
        return result;

    const int left = start < end ? start : end;
    const int right = start < end ? end : start;
    result.span = right - left;
    result.inside = x >= left && x <= right && y >= 0 && y < height;
    if( durationMs <= 0 )
        return result;

    // Clamp before subtracting to handle even extreme out-of-widget positions.
    const int clamped = x <= left ? left : x >= right ? right : x;
    const int offset = start < end ? clamped - start : start - clamped;
    // Avoid durationMs * offset overflow. Remainder and offset are <= INT_MAX.
    result.timestampMs = (durationMs / result.span) * offset
                       + ((durationMs % result.span) * offset) / result.span;
    return result;
}
}
#endif

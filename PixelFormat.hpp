// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __PIXEL_FORMAT_HPP__
#define __PIXEL_FORMAT_HPP__

namespace JxrToAvif
{
    enum class PixelFormat
    {
        Rgb = 0,
        Yuv444,
        Yuv422,
        Yuv420,
        Yuv400
    };
}

#endif // __PIXEL_FORMAT_HPP__

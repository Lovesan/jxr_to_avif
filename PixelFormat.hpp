// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __PIXEL_FORMAT_HPP__
#define __PIXEL_FORMAT_HPP__

namespace JxrToAvif
{
    enum PixelFormat
    {
        PixelFormatRgb = 0,
        PixelFormatYuv444,
        PixelFormatYuv422,
        PixelFormatYuv420,
        PixelFormatYuv400
    };
}

#endif // __PIXEL_FORMAT_HPP__

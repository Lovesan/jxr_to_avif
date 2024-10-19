// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __JXR_IMAGE_HPP__
#define __JXR_IMAGE_HPP__

#include <string>
#include <memory>
#include <simd_math.h>

namespace JxrToAvif
{
    class JxrImage
    {
    public:
        static constexpr double DefaultMaxCllPercentile = 0.9999;

        explicit JxrImage(const std::wstring& filename, bool realMaxCLL = false, double maxCllPercentile = DefaultMaxCllPercentile);

        JxrImage(const JxrImage&) = delete;

        JxrImage(JxrImage&&) = default;

        JxrImage& operator=(const JxrImage&) = delete;

        JxrImage& operator=(JxrImage&&) = default;

        ~JxrImage() = default;

        [[nodiscard]] uint32_t GetWidth() const
        {
            return _width;
        }

        [[nodiscard]] uint32_t GetHeight() const
        {
            return _height;
        }

        [[nodiscard]] uint16_t GetMaxCLL() const
        {
            return _maxCLL;
        }

        [[nodiscard]] uint16_t GetMaxPALL() const
        {
            return _maxPALL;
        }

        [[nodiscard]] ushort3* GetDataPointer() const
        {
            return _pixels.get();
        }

    private:
        uint32_t _width;
        uint32_t _height;
        uint16_t _maxCLL;
        uint16_t _maxPALL;
        std::unique_ptr<ushort3[]> _pixels;
    };
}

#endif // __JXR_IMAGE_HPP__

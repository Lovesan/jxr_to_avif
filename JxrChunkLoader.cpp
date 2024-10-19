// Copyright 2020 Joe Drago, 2024 Dmitry Ignatiev. All rights reserved

#include <cmath>
#include "JxrChunkLoader.hpp"

namespace JxrToAvif
{
    const float3x3 JxrChunkLoader::ScRgbToBt2100 = {
       {
           {
               static_cast<float>(2939026994.L / 585553224375.L),
               static_cast<float>(9255011753.L / 3513319346250.L),
               static_cast<float>(173911579.L / 501902763750.L),
               static_cast<float>(76515593.L / 138420033750.L),
               static_cast<float>(6109575001.L / 830520202500.L),
               static_cast<float>(75493061.L / 830520202500.L),
               static_cast<float>(12225392.L / 93230009375.L),
               static_cast<float>(1772384008.L / 2517210253125.L),
               static_cast<float>(18035212433.L / 2517210253125.L)
           }
       },
    };

    JxrChunkLoader::JxrChunkLoader(ushort3* output, const jxr_data& data, const uint32_t startLine, const uint32_t endLine)
        : _output(output), _data(data), _nitCounts(std::make_unique<uint32_t[]>(MaxNits)),
        _startLine(startLine), _endLine(endLine), _maxComponentSum(0), _maxNits(0)
    {
        _thread = std::thread(&JxrChunkLoader::ProcessChunk, this);
    }

    JxrChunkLoader::~JxrChunkLoader()
    {
        Wait();
    }

    void JxrChunkLoader::Wait()
    {
        if (_thread.joinable())
            _thread.join();
    }

    void JxrChunkLoader::ProcessChunk()
    {
        const float4x4 colorSpaceTransform = float4x4_transpose(float3x3_load(&ScRgbToBt2100));
        float finalMaxComponent = 0;
        double maxComponentSum = 0;

        for (uint32_t i = _startLine; i < _endLine; i++) {
            for (uint32_t j = 0; j < _data.width; j++) {
                const auto pixelIndex = i * _data.width + j;
                float4 v;

                if (_data.bytes_per_pixel == 16) {
                    v = float3_load(reinterpret_cast<float3*>(&reinterpret_cast<float4*>(_data.pixels)[pixelIndex]));
                }
                else {
                    v = half3_load(reinterpret_cast<half3*>(&reinterpret_cast<half4*>(_data.pixels)[pixelIndex]));
                }

                const auto bt2020 = float4_saturate(float3_transform(v, colorSpaceTransform));

                const auto maxComponent = float4_hmax(float4_min(bt2020, float4_set(2.f, 2.f, 2.f, 0.f)));

                const auto nits = static_cast<uint32_t>(roundf(maxComponent * 10000));
                _nitCounts[nits]++;

                if (maxComponent > finalMaxComponent) {
                    finalMaxComponent = maxComponent;
                }

                maxComponentSum += maxComponent;

                const auto pixel2020 = float4_to_int4(float4_scale(float4_pq_inv_eotf(bt2020), 65535));
                ushort3_store(&_output[pixelIndex], pixel2020);
            }
        }

        _maxNits = static_cast<uint16_t>(roundf(finalMaxComponent * 10000));
        _maxComponentSum = maxComponentSum;
    }
}

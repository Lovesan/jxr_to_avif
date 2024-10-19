// Copyright 2020 Joe Drago, 2024 Dmitry Ignatiev. All rights reserved

#include <cstring>
#include <cmath>
#include <vector>
#include <iostream>
#include "jxr_sys_helpers.h"
#include "JxrData.hpp"
#include "JxrChunkLoader.hpp"
#include "JxrImage.hpp"

namespace JxrToAvif
{
    JxrImage::JxrImage(const std::wstring& filename, const bool realMaxCLL, const double maxCllPercentile)
        : _width(0), _height(0), _maxCLL(0), _maxPALL(0)
    {
        const JxrLoaderThreadState state;
        {
            const JxrData dataWrapper(filename);

            const jxr_data& data = dataWrapper.Get();

            _width = data.width;
            _height = data.height;

            const auto pixelCount = static_cast<size_t>(_width) * _height;

            _pixels = std::unique_ptr<ushort3[]>(new ushort3[pixelCount]);

            const uint32_t numThreads = jxr_get_number_of_processors();

            std::cout << "Using " << numThreads << " threads\n";

            std::cout << "Converting pixels to BT.2100 PQ...\n" << std::flush;

            uint32_t convThreads = numThreads < 64 ? numThreads : 64;

            uint32_t chunkSize = _height / convThreads;

            if (chunkSize == 0) {
                convThreads = _height;
                chunkSize = 1;
            }

            std::vector<std::unique_ptr<JxrChunkLoader>> loaders;

            for (uint32_t i = 0; i < convThreads; i++)
            {
                uint32_t startLine = i * chunkSize,
                    endLine = (i == convThreads - 1) ? _height : (i + 1) * chunkSize;

                loaders.push_back(std::make_unique<JxrChunkLoader>(_pixels.get(), data, startLine, endLine));
            }

            double maxComponentSum = 0;

            for (uint32_t i = 0; i < convThreads; i++)
            {
                loaders[i]->Wait();

                const auto tMaxNits = loaders[i]->GetMaxNits();
                if (tMaxNits > _maxCLL)
                {
                    _maxCLL = tMaxNits;
                }

                maxComponentSum += loaders[i]->GetMaxComponentSum();
            }

            if (!realMaxCLL)
            {
                uint16_t currentIdx = _maxCLL;
                uint64_t count = 0;
                const auto countTarget = static_cast<uint64_t>(round((1 - maxCllPercentile) * static_cast<double>(pixelCount)));
                while (true)
                {
                    for (uint32_t i = 0; i < convThreads; i++)
                    {
                        count += loaders[i]->GetNitCount(currentIdx);
                    }
                    if (count >= countTarget)
                    {
                        _maxCLL = currentIdx;
                        break;
                    }
                    currentIdx--;
                }
            }

            _maxPALL = static_cast<uint16_t>(round(10000 * (maxComponentSum / static_cast<double>(pixelCount))));

            std::cout << "Computed HDR metadata: " << _maxCLL << " MaxCLL, " << _maxPALL << " MaxPALL.\n";
        }
    }
}

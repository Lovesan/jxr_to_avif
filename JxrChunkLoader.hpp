// Copyright 2024 Dmitry Ignatiev. All rights reserved

#ifndef __JXR_CHUNK_LOADER_HPP__
#define __JXR_CHUNK_LOADER_HPP__

#include <cstdint>
#include <memory>
#include <thread>
#include <simd_math.h>
#include "jxr_data.h"

namespace JxrToAvif
{
    class JxrChunkLoader
    {
    public:
        static constexpr int MaxNits = 10000;
        static constexpr uint8_t OutputDepth = 16;

        JxrChunkLoader(ushort3* output, const jxr_data& data, uint32_t startLine, uint32_t endLine);

        JxrChunkLoader(const JxrChunkLoader&) = delete;

        JxrChunkLoader(JxrChunkLoader&& rhs) = delete;

        JxrChunkLoader& operator=(const JxrChunkLoader&) = delete;

        JxrChunkLoader& operator=(JxrChunkLoader&&) = delete;

        ~JxrChunkLoader();

        void Wait();

        [[nodiscard]] uint16_t GetMaxNits() const
        {
            return _maxNits;
        }

        [[nodiscard]] double GetMaxComponentSum() const
        {
            return _maxComponentSum;
        }

        [[nodiscard]] uint32_t GetNitCount(const int nit) const
        {
            return _nitCounts[nit];
        }

    private:
        static const float3x3 ScRgbToBt2100;

        std::thread _thread;
        ushort3* _output;
        jxr_data _data;
        std::unique_ptr<uint32_t[]> _nitCounts;
        uint32_t _startLine;
        uint32_t _endLine;
        double _maxComponentSum;
        uint16_t _maxNits;

        void ProcessChunk();
    };
}
#endif // __JXR_CHUNK_LOADER_HPP__

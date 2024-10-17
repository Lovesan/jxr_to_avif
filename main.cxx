// adapted from avif-example-encode.c, see libavif license in LICENSE-THIRD-PARTY
// original copyright notice follows

// Copyright 2020 Joe Drago, 2024 Dmitry Ignatiev. All rights reserved
#include <cstring>
#include <windows.h>
#include <wincodec.h>
#include <cmath>
#include <cstdint>
#include <iostream>

#include <avif/avif.h>
#include <simd_math.h>
#include <string>

#include "PixelFormat.hpp"
#include "CommandLineParser.hpp"
#include "jxr_data.h"

constexpr auto INTERMEDIATE_BITS = 16;  // bit depth of the integer texture given to the encoder;

#define MAXCLL_PERCENTILE 0.9999

static const float3x3 scrgb_to_bt2100 = {
        2939026994.L / 585553224375.L, 9255011753.L / 3513319346250.L, 173911579.L / 501902763750.L,
        76515593.L / 138420033750.L,   6109575001.L / 830520202500.L,  75493061.L / 830520202500.L,
        12225392.L / 93230009375.L,    1772384008.L / 2517210253125.L, 18035212433.L / 2517210253125.L,
};

typedef struct ThreadData {
    uint8_t *pixels;
    uint16_t *converted;
    uint32_t width;
    uint32_t start;
    uint32_t stop;
    double sumOfMaxComp;
    uint32_t *nitCounts;
    uint16_t maxNits;
    uint8_t bytesPerColor;
} ThreadData;

DWORD WINAPI ThreadFunc(LPVOID lpParam) {
    ThreadData *d = static_cast<ThreadData*>(lpParam);
    uint8_t *pixels = d->pixels;
    uint8_t bytesPerColor = d->bytesPerColor;
    uint16_t *converted = d->converted;
    uint32_t width = d->width;
    uint32_t start = d->start;
    uint32_t stop = d->stop;
    float4x4 mScrgbToBt2100 = float4x4_transpose(float3x3_load(&scrgb_to_bt2100));

    float maxMaxComp = 0;
    double sumOfMaxComp = 0;

    for (uint32_t i = start; i < stop; i++) {
        for (uint32_t j = 0; j < width; j++) {
            float4 v;

            if (bytesPerColor == 4) {
                v = float3_load(reinterpret_cast<float3*>(reinterpret_cast<float*>(pixels) + i * 4 * width + 4 * j));
            } else {
                v = half3_load(reinterpret_cast<half3*>(reinterpret_cast<uint16_t*>(pixels) + i * 4 * width + 4 * j));
            }

            auto bt2020 = float3_transform(v, mScrgbToBt2100);
            bt2020 = float4_saturate(bt2020);

            auto maxComp = float4_hmax(float4_min(bt2020, float4_set(2.f, 2.f, 2.f, 0.f)));

            auto nits = static_cast<uint32_t>(roundf(maxComp * 10000));
            d->nitCounts[nits]++;

            if (maxComp > maxMaxComp) {
                maxMaxComp = maxComp;
            }

            sumOfMaxComp += maxComp;

            auto pixel2020 = float4_to_int4(float4_scale(float4_pq_inv_eotf(bt2020), (1 << INTERMEDIATE_BITS) - 1));
            ushort3_store(reinterpret_cast<ushort3*>(converted + static_cast<size_t>(3) * width * i + static_cast<size_t>(3) * j), pixel2020);
        }
    }

    d->maxNits = static_cast<uint16_t>(roundf(maxMaxComp * 10000));
    d->sumOfMaxComp = sumOfMaxComp;

    return 0;
}

using namespace JxrToAvif;

int main(int argc, char *argv[]) {
    CommandLineParser cmdLineParser(argc, argv);

    if(!cmdLineParser.Parse() || cmdLineParser.GetIsHelpRequired())
    {
        CommandLineParser::PrintUsage();
        return 1;
    }

    auto speed = cmdLineParser.GetSpeed();
    auto inputFile = cmdLineParser.GetInputFile().c_str();
    auto outputFile = cmdLineParser.GetOutputFile().c_str();
    auto useTiling = cmdLineParser.GetIsTilingUsed();
    auto depth = cmdLineParser.GetDepth();
    auto outputFormat = cmdLineParser.GetPixelFormat();
    auto realMaxCLL = cmdLineParser.GetIsRealMaxCLL();

    int hr = init_jxr_loader_thread();

    if(hr < 0)
    {
        auto errorDesc = get_jxr_error_description(hr);
        std::wcerr << L"Failed to initialize JXR loader thread: " << errorDesc << L"\n";
        free_jxr_error_description(errorDesc);
        return hr;
    }

    jxr_data jxr_data;

    hr = get_jxr_data(inputFile, &jxr_data);

    if(hr < 0)
    {
        auto errorDesc = get_jxr_error_description(hr);
        std::wcerr << L"Failed to get image data: " << errorDesc << L"\n";
        free_jxr_error_description(errorDesc);
        return hr;
    }

    auto *converted = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * jxr_data.width * jxr_data.height * 3));

    if (converted == nullptr)
    {
        std::cerr << "Failed to allocate converted pixels\n";
        free_jxr_data(&jxr_data);
        deinit_jxr_loader_thread();
        return 1;
    }

    SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    uint32_t numThreads = systemInfo.dwNumberOfProcessors;

    std::cout << "Using " << numThreads << " threads\n";

    std::cout << "Converting pixels to BT.2100 PQ...\n";

    uint16_t maxCLL, maxPALL;
    {
        uint32_t convThreads = numThreads < 64 ? numThreads : 64;

        uint32_t chunkSize = jxr_data.height / convThreads;

        if (chunkSize == 0) {
            convThreads = jxr_data.height;
            chunkSize = 1;
        }

        HANDLE* hThreadArray = (HANDLE*)alloca(sizeof(HANDLE)*convThreads);
        ThreadData** threadData = (ThreadData**)alloca(sizeof(ThreadData*) * convThreads);
        DWORD* dwThreadIdArray = (DWORD*)alloca(sizeof(DWORD) * convThreads);

        for (uint32_t i = 0; i < convThreads; i++) {
            threadData[i] = (ThreadData*)malloc(sizeof(ThreadData));
            if (threadData[i] == NULL) {
                fprintf(stderr, "Failed to allocate thread data\n");
                return 1;
            }

            threadData[i]->pixels = jxr_data.pixels;
            threadData[i]->bytesPerColor = jxr_data.bytes_per_pixel / 4;
            threadData[i]->converted = converted;
            threadData[i]->width = jxr_data.width;
            threadData[i]->start = i * chunkSize;
            if (i != convThreads - 1) {
                threadData[i]->stop = (i + 1) * chunkSize;
            } else {
                threadData[i]->stop = jxr_data.height;
            }

            threadData[i]->nitCounts = static_cast<uint32_t*>(calloc(10000, sizeof(uint32_t)));

            HANDLE hThread = CreateThread(
                    NULL,                   // default security attributes
                    0,                      // use default stack size
                    ThreadFunc,       // thread function name
                    threadData[i],          // argument to thread function
                    0,                      // use default creation flags
                    &dwThreadIdArray[i]);   // returns the thread identifier

            if (hThread) {
                hThreadArray[i] = hThread;
            } else {
                std::cerr << "Failed to create thread\n";
                return 1;
            }
        }

        WaitForMultipleObjects(convThreads, hThreadArray, TRUE, INFINITE);

        maxCLL = 0;
        double sumOfMaxComp = 0;

        for (uint32_t i = 0; i < convThreads; i++) {
            HANDLE hThread = hThreadArray[i];

            DWORD exitCode;
            if (!GetExitCodeThread(hThread, &exitCode) || exitCode) {
                std::cerr << "Thread failed to terminate properly";
                return 1;
            }
            CloseHandle(hThread);

            uint16_t tMaxNits = threadData[i]->maxNits;
            if (tMaxNits > maxCLL) {
                maxCLL = tMaxNits;
            }

            sumOfMaxComp += threadData[i]->sumOfMaxComp;
        }

        auto pixelCount = static_cast<uint64_t>(jxr_data.width) * jxr_data.height;

        if(!realMaxCLL)
        {
            uint16_t currentIdx = maxCLL;
            uint64_t count = 0;
            uint64_t countTarget = static_cast<uint64_t>(round((1 - MAXCLL_PERCENTILE) * static_cast<double>(pixelCount)));
            while (true) {
                for (uint32_t i = 0; i < convThreads; i++) {
                    count += threadData[i]->nitCounts[currentIdx];
                }
                if (count >= countTarget) {
                    maxCLL = currentIdx;
                    break;
                }
                currentIdx--;
            }
        }

        for (uint32_t i = 0; i < convThreads; i++) {
            free(threadData[i]->nitCounts);
            free(threadData[i]);
        }

        maxPALL = static_cast<uint16_t>(round(10000 * (sumOfMaxComp / static_cast<double>(pixelCount))));
    }

    std::cout << "Computed HDR metadata: " << maxCLL << " MaxCLL, " << maxPALL << " MaxPALL.\n";

    int returnCode = 1;
    avifEncoder *encoder = nullptr;
    avifRWData avifOutput = AVIF_DATA_EMPTY;
    avifRGBImage rgb = {};
    avifPixelFormat targetFormat;
    switch (outputFormat)
    {
    case PixelFormatYuv400:
        targetFormat = AVIF_PIXEL_FORMAT_YUV400;
        break;
    case PixelFormatYuv420:
        targetFormat = AVIF_PIXEL_FORMAT_YUV420;
        break;
    case PixelFormatYuv422:
        targetFormat = AVIF_PIXEL_FORMAT_YUV422;
        break;
    case PixelFormatYuv444:
    case PixelFormatRgb:
        targetFormat = AVIF_PIXEL_FORMAT_YUV444;
        break;
    }

    auto image = avifImageCreate(jxr_data.width, jxr_data.height, depth, targetFormat); // these values dictate what goes into the final AVIF
    if (!image)
    {
        std::cerr << "Out of memory\n";
    }
    else
    {
        // Configure image here: (see avif/avif.h)
        // * colorPrimaries
        // * transferCharacteristics
        // * matrixCoefficients
        // * avifImageSetProfileICC()
        // * avifImageSetMetadataExif()
        // * avifImageSetMetadataXMP()
        // * yuvRange
        // * alphaPremultiplied
        // * transforms (transformFlags, pasp, clap, irot, imir)
        image->colorPrimaries = AVIF_COLOR_PRIMARIES_BT2020;
        image->transferCharacteristics = AVIF_TRANSFER_CHARACTERISTICS_SMPTE2084;

        if (outputFormat == PixelFormatRgb)
        {
            image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
        }
        else
        {
            image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
        }

        image->clli.maxCLL = maxCLL;
        image->clli.maxPALL = maxPALL;

        // If you have RGB(A) data you want to encode, use this path
        std::cout << "Doing AVIF encoding...\n";

        avifRGBImageSetDefaults(&rgb, image);
        // Override RGB(A)->YUV(A) defaults here:
        //   depth, format, chromaDownsampling, avoidLibYUV, ignoreAlpha, alphaPremultiplied, etc.
        rgb.format = AVIF_RGB_FORMAT_RGB;
        rgb.depth = INTERMEDIATE_BITS;
        rgb.pixels = reinterpret_cast<uint8_t*>(converted);
        rgb.rowBytes = 3 * sizeof(uint16_t) * jxr_data.width;

        auto convertResult = avifImageRGBToYUV(image, &rgb);
        if (convertResult != AVIF_RESULT_OK)
        {
            std::cerr << "Failed to convert to YUV(A): " << avifResultToString(convertResult) << "\n";
        }
        else {

            free(rgb.pixels);

            encoder = avifEncoderCreate();
            if (!encoder)
            {
                std::cerr << "Out of memory\n";
            }
            else
            {
                // Configure your encoder here (see avif/avif.h):
                // * maxThreads
                // * quality
                // * qualityAlpha
                // * tileRowsLog2
                // * tileColsLog2
                // * speed
                // * keyframeInterval
                // * timescale
                encoder->quality = AVIF_QUALITY_LOSSLESS;
                encoder->qualityAlpha = AVIF_QUALITY_LOSSLESS;
                encoder->speed = speed;
                encoder->maxThreads = (int)numThreads;
                encoder->autoTiling = useTiling ? AVIF_TRUE : AVIF_FALSE;

                // Call avifEncoderAddImage() for each image in your sequence
                // Only set AVIF_ADD_IMAGE_FLAG_SINGLE if you're not encoding a sequence
                // Use avifEncoderAddImageGrid() instead with an array of avifImage* to make a grid image
                auto addImageResult = avifEncoderAddImage(encoder, image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
                if (addImageResult != AVIF_RESULT_OK)
                {
                    std::cerr << "Failed to add image to encoder: " << avifResultToString(addImageResult) << "\n";
                }
                else
                {
                    auto finishResult = avifEncoderFinish(encoder, &avifOutput);
                    if (finishResult != AVIF_RESULT_OK)
                    {
                        std::cerr << "Failed to finish encoding: " << avifResultToString(finishResult) << "\n";
                    }
                    else
                    {
                        std::cout << "Encode success: " << avifOutput.size << " total bytes\n";

                        FILE* f = nullptr;
                        _wfopen_s(&f, outputFile, L"wb");
                        auto bytesWritten = fwrite(avifOutput.data, 1, avifOutput.size, f);
                        fclose(f);
                        if (bytesWritten != avifOutput.size)
                        {
                            std::cerr << "Failed to write " << avifOutput.size << " bytes\n";
                        }
                        else
                        {
                            printf("Wrote: %ls\n", outputFile);

                            returnCode = 0;
                        }
                    }
                }
            }
        }
    }
    if (image) {
        avifImageDestroy(image);
    }
    if (encoder) {
        avifEncoderDestroy(encoder);
    }
    avifRWDataFree(&avifOutput);
    free_jxr_data(&jxr_data);
    deinit_jxr_loader_thread();
    return returnCode;
}

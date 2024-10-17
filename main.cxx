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
#include "jxr_data.h"

constexpr auto INTERMEDIATE_BITS = 16;  // bit depth of the integer texture given to the encoder;
constexpr auto DEFAULT_SPEED = 6;  // 6 is default speed of the command line encoder, so it should be a good value?;
#define USE_TILING AVIF_TRUE  // slightly larger file size, but faster encode and decode

constexpr auto TARGET_BITS = 12;  // bit depth of the output, should be 10 or 12;
#define TARGET_FORMAT AVIF_PIXEL_FORMAT_YUV420
//#define TARGET_RGB  // uncomment to output RGB instead of YUV (much larger file size)

#define MAXCLL_PERCENTILE 0.9999  // comment out to calculate true MaxCLL instead of top percentile

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
#ifdef MAXCLL_PERCENTILE
    uint32_t *nitCounts;
#endif
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

            float maxComp = float4_hmax(float4_min(bt2020, float4_set(2.f, 2.f, 2.f, 0.f)));

#ifdef MAXCLL_PERCENTILE
            auto nits = static_cast<uint32_t>(roundf(maxComp * 10000));
            d->nitCounts[nits]++;
#endif
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

int main(int argc, char *argv[]) {
    if ((argc <= 1) || argc > 3 == strcmp(argv[1], "--speed") || argc > 5) {
        std::cerr << "jxr_to_avid [--speed n] input.jxr [output.avif]\n";
        return 1;
    }

    int speed = DEFAULT_SPEED;
    LPWSTR inputFile;
    LPCWSTR outputFile = L"output.avif";

    {
        LPWSTR *szArglist;
        int nArgs;

        szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (nullptr == szArglist) {
            std::cerr << "CommandLineToArgvW failed\n";
            return 1;
        }

        int rest = 1;

        if (!strcmp("--speed", argv[1])) {
            speed = atoi(argv[2]);
            if (speed < AVIF_SPEED_SLOWEST || speed > AVIF_SPEED_FASTEST) {
                std::cerr << "Speed must be in range of [" << AVIF_SPEED_SLOWEST << ", " << AVIF_SPEED_FASTEST << "]\n";
                return 1;
            }
            rest += 2;
        }

        inputFile = szArglist[rest + 0];

        if (rest + 1 < argc) {
            outputFile = szArglist[rest + 1];
        }
    }

    // Initialize OLE
    OleInitialize(NULL);

    jxr_data jxr_data;

    int hr = get_jxr_data(inputFile, &jxr_data);

    if(hr < 0)
    {
        const wchar_t* errorDesc = get_jxr_error_description(hr);
        std::wcerr << L"Failed to get image data: " << errorDesc << L"\n";
        return hr;
    }

    auto *converted = static_cast<uint16_t*>(malloc(sizeof(uint16_t) * jxr_data.width * jxr_data.height * 3));

    if (converted == nullptr)
    {
        std::cerr << "Failed to allocate converted pixels\n";
        free_jxr_data(&jxr_data);
        OleUninitialize();
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

#ifdef MAXCLL_PERCENTILE
            threadData[i]->nitCounts = static_cast<uint32_t*>(calloc(10000, sizeof(uint32_t)));
#endif

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

#ifdef MAXCLL_PERCENTILE
        uint16_t currentIdx = maxCLL;
        uint64_t count = 0;
        uint64_t countTarget = (uint64_t) round((1 - MAXCLL_PERCENTILE) * static_cast<double>(static_cast<uint64_t>(jxr_data.width) * jxr_data.height));
        while (1) {
            for (uint32_t i = 0; i < convThreads; i++) {
                count += threadData[i]->nitCounts[currentIdx];
            }
            if (count >= countTarget) {
                maxCLL = currentIdx;
                break;
            }
            currentIdx--;
        }
#endif

        for (uint32_t i = 0; i < convThreads; i++) {
#ifdef MAXCLL_PERCENTILE
            free(threadData[i]->nitCounts);
#endif
            free(threadData[i]);
        }

        maxPALL = static_cast<uint16_t>(round(10000 * (sumOfMaxComp / static_cast<double>(static_cast<uint64_t>(jxr_data.width) * jxr_data.height))));
    }

    int returnCode = 1;
    avifEncoder *encoder = nullptr;
    avifRWData avifOutput = AVIF_DATA_EMPTY;
    avifRGBImage rgb = {};

    avifImage *image = avifImageCreate(jxr_data.width, jxr_data.height, TARGET_BITS,
                                       TARGET_FORMAT); // these values dictate what goes into the final AVIF
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

#ifdef TARGET_RGB
        image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
#else
        image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
#endif

        printf("Computed HDR metadata: %u MaxCLL, %u MaxPALL\n", maxCLL, maxPALL);

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

        avifResult convertResult = avifImageRGBToYUV(image, &rgb);
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
                encoder->autoTiling = USE_TILING;

                // Call avifEncoderAddImage() for each image in your sequence
                // Only set AVIF_ADD_IMAGE_FLAG_SINGLE if you're not encoding a sequence
                // Use avifEncoderAddImageGrid() instead with an array of avifImage* to make a grid image
                avifResult addImageResult = avifEncoderAddImage(encoder, image, 1, AVIF_ADD_IMAGE_FLAG_SINGLE);
                if (addImageResult != AVIF_RESULT_OK)
                {
                    std::cerr << "Failed to add image to encoder: " << avifResultToString(addImageResult) << "\n";
                }
                else
                {
                    avifResult finishResult = avifEncoderFinish(encoder, &avifOutput);
                    if (finishResult != AVIF_RESULT_OK)
                    {
                        std::cerr << "Failed to finish encoding: " << avifResultToString(finishResult) << "\n";
                    }
                    else
                    {
                        std::cout << "Encode success: " << avifOutput.size << " total bytes\n";

                        FILE* f = nullptr;
                        _wfopen_s(&f, outputFile, L"wb");
                        size_t bytesWritten = fwrite(avifOutput.data, 1, avifOutput.size, f);
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
    OleUninitialize();
    return returnCode;
}

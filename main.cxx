// adapted from avif-example-encode.c, see libavif license in LICENSE-THIRD-PARTY
// original copyright notice follows

// Copyright 2020 Joe Drago, 2024 Dmitry Ignatiev. All rights reserved

#include <cmath>
#include <iostream>

#include <avif/avif.h>

#include "CommandLineParser.hpp"
#include "JxrImage.hpp"
#include "jxr_sys_helpers.h"

constexpr auto INTERMEDIATE_BITS = 16;  // bit depth of the integer texture given to the encoder;

using namespace JxrToAvif;

int main(int argc, char *argv[])
{
    try
    {
        CommandLineParser cmdLineParser(argc, argv);

        if (!cmdLineParser.Parse() || cmdLineParser.GetIsHelpRequired())
        {
            CommandLineParser::PrintUsage();
            return 1;
        }

        const auto speed = cmdLineParser.GetSpeed();
        const auto inputFile = cmdLineParser.GetInputFile();
        const auto outputFile = cmdLineParser.GetOutputFile().c_str();
        const auto useTiling = cmdLineParser.GetIsTilingUsed();
        const auto depth = cmdLineParser.GetDepth();
        const auto outputFormat = cmdLineParser.GetPixelFormat();
        const auto realMaxCLL = cmdLineParser.GetIsRealMaxCLL();

        const JxrImage jxrImage(inputFile, realMaxCLL);        

        int returnCode = 1;
        avifEncoder* encoder = nullptr;
        avifRWData avifOutput = AVIF_DATA_EMPTY;
        avifRGBImage rgb = {};
        avifPixelFormat targetFormat = AVIF_PIXEL_FORMAT_YUV444;
        switch (outputFormat)
        {
        case PixelFormat::Yuv400:
            targetFormat = AVIF_PIXEL_FORMAT_YUV400;
            break;
        case PixelFormat::Yuv420:
            targetFormat = AVIF_PIXEL_FORMAT_YUV420;
            break;
        case PixelFormat::Yuv422:
            targetFormat = AVIF_PIXEL_FORMAT_YUV422;
            break;
        case PixelFormat::Yuv444:
        case PixelFormat::Rgb:
            targetFormat = AVIF_PIXEL_FORMAT_YUV444;
            break;
        }

        const auto image = avifImageCreate(jxrImage.GetWidth(), jxrImage.GetHeight(), depth, targetFormat); // these values dictate what goes into the final AVIF
        if (!image)
        {
            throw std::bad_alloc();
        }
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

        if (outputFormat == PixelFormat::Rgb)
        {
            image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_IDENTITY;
        }
        else
        {
            image->matrixCoefficients = AVIF_MATRIX_COEFFICIENTS_BT2020_NCL;
        }

        image->clli.maxCLL = jxrImage.GetMaxCLL();
        image->clli.maxPALL = jxrImage.GetMaxPALL();

        // If you have RGB(A) data you want to encode, use this path
        std::cout << "Doing AVIF encoding...\n" << std::flush;

        avifRGBImageSetDefaults(&rgb, image);
        // Override RGB(A)->YUV(A) defaults here:
        //   depth, format, chromaDownsampling, avoidLibYUV, ignoreAlpha, alphaPremultiplied, etc.
        rgb.format = AVIF_RGB_FORMAT_RGB;
        rgb.depth = INTERMEDIATE_BITS;
        rgb.pixels = reinterpret_cast<uint8_t*>(jxrImage.GetDataPointer());
        rgb.rowBytes = sizeof(ushort3) * jxrImage.GetWidth();

        auto convertResult = avifImageRGBToYUV(image, &rgb);
        if (convertResult != AVIF_RESULT_OK)
        {
            std::cerr << "Failed to convert to YUV(A): " << avifResultToString(convertResult) << "\n";
        }
        else
        {
            encoder = avifEncoderCreate();
            if (!encoder)
            {
                throw std::bad_alloc();
            }
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
            encoder->maxThreads = static_cast<int>(jxr_get_number_of_processors());
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

                    auto rv = jxr_write_data_to_file(outputFile, avifOutput.data, avifOutput.size);
                    if (rv < 0)
                    {
                        auto writeErrorDesc = jxr_get_error_description(rv);
                        std::cerr << "Failed to write " << avifOutput.size << " bytes: " << writeErrorDesc << "\n";
                        jxr_free_error_description(writeErrorDesc);
                        returnCode = rv;
                    }
                    else
                    {
                        std::wcout << L"Wrote: " << outputFile << L"\n";
                        returnCode = 0;
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
        return returnCode;
    }
    catch (std::bad_alloc&)
    {
        std::cerr << "Out of memory\n";
        return 1;
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

// Copyright 2024 Dmitry Ignatiev. All rights reserved

#include <cwctype>
#include <iostream>
#include <algorithm>
#include <exception>
#include "CommandLineParser.hpp"

namespace JxrToAvif
{
    CommandLineParser::CommandLineParser([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
        : _cmdline{}, _speed(DefaultSpeed),
        _helpRequired(false), _useTiling(true), _realMaxCLL(false),
        _format(PixelFormat::Yuv444), _depth(12), _outputFile(DefaultOutputFile)
    {
        const auto rv = jxr_get_command_line(argc, argv, &_cmdline);
        if(rv < 0)
        {
            throw std::runtime_error("Failed to retrieve process command line.");
        }
    }

    CommandLineParser::~CommandLineParser()
    {
        jxr_free_command_line(&_cmdline);
    }

    bool CommandLineParser::Parse()
    {
        int i = 1;
        bool hasInputFile = false, hasOutputFile = false;

        if (_cmdline.argc < 1)
            return false;

        while (i < _cmdline.argc)
        {
            auto arg = std::wstring(_cmdline.argv[i]);
            if (arg == L"--help")
            {
                _helpRequired = true;
            }
            else if (arg == L"--speed")
            {
                ++i;
                if (i >= _cmdline.argc)
                {
                    return false;
                }
                arg = std::wstring(_cmdline.argv[i]);
                try
                {
                    const auto n = std::stoi(arg);
                    if (n < 0 || n > 10)
                        return false;
                    _speed = n;
                }
                catch (std::exception&)
                {
                    return false;
                }
            }
            else if(arg == L"--depth")
            {
                ++i;
                if(i >= _cmdline.argc)
                {
                    return false;
                }
                arg = std::wstring(_cmdline.argv[i]);
                try
                {
                    const auto n = std::stoi(arg);
                    if (n != 10 && n != 12)
                        return false;
                    _depth = static_cast<uint8_t>(n);
                }
                catch (std::exception&)
                {
                    return false;
                }
            }
            else if (arg == L"--format")
            {
                ++i;
                if(i >= _cmdline.argc)
                {
                    return false;
                }
                arg = std::wstring(_cmdline.argv[i]);
                std::transform(arg.begin(), arg.end(), arg.begin(), std::towlower);
                if(arg == L"rgb")
                {
                    _format = PixelFormat::Rgb;
                }
                else if(arg == L"yuv444")
                {
                    _format = PixelFormat::Yuv444;
                }
                else if(arg == L"yuv422")
                {
                    _format = PixelFormat::Yuv422;
                }
                else if(arg == L"yuv420")
                {
                    _format = PixelFormat::Yuv420;
                }
                else if(arg == L"yuv400")
                {
                    _format = PixelFormat::Yuv400;
                }
                else
                {
                    return false;
                }
            }
            else if(arg == L"--without-tiling")
            {
                _useTiling = false;
            }
            else if(arg == L"--real-maxcll")
            {
                _realMaxCLL = true;
            }
            else if (hasOutputFile)
            {
                return false;
            }
            else if (hasInputFile)
            {
                hasOutputFile = true;
                _outputFile = arg;
            }
            else
            {
                hasInputFile = true;
                _inputFile = arg;
            }
            ++i;
        }

        return hasInputFile;
    }

    void CommandLineParser::PrintUsage()
    {
        std::cout << "Usage: jxr_to_avif [options] input.jxr [output.avif]\n";
        std::cout << "Options:\n";
        std::cout << "  --help              Print this message.\n";
        std::cout << "  --speed <n>         AVIF encoding speed.\n";
        std::cout << "                      Must be in range of 0 to 10. Defaults to 6.\n";
        std::cout << "  --without-tiling    Do not use tiling.\n";
        std::cout << "                      Tiling means slightly larger file size\n";
        std::cout << "                      but faster encoding and decoding.\n";
        std::cout << "  --depth <n>         Output color depth. May equal 10 or 12.\n";
        std::cout << "                      Defaults to 12 bits.\n";
        std::cout << "  --format            Output pixel format. Defaults to yuv444.\n";
        std::cout << "                      Must be one of:\n";
        std::cout << "                        rgb, yuv444, yuv422, yuv420, yuv400\n";
        std::cout << "  --real-maxcll      Calculate real MaxCLL\n";
        std::cout << "                     instead of top percentile.\n";
    }
}
